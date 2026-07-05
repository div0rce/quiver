// K1 compare — AVX2 backend (PRD 08 K1; Survey §4.1). Technique: vector compares packed to
// predicate bits via movemask idioms per lane width (8-bit: movemask_epi8; 16-bit: BMI2 PEXT
// of the per-lane high-byte bits; 32-bit: movemask_ps on the cast mask; 64-bit: movemask_pd
// nibbles from two vectors). Unsigned integers compare via sign-bias XOR then signed compare;
// ne/le/ge derive by bit inversion AFTER packing (exact for total-ordered integers); floats
// use _mm256_cmp_ps/pd ordered-quiet predicates directly (NEQ_UQ gives NaN-true for kNe, all
// others NaN-false — matching C++ operator semantics), between = GE(lo) AND LE(hi) lane masks.
// Validity is ANDed at byte granularity. Selvec forms feed predicate bytes into the
// kCompactLut32 index-store core (as K3); scratch stays inside the n-element capacity region
// (REQ-MEM-008). Tails are scalar and byte-assembled exactly like the reference (ADR-015).
// Bit-identical to compare_scalar_impl.h (REQ-KERNEL-002).
// Module: MOD-K1-COMPARE | REQs: REQ-K1-001..003, REQ-SIMD-001..003/-005 | ADR-003, ADR-016
#include "src/kernels/common/luts.h"
#include "src/kernels/common/target_regions.h"
#include "src/kernels/compare/compare_scalar_impl.h"

#if defined(__x86_64__) || defined(_M_X64)

#include <cstring>
#include <immintrin.h>
#include <type_traits>

QUIVER_BEGIN_NAMESPACE
QUIVER_TARGET_AVX2_BEGIN
namespace detail::avx2 {

namespace {

// --- Vector loads/broadcasts (type-dispatched; integer forms work on the byte image) --------

template <class T>
QUIVER_FORCE_INLINE auto cmp_load_vec(const T* p) noexcept {
  if constexpr (std::is_same_v<T, float>) {
    return _mm256_loadu_ps(p);
  } else if constexpr (std::is_same_v<T, double>) {
    return _mm256_loadu_pd(p);
  } else {
    return _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p));
  }
}

template <class T>
QUIVER_FORCE_INLINE auto cmp_broadcast(T value) noexcept {
  if constexpr (std::is_same_v<T, float>) {
    return _mm256_set1_ps(value);
  } else if constexpr (std::is_same_v<T, double>) {
    return _mm256_set1_pd(value);
  } else if constexpr (sizeof(T) == 1) {
    return _mm256_set1_epi8(static_cast<char>(value));
  } else if constexpr (sizeof(T) == 2) {
    return _mm256_set1_epi16(static_cast<short>(value));
  } else if constexpr (sizeof(T) == 4) {
    return _mm256_set1_epi32(static_cast<int>(value));
  } else {
    return _mm256_set1_epi64x(static_cast<long long>(value));
  }
}

// --- Integer relation machinery: {eq, gt} primitives; lt via operand swap; ne/le/ge via
// --- post-pack bit inversion. Unsigned compares bias both operands into signed order.

template <class T>
QUIVER_FORCE_INLINE __m256i bias_unsigned(__m256i v) noexcept {
  if constexpr (sizeof(T) == 1) {
    return _mm256_xor_si256(v, _mm256_set1_epi8(static_cast<char>(0x80)));
  } else if constexpr (sizeof(T) == 2) {
    return _mm256_xor_si256(v, _mm256_set1_epi16(static_cast<short>(0x8000)));
  } else if constexpr (sizeof(T) == 4) {
    return _mm256_xor_si256(v, _mm256_set1_epi32(static_cast<int>(0x80000000u)));
  } else {
    return _mm256_xor_si256(v, _mm256_set1_epi64x(static_cast<long long>(0x8000000000000000ull)));
  }
}

template <class T>
QUIVER_FORCE_INLINE __m256i int_eq(__m256i a, __m256i b) noexcept {
  if constexpr (sizeof(T) == 1) {
    return _mm256_cmpeq_epi8(a, b);
  } else if constexpr (sizeof(T) == 2) {
    return _mm256_cmpeq_epi16(a, b);
  } else if constexpr (sizeof(T) == 4) {
    return _mm256_cmpeq_epi32(a, b);
  } else {
    return _mm256_cmpeq_epi64(a, b);
  }
}

template <class T>
QUIVER_FORCE_INLINE __m256i int_gt(__m256i a, __m256i b) noexcept {
  if constexpr (std::is_unsigned_v<T>) {
    a = bias_unsigned<T>(a);
    b = bias_unsigned<T>(b);
  }
  if constexpr (sizeof(T) == 1) {
    return _mm256_cmpgt_epi8(a, b);
  } else if constexpr (sizeof(T) == 2) {
    return _mm256_cmpgt_epi16(a, b);
  } else if constexpr (sizeof(T) == 4) {
    return _mm256_cmpgt_epi32(a, b);
  } else {
    return _mm256_cmpgt_epi64(a, b);
  }
}

template <class T>
QUIVER_FORCE_INLINE __m256i int_mask(CompareOp op, __m256i a, __m256i b) noexcept {
  switch (op) {
  case CompareOp::kEq:
  case CompareOp::kNe:
    return int_eq<T>(a, b);
  case CompareOp::kGt:
  case CompareOp::kLe:
    return int_gt<T>(a, b);
  case CompareOp::kLt:
  case CompareOp::kGe:
    return int_gt<T>(b, a);
  }
  return _mm256_setzero_si256();  // unreachable for in-contract op values
}

QUIVER_FORCE_INLINE bool int_invert(CompareOp op) noexcept {
  return op == CompareOp::kNe || op == CompareOp::kLe || op == CompareOp::kGe;
}

// Float lane masks: ordered-quiet predicates match C++ operator truth tables exactly
// (NaN lanes false except NEQ_UQ true; -0.0 == +0.0). imm8 must be constant -> switch.
QUIVER_FORCE_INLINE __m256 f32_mask(CompareOp op, __m256 a, __m256 b) noexcept {
  switch (op) {
  case CompareOp::kEq:
    return _mm256_cmp_ps(a, b, _CMP_EQ_OQ);
  case CompareOp::kNe:
    return _mm256_cmp_ps(a, b, _CMP_NEQ_UQ);
  case CompareOp::kLt:
    return _mm256_cmp_ps(a, b, _CMP_LT_OQ);
  case CompareOp::kLe:
    return _mm256_cmp_ps(a, b, _CMP_LE_OQ);
  case CompareOp::kGt:
    return _mm256_cmp_ps(a, b, _CMP_GT_OQ);
  case CompareOp::kGe:
    return _mm256_cmp_ps(a, b, _CMP_GE_OQ);
  }
  return _mm256_setzero_ps();  // unreachable for in-contract op values
}

QUIVER_FORCE_INLINE __m256d f64_mask(CompareOp op, __m256d a, __m256d b) noexcept {
  switch (op) {
  case CompareOp::kEq:
    return _mm256_cmp_pd(a, b, _CMP_EQ_OQ);
  case CompareOp::kNe:
    return _mm256_cmp_pd(a, b, _CMP_NEQ_UQ);
  case CompareOp::kLt:
    return _mm256_cmp_pd(a, b, _CMP_LT_OQ);
  case CompareOp::kLe:
    return _mm256_cmp_pd(a, b, _CMP_LE_OQ);
  case CompareOp::kGt:
    return _mm256_cmp_pd(a, b, _CMP_GT_OQ);
  case CompareOp::kGe:
    return _mm256_cmp_pd(a, b, _CMP_GE_OQ);
  }
  return _mm256_setzero_pd();  // unreachable for in-contract op values
}

template <class T>
QUIVER_FORCE_INLINE auto typed_mask(CompareOp op, decltype(cmp_load_vec<T>(nullptr)) a,
                                    decltype(cmp_load_vec<T>(nullptr)) b) noexcept {
  if constexpr (std::is_same_v<T, float>) {
    return f32_mask(op, a, b);
  } else if constexpr (std::is_same_v<T, double>) {
    return f64_mask(op, a, b);
  } else {
    return int_mask<T>(op, a, b);
  }
}

// --- Vector predicates: mask(i) yields the lane mask for one vector at element i; inv()
// --- says whether packed bits must be complemented; one(i) is the exact scalar predicate
// --- for tails (bit-identity with the reference, ADR-015).

template <class T>
struct CmpRhs {  // in[i] <op> comparand
  CompareOp op;
  const T* in;
  T comparand;
  decltype(cmp_broadcast(T{})) bvec;
  QUIVER_FORCE_INLINE auto mask(std::int64_t i) const noexcept {
    return typed_mask<T>(op, cmp_load_vec(in + i), bvec);
  }
  QUIVER_FORCE_INLINE bool inv() const noexcept { return std::is_integral_v<T> && int_invert(op); }
  QUIVER_FORCE_INLINE bool one(std::int64_t i) const noexcept {
    return scalar_impl::compare_one(op, in[i], comparand);
  }
};

template <class T>
struct CmpBatch {  // a[i] <op> b[i]
  CompareOp op;
  const T* a;
  const T* b;
  QUIVER_FORCE_INLINE auto mask(std::int64_t i) const noexcept {
    return typed_mask<T>(op, cmp_load_vec(a + i), cmp_load_vec(b + i));
  }
  QUIVER_FORCE_INLINE bool inv() const noexcept { return std::is_integral_v<T> && int_invert(op); }
  QUIVER_FORCE_INLINE bool one(std::int64_t i) const noexcept {
    return scalar_impl::compare_one(op, a[i], b[i]);
  }
};

template <class T>
struct CmpBetween {  // lo <= in[i] && in[i] <= hi (inclusive; NaN excluded by ordered cmp)
  const T* in;
  T lo;
  T hi;
  decltype(cmp_broadcast(T{})) lo_vec;
  decltype(cmp_broadcast(T{})) hi_vec;
  QUIVER_FORCE_INLINE auto mask(std::int64_t i) const noexcept {
    if constexpr (std::is_same_v<T, float>) {
      const __m256 a = _mm256_loadu_ps(in + i);
      return _mm256_and_ps(_mm256_cmp_ps(a, lo_vec, _CMP_GE_OQ),
                           _mm256_cmp_ps(a, hi_vec, _CMP_LE_OQ));
    } else if constexpr (std::is_same_v<T, double>) {
      const __m256d a = _mm256_loadu_pd(in + i);
      return _mm256_and_pd(_mm256_cmp_pd(a, lo_vec, _CMP_GE_OQ),
                           _mm256_cmp_pd(a, hi_vec, _CMP_LE_OQ));
    } else {
      // in-range = !(lo > a || a > hi): OR the two exclusions, complement after packing.
      const __m256i a = cmp_load_vec(in + i);
      return _mm256_or_si256(int_gt<T>(lo_vec, a), int_gt<T>(a, hi_vec));
    }
  }
  QUIVER_FORCE_INLINE bool inv() const noexcept { return std::is_integral_v<T>; }
  QUIVER_FORCE_INLINE bool one(std::int64_t i) const noexcept {
    return (lo <= in[i]) && (in[i] <= hi);
  }
};

// --- Validity fetchers: bytes() returns the validity bits for one group (memcpy leaves the
// --- unused high bytes zero, which is harmless because predicate bits are width-masked);
// --- one(i) is the scalar check for tails. nullptr means all-valid (REQ-API-008).

struct OneValidity {
  const std::uint8_t* v;
  QUIVER_FORCE_INLINE std::uint32_t bytes(std::int64_t byte_idx, int nbytes) const noexcept {
    if (v == nullptr) {
      return ~0u;
    }
    std::uint32_t x = 0;
    std::memcpy(&x, v + byte_idx, static_cast<std::size_t>(nbytes));
    return x;
  }
  QUIVER_FORCE_INLINE bool one(std::int64_t i) const noexcept { return is_valid(v, i); }
};

struct TwoValidity {
  const std::uint8_t* a;
  const std::uint8_t* b;
  QUIVER_FORCE_INLINE std::uint32_t bytes(std::int64_t byte_idx, int nbytes) const noexcept {
    return OneValidity{a}.bytes(byte_idx, nbytes) & OneValidity{b}.bytes(byte_idx, nbytes);
  }
  QUIVER_FORCE_INLINE bool one(std::int64_t i) const noexcept {
    return is_valid(a, i) && is_valid(b, i);
  }
};

// Group geometry: elements per packed-bit group (one vector, except 64-bit lanes use two).
template <class T>
constexpr std::int64_t group_lanes() noexcept {
  return sizeof(T) == 1 ? 32 : sizeof(T) == 2 ? 16 : 8;
}

// Packed predicate bits for the group starting at element i (inversion applied, width-masked).
template <class T, class Pred>
QUIVER_FORCE_INLINE std::uint32_t group_bits(const Pred& pred, std::int64_t i) noexcept {
  std::uint32_t bits = 0;
  if constexpr (std::is_same_v<T, float>) {
    bits = static_cast<std::uint32_t>(_mm256_movemask_ps(pred.mask(i)));
  } else if constexpr (std::is_same_v<T, double>) {
    const auto lo = static_cast<std::uint32_t>(_mm256_movemask_pd(pred.mask(i)));
    const auto hi = static_cast<std::uint32_t>(_mm256_movemask_pd(pred.mask(i + 4)));
    bits = lo | (hi << 4);
  } else if constexpr (sizeof(T) == 1) {
    bits = static_cast<std::uint32_t>(_mm256_movemask_epi8(pred.mask(i)));
  } else if constexpr (sizeof(T) == 2) {
    // movemask_epi8 yields two identical bits per 16-bit lane; PEXT keeps one per lane.
    bits = _pext_u32(static_cast<std::uint32_t>(_mm256_movemask_epi8(pred.mask(i))), 0xAAAAAAAAu);
  } else if constexpr (sizeof(T) == 4) {
    bits = static_cast<std::uint32_t>(_mm256_movemask_ps(_mm256_castsi256_ps(pred.mask(i))));
  } else {
    const auto lo =
        static_cast<std::uint32_t>(_mm256_movemask_pd(_mm256_castsi256_pd(pred.mask(i))));
    const auto hi =
        static_cast<std::uint32_t>(_mm256_movemask_pd(_mm256_castsi256_pd(pred.mask(i + 4))));
    bits = lo | (hi << 4);
  }
  if (pred.inv()) {
    bits = ~bits;  // complement first, then width-mask (upper garbage bits flip too)
  }
  constexpr std::int64_t kGroup = group_lanes<T>();
  if constexpr (kGroup < 32) {
    bits &= (1u << kGroup) - 1u;
  }
  return bits;
}

// Bitmap emission core: vector groups, byte-granular validity AND, scalar byte-assembled tail.
template <class T, class Pred, class Validity>
std::int64_t emit_bitmap_avx2(std::int64_t n, const Pred& pred, const Validity& val,
                              std::uint8_t* out) noexcept {
  constexpr std::int64_t kGroup = group_lanes<T>();
  constexpr int kBytes = static_cast<int>(kGroup / 8);
  std::int64_t count = 0;
  std::int64_t i = 0;
  for (; i + kGroup <= n; i += kGroup) {
    const std::uint32_t bits = group_bits<T>(pred, i) & val.bytes(i >> 3, kBytes);
    std::memcpy(out + (i >> 3), &bits, kBytes);
    count += std::popcount(bits);
  }
  if (i < n) {  // scalar tail: byte assembly identical to the reference (ADR-015/ADR-016)
    std::int64_t byte_idx = i >> 3;
    std::uint8_t byte = 0;
    int k = 0;
    for (; i < n; ++i) {
      const bool p = val.one(i) && pred.one(i);
      byte = static_cast<std::uint8_t>(byte | (static_cast<std::uint8_t>(p) << k));
      if (++k == 8) {
        out[byte_idx++] = byte;
        count += std::popcount(byte);
        byte = 0;
        k = 0;
      }
    }
    if (k != 0) {
      out[byte_idx] = byte;  // bits >= tail are zero by construction (ADR-016)
      count += std::popcount(byte);
    }
  }
  return count;
}

// Selvec emission core: predicate bytes drive kCompactLut32 index stores (the K3 core).
// A store spans [count, count+8) with count <= the group's start element, so full-vector
// stores stay inside the n-element capacity region (REQ-MEM-008); output is strictly
// increasing by construction.
template <class T, class Pred, class Validity>
std::int64_t emit_selvec_avx2(std::int64_t n, const Pred& pred, const Validity& val,
                              std::uint32_t* out) noexcept {
  constexpr std::int64_t kGroup = group_lanes<T>();
  constexpr int kBytes = static_cast<int>(kGroup / 8);
  std::int64_t count = 0;
  std::int64_t i = 0;
  for (; i + kGroup <= n; i += kGroup) {
    const std::uint32_t bits = group_bits<T>(pred, i) & val.bytes(i >> 3, kBytes);
    for (int byte_k = 0; byte_k < kBytes; ++byte_k) {
      const auto byte = static_cast<std::uint8_t>((bits >> (8 * byte_k)) & 0xFFu);
      const std::int64_t base_idx = i + std::int64_t{8} * byte_k;
      const __m256i lanes =
          _mm256_loadu_si256(reinterpret_cast<const __m256i*>(kCompactLut32.perm[byte]));
      const __m256i base = _mm256_set1_epi32(static_cast<int>(base_idx));
      _mm256_storeu_si256(reinterpret_cast<__m256i*>(out + count), _mm256_add_epi32(lanes, base));
      count += kPopcountLut.count[byte];
    }
  }
  for (; i < n; ++i) {  // scalar tail identical to the reference (ADR-015)
    out[count] = static_cast<std::uint32_t>(i);
    count += (val.one(i) && pred.one(i)) ? 1 : 0;
  }
  return count;
}

}  // namespace

// --- Concrete overloads (mirroring the scalar backend set; ADR-006) --------------------------

// NOLINTBEGIN(bugprone-macro-parentheses): T expands to type names inside declarators.
#define QUIVER_K1_DEFINE(T)                                                                        \
  std::int64_t k1_compare_bitmap(CompareOp op, const T* in, std::int64_t n, T comparand,           \
                                 const std::uint8_t* validity, std::uint8_t* out) noexcept {       \
    return emit_bitmap_avx2<T>(n, CmpRhs<T>{op, in, comparand, cmp_broadcast(comparand)},          \
                               OneValidity{validity}, out);                                        \
  }                                                                                                \
  std::int64_t k1_compare_bitmap2(CompareOp op, const T* a, const T* b, std::int64_t n,            \
                                  const std::uint8_t* a_validity, const std::uint8_t* b_validity,  \
                                  std::uint8_t* out) noexcept {                                    \
    return emit_bitmap_avx2<T>(n, CmpBatch<T>{op, a, b}, TwoValidity{a_validity, b_validity},      \
                               out);                                                               \
  }                                                                                                \
  std::int64_t k1_compare_between_bitmap(const T* in, std::int64_t n, T lo, T hi,                  \
                                         const std::uint8_t* validity,                             \
                                         std::uint8_t* out) noexcept {                             \
    return emit_bitmap_avx2<T>(n, CmpBetween<T>{in, lo, hi, cmp_broadcast(lo), cmp_broadcast(hi)}, \
                               OneValidity{validity}, out);                                        \
  }                                                                                                \
  std::int64_t k1_compare_selvec(CompareOp op, const T* in, std::int64_t n, T comparand,           \
                                 const std::uint8_t* validity, std::uint32_t* out) noexcept {      \
    return emit_selvec_avx2<T>(n, CmpRhs<T>{op, in, comparand, cmp_broadcast(comparand)},          \
                               OneValidity{validity}, out);                                        \
  }                                                                                                \
  std::int64_t k1_compare_selvec2(CompareOp op, const T* a, const T* b, std::int64_t n,            \
                                  const std::uint8_t* a_validity, const std::uint8_t* b_validity,  \
                                  std::uint32_t* out) noexcept {                                   \
    return emit_selvec_avx2<T>(n, CmpBatch<T>{op, a, b}, TwoValidity{a_validity, b_validity},      \
                               out);                                                               \
  }                                                                                                \
  std::int64_t k1_compare_between_selvec(const T* in, std::int64_t n, T lo, T hi,                  \
                                         const std::uint8_t* validity,                             \
                                         std::uint32_t* out) noexcept {                            \
    return emit_selvec_avx2<T>(n, CmpBetween<T>{in, lo, hi, cmp_broadcast(lo), cmp_broadcast(hi)}, \
                               OneValidity{validity}, out);                                        \
  }

QUIVER_K1_DEFINE(std::int8_t)
QUIVER_K1_DEFINE(std::int16_t)
QUIVER_K1_DEFINE(std::int32_t)
QUIVER_K1_DEFINE(std::int64_t)
QUIVER_K1_DEFINE(std::uint8_t)
QUIVER_K1_DEFINE(std::uint16_t)
QUIVER_K1_DEFINE(std::uint32_t)
QUIVER_K1_DEFINE(std::uint64_t)
QUIVER_K1_DEFINE(float)
QUIVER_K1_DEFINE(double)
#undef QUIVER_K1_DEFINE
// NOLINTEND(bugprone-macro-parentheses)

}  // namespace detail::avx2
QUIVER_TARGET_AVX2_END
QUIVER_END_NAMESPACE

#endif  // x86-64
