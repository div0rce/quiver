// K1 compare — NEON backend (PRD 08 K1; Survey §4.1). Technique: 128-bit vector compares
// packed to predicate bits per 8-element group via bit-weight AND + horizontal add (vaddv) —
// NEON has no movemask instruction; the PRD's `shrn` narrowing idiom is the documented
// alternative micro-optimization, to be evaluated with ledger data (gate M5 technique note).
// Unsigned orderings are NATIVE on NEON (vcgtq_u*, no bias trick). Integers use the
// {eq, gt}+swap primitives with ne/le/ge derived by bit inversion after packing (exact for
// total orders); floats use native ordered compares (vclt/vcle/vcgt/vcge: NaN lanes false)
// with kNe as post-pack inversion of eq — !(a==b) is exactly C++ `!=` NaN semantics.
// Groups are 8 elements (16 for 8-bit): one vector for 8/16-bit lanes, two for 32-bit,
// four for 64-bit. The 64-bit BITMAP forms instead delegate to the autovectorized scalar
// reference (measured faster than the handwritten pack on Apple M2; the 64-bit-lane pack needs a
// horizontal reduce per group). Selvec and narrower widths keep the handwritten NEON below.
// Validity ANDs at byte granularity; selvec forms feed predicate bytes
// into the kCompactLut32 index-store core; scratch stays inside the n-element capacity
// region (REQ-MEM-008). Tails are scalar and byte-assembled exactly like the reference
// (ADR-015). Bit-identical to compare_scalar_impl.h (REQ-KERNEL-002).
// Module: MOD-K1-COMPARE | REQs: REQ-K1-001..003, REQ-SIMD-001..003/-005/-008 | ADR-003
#include "src/kernels/common/luts.h"
#include "src/kernels/compare/compare_scalar_impl.h"

#if defined(__aarch64__) || defined(_M_ARM64)

#include <arm_neon.h>
#include <cstring>
#include <type_traits>

QUIVER_BEGIN_NAMESPACE
namespace detail::neon {

namespace {

// --- Type-dispatched vector aliases, loads, broadcasts ---------------------------------------

template <class T>
struct cmp_VecOf;
template <>
struct cmp_VecOf<std::int8_t> {
  using type = int8x16_t;
};
template <>
struct cmp_VecOf<std::uint8_t> {
  using type = uint8x16_t;
};
template <>
struct cmp_VecOf<std::int16_t> {
  using type = int16x8_t;
};
template <>
struct cmp_VecOf<std::uint16_t> {
  using type = uint16x8_t;
};
template <>
struct cmp_VecOf<std::int32_t> {
  using type = int32x4_t;
};
template <>
struct cmp_VecOf<std::uint32_t> {
  using type = uint32x4_t;
};
template <>
struct cmp_VecOf<std::int64_t> {
  using type = int64x2_t;
};
template <>
struct cmp_VecOf<std::uint64_t> {
  using type = uint64x2_t;
};
template <>
struct cmp_VecOf<float> {
  using type = float32x4_t;
};
template <>
struct cmp_VecOf<double> {
  using type = float64x2_t;
};
template <class T>
using cmp_Vec = typename cmp_VecOf<T>::type;

template <class T>
QUIVER_FORCE_INLINE cmp_Vec<T> cmp_load_vec(const T* p) noexcept {
  if constexpr (std::is_same_v<T, std::int8_t>) {
    return vld1q_s8(p);
  } else if constexpr (std::is_same_v<T, std::uint8_t>) {
    return vld1q_u8(p);
  } else if constexpr (std::is_same_v<T, std::int16_t>) {
    return vld1q_s16(p);
  } else if constexpr (std::is_same_v<T, std::uint16_t>) {
    return vld1q_u16(p);
  } else if constexpr (std::is_same_v<T, std::int32_t>) {
    return vld1q_s32(p);
  } else if constexpr (std::is_same_v<T, std::uint32_t>) {
    return vld1q_u32(p);
  } else if constexpr (std::is_same_v<T, std::int64_t>) {
    return vld1q_s64(p);
  } else if constexpr (std::is_same_v<T, std::uint64_t>) {
    return vld1q_u64(p);
  } else if constexpr (std::is_same_v<T, float>) {
    return vld1q_f32(p);
  } else {
    return vld1q_f64(p);
  }
}

template <class T>
QUIVER_FORCE_INLINE cmp_Vec<T> cmp_broadcast(T v) noexcept {
  if constexpr (std::is_same_v<T, std::int8_t>) {
    return vdupq_n_s8(v);
  } else if constexpr (std::is_same_v<T, std::uint8_t>) {
    return vdupq_n_u8(v);
  } else if constexpr (std::is_same_v<T, std::int16_t>) {
    return vdupq_n_s16(v);
  } else if constexpr (std::is_same_v<T, std::uint16_t>) {
    return vdupq_n_u16(v);
  } else if constexpr (std::is_same_v<T, std::int32_t>) {
    return vdupq_n_s32(v);
  } else if constexpr (std::is_same_v<T, std::uint32_t>) {
    return vdupq_n_u32(v);
  } else if constexpr (std::is_same_v<T, std::int64_t>) {
    return vdupq_n_s64(v);
  } else if constexpr (std::is_same_v<T, std::uint64_t>) {
    return vdupq_n_u64(v);
  } else if constexpr (std::is_same_v<T, float>) {
    return vdupq_n_f32(v);
  } else {
    return vdupq_n_f64(v);
  }
}

// Unsigned lane-mask type of T's width (what vector compares return).
template <class T>
QUIVER_FORCE_INLINE auto cmp_eq(cmp_Vec<T> a, cmp_Vec<T> b) noexcept {
  if constexpr (sizeof(T) == 1) {
    if constexpr (std::is_signed_v<T>) {
      return vceqq_s8(a, b);
    } else {
      return vceqq_u8(a, b);
    }
  } else if constexpr (sizeof(T) == 2) {
    if constexpr (std::is_signed_v<T>) {
      return vceqq_s16(a, b);
    } else {
      return vceqq_u16(a, b);
    }
  } else if constexpr (sizeof(T) == 4) {
    if constexpr (std::is_same_v<T, float>) {
      return vceqq_f32(a, b);
    } else if constexpr (std::is_signed_v<T>) {
      return vceqq_s32(a, b);
    } else {
      return vceqq_u32(a, b);
    }
  } else {
    if constexpr (std::is_same_v<T, double>) {
      return vceqq_f64(a, b);
    } else if constexpr (std::is_signed_v<T>) {
      return vceqq_s64(a, b);
    } else {
      return vceqq_u64(a, b);
    }
  }
}

template <class T>
QUIVER_FORCE_INLINE auto cmp_gt(cmp_Vec<T> a, cmp_Vec<T> b) noexcept {
  if constexpr (sizeof(T) == 1) {
    if constexpr (std::is_signed_v<T>) {
      return vcgtq_s8(a, b);
    } else {
      return vcgtq_u8(a, b);
    }
  } else if constexpr (sizeof(T) == 2) {
    if constexpr (std::is_signed_v<T>) {
      return vcgtq_s16(a, b);
    } else {
      return vcgtq_u16(a, b);
    }
  } else if constexpr (sizeof(T) == 4) {
    if constexpr (std::is_same_v<T, float>) {
      return vcgtq_f32(a, b);
    } else if constexpr (std::is_signed_v<T>) {
      return vcgtq_s32(a, b);
    } else {
      return vcgtq_u32(a, b);
    }
  } else {
    if constexpr (std::is_same_v<T, double>) {
      return vcgtq_f64(a, b);
    } else if constexpr (std::is_signed_v<T>) {
      return vcgtq_s64(a, b);
    } else {
      return vcgtq_u64(a, b);
    }
  }
}

template <class T>
QUIVER_FORCE_INLINE auto cmp_ge(cmp_Vec<T> a,
                                cmp_Vec<T> b) noexcept {  // floats only (native ordered)
  if constexpr (std::is_same_v<T, float>) {
    return vcgeq_f32(a, b);
  } else {
    return vcgeq_f64(a, b);
  }
}

template <class T>
QUIVER_FORCE_INLINE auto cmp_le(cmp_Vec<T> a,
                                cmp_Vec<T> b) noexcept {  // floats only (native ordered)
  if constexpr (std::is_same_v<T, float>) {
    return vcleq_f32(a, b);
  } else {
    return vcleq_f64(a, b);
  }
}

// Lane-mask combinators at the mask's own width.
QUIVER_FORCE_INLINE uint8x16_t mask_or(uint8x16_t a, uint8x16_t b) noexcept {
  return vorrq_u8(a, b);
}
QUIVER_FORCE_INLINE uint16x8_t mask_or(uint16x8_t a, uint16x8_t b) noexcept {
  return vorrq_u16(a, b);
}
QUIVER_FORCE_INLINE uint32x4_t mask_or(uint32x4_t a, uint32x4_t b) noexcept {
  return vorrq_u32(a, b);
}
QUIVER_FORCE_INLINE uint64x2_t mask_or(uint64x2_t a, uint64x2_t b) noexcept {
  return vorrq_u64(a, b);
}
// (float `between` only: integer between uses mask_or + post-pack inversion instead)
QUIVER_FORCE_INLINE uint32x4_t mask_and(uint32x4_t a, uint32x4_t b) noexcept {
  return vandq_u32(a, b);
}
QUIVER_FORCE_INLINE uint64x2_t mask_and(uint64x2_t a, uint64x2_t b) noexcept {
  return vandq_u64(a, b);
}

// --- Bit packing: weight-AND + horizontal add (one bit per lane, LSB-first) ------------------

QUIVER_FORCE_INLINE std::uint32_t pack_bits(uint8x16_t m) noexcept {  // 16 lanes -> 16 bits
  const uint8x8_t w = vcreate_u8(0x8040201008040201ull);
  const std::uint32_t lo = vaddv_u8(vand_u8(vget_low_u8(m), w));
  const std::uint32_t hi = vaddv_u8(vand_u8(vget_high_u8(m), w));
  return lo | (hi << 8);
}
QUIVER_FORCE_INLINE std::uint32_t pack_bits(uint16x8_t m) noexcept {  // 8 lanes -> 8 bits
  const uint16x8_t w = {1, 2, 4, 8, 16, 32, 64, 128};
  return vaddvq_u16(vandq_u16(m, w));
}
// 32/64-bit lane masks pack via weight-AND + a PAIRWISE-add tree (vpaddq) — no per-vector
// scalar extracts and no long narrowing chain. First-ledger evidence on Apple M2: the
// scalar-extract version ran ~2.5x behind autovec and a vmovn narrowing chain ~1.7x behind;
// the pairwise tree is the cheapest packing found (whatever the ledger says about the final
// verdict is what the family page publishes — Charter T7).
QUIVER_FORCE_INLINE std::uint32_t pack2_bits(uint32x4_t m0, uint32x4_t m1) noexcept {
  const uint32x4_t w_lo = {1, 2, 4, 8};
  const uint32x4_t w_hi = {16, 32, 64, 128};
  const uint32x4_t s = vpaddq_u32(vandq_u32(m0, w_lo), vandq_u32(m1, w_hi));
  return vaddvq_u32(s);
}
QUIVER_FORCE_INLINE std::uint32_t pack4_bits(uint64x2_t m0, uint64x2_t m1, uint64x2_t m2,
                                             uint64x2_t m3) noexcept {
  const uint64x2_t w01 = {1, 2};
  const uint64x2_t w23 = {4, 8};
  const uint64x2_t w45 = {16, 32};
  const uint64x2_t w67 = {64, 128};
  const uint64x2_t s0 = vpaddq_u64(vandq_u64(m0, w01), vandq_u64(m1, w23));
  const uint64x2_t s1 = vpaddq_u64(vandq_u64(m2, w45), vandq_u64(m3, w67));
  return static_cast<std::uint32_t>(vaddvq_u64(vpaddq_u64(s0, s1)));
}

// --- Vector predicates (mirroring the AVX2 backend's structure): mask(i) yields the lane
// --- mask for one vector at element i; inv() says whether packed bits must be complemented
// --- (integers + float kNe only); one(i) is the exact scalar predicate for tails.

template <class T>
QUIVER_FORCE_INLINE auto op_mask(CompareOp op, cmp_Vec<T> a, cmp_Vec<T> b) noexcept {
  if constexpr (std::is_floating_point_v<T>) {
    // Native ordered compares (NaN false); kNe = post-pack inversion of eq (NaN true, exact).
    switch (op) {
    case CompareOp::kEq:
    case CompareOp::kNe:
      return cmp_eq<T>(a, b);
    case CompareOp::kLt:
      return cmp_gt<T>(b, a);
    case CompareOp::kLe:
      return cmp_le<T>(a, b);
    case CompareOp::kGt:
      return cmp_gt<T>(a, b);
    case CompareOp::kGe:
      return cmp_ge<T>(a, b);
    }
    return cmp_eq<T>(a, a);  // unreachable for in-contract op values
  } else {
    switch (op) {
    case CompareOp::kEq:
    case CompareOp::kNe:
      return cmp_eq<T>(a, b);
    case CompareOp::kGt:
    case CompareOp::kLe:
      return cmp_gt<T>(a, b);
    case CompareOp::kLt:
    case CompareOp::kGe:
      return cmp_gt<T>(b, a);
    }
    return cmp_eq<T>(a, a);  // unreachable for in-contract op values
  }
}

template <class T>
QUIVER_FORCE_INLINE bool op_inverts(CompareOp op) noexcept {
  if constexpr (std::is_floating_point_v<T>) {
    return op == CompareOp::kNe;
  } else {
    return op == CompareOp::kNe || op == CompareOp::kLe || op == CompareOp::kGe;
  }
}

template <class T>
struct CmpRhs {  // in[i] <op> comparand
  CompareOp op;
  const T* in;
  T comparand;
  cmp_Vec<T> bvec;
  QUIVER_FORCE_INLINE auto mask(std::int64_t i) const noexcept {
    return op_mask<T>(op, cmp_load_vec(in + i), bvec);
  }
  QUIVER_FORCE_INLINE bool inv() const noexcept { return op_inverts<T>(op); }
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
    return op_mask<T>(op, cmp_load_vec(a + i), cmp_load_vec(b + i));
  }
  QUIVER_FORCE_INLINE bool inv() const noexcept { return op_inverts<T>(op); }
  QUIVER_FORCE_INLINE bool one(std::int64_t i) const noexcept {
    return scalar_impl::compare_one(op, a[i], b[i]);
  }
};

template <class T>
struct CmpBetween {  // lo <= in[i] && in[i] <= hi (inclusive; NaN excluded by ordered cmp)
  const T* in;
  T lo;
  T hi;
  cmp_Vec<T> lo_vec;
  cmp_Vec<T> hi_vec;
  QUIVER_FORCE_INLINE auto mask(std::int64_t i) const noexcept {
    const cmp_Vec<T> a = cmp_load_vec(in + i);
    if constexpr (std::is_floating_point_v<T>) {
      return mask_and(cmp_ge<T>(a, lo_vec), cmp_le<T>(a, hi_vec));
    } else {
      // in-range = !(lo > a || a > hi): OR the exclusions, complement after packing.
      return mask_or(cmp_gt<T>(lo_vec, a), cmp_gt<T>(a, hi_vec));
    }
  }
  QUIVER_FORCE_INLINE bool inv() const noexcept { return std::is_integral_v<T>; }
  QUIVER_FORCE_INLINE bool one(std::int64_t i) const noexcept {
    return (lo <= in[i]) && (in[i] <= hi);
  }
};

// --- Validity fetchers (byte granularity; nullptr means all-valid, REQ-API-008) --------------

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

// Group geometry: 8-element groups (16 for byte lanes) — 1/1/2/4 vectors per group.
template <class T>
constexpr std::int64_t group_lanes() noexcept {
  return sizeof(T) == 1 ? 16 : 8;
}
template <class T>
constexpr std::int64_t cmp_vec_lanes() noexcept {
  return static_cast<std::int64_t>(16 / sizeof(T));
}

// Packed predicate bits for the group starting at element i (inversion applied, width-masked).
template <class T, class Pred>
QUIVER_FORCE_INLINE std::uint32_t group_bits(const Pred& pred, std::int64_t i) noexcept {
  constexpr std::int64_t kV = cmp_vec_lanes<T>();
  std::uint32_t bits;
  if constexpr (sizeof(T) <= 2) {
    bits = pack_bits(pred.mask(i));
  } else if constexpr (sizeof(T) == 4) {
    bits = pack2_bits(pred.mask(i), pred.mask(i + kV));
  } else {
    bits =
        pack4_bits(pred.mask(i), pred.mask(i + kV), pred.mask(i + 2 * kV), pred.mask(i + 3 * kV));
  }
  if (pred.inv()) {
    bits = ~bits;  // complement first, then width-mask
  }
  constexpr std::int64_t kGroup = group_lanes<T>();
  bits &= (kGroup == 16) ? 0xFFFFu : 0xFFu;
  return bits;
}

// Bitmap emission core: vector groups, byte-granular validity AND, scalar byte-assembled tail.
template <class T, class Pred, class Validity>
std::int64_t emit_bitmap_neon(std::int64_t n, const Pred& pred, const Validity& val,
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
// A store spans [count, count+8) with count <= the byte-group's start element, so the
// full-width stores stay inside the n-element capacity region (REQ-MEM-008).
template <class T, class Pred, class Validity>
std::int64_t emit_selvec_neon(std::int64_t n, const Pred& pred, const Validity& val,
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
      const uint32x4_t base = vdupq_n_u32(static_cast<std::uint32_t>(base_idx));
      const std::uint32_t* row = kCompactLut32.perm[byte];
      vst1q_u32(out + count, vaddq_u32(vld1q_u32(row), base));
      vst1q_u32(out + count + 4, vaddq_u32(vld1q_u32(row + 4), base));
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
    if constexpr (sizeof(T) == 8) /* 64-bit bitmap: autovec scalar wins (see header) */            \
      return scalar_impl::compare_bitmap(op, in, n, comparand, validity, out);                     \
    else                                                                                           \
      return emit_bitmap_neon<T>(n, CmpRhs<T>{op, in, comparand, cmp_broadcast(comparand)},        \
                                 OneValidity{validity}, out);                                      \
  }                                                                                                \
  std::int64_t k1_compare_bitmap2(CompareOp op, const T* a, const T* b, std::int64_t n,            \
                                  const std::uint8_t* a_validity, const std::uint8_t* b_validity,  \
                                  std::uint8_t* out) noexcept {                                    \
    if constexpr (sizeof(T) == 8)                                                                  \
      return scalar_impl::compare_bitmap2(op, a, b, n, a_validity, b_validity, out);               \
    else                                                                                           \
      return emit_bitmap_neon<T>(n, CmpBatch<T>{op, a, b}, TwoValidity{a_validity, b_validity},    \
                                 out);                                                             \
  }                                                                                                \
  std::int64_t k1_compare_between_bitmap(const T* in, std::int64_t n, T lo, T hi,                  \
                                         const std::uint8_t* validity,                             \
                                         std::uint8_t* out) noexcept {                             \
    if constexpr (sizeof(T) == 8)                                                                  \
      return scalar_impl::compare_between_bitmap(in, n, lo, hi, validity, out);                    \
    else                                                                                           \
      return emit_bitmap_neon<T>(n,                                                                \
                                 CmpBetween<T>{in, lo, hi, cmp_broadcast(lo), cmp_broadcast(hi)},  \
                                 OneValidity{validity}, out);                                      \
  }                                                                                                \
  std::int64_t k1_compare_selvec(CompareOp op, const T* in, std::int64_t n, T comparand,           \
                                 const std::uint8_t* validity, std::uint32_t* out) noexcept {      \
    return emit_selvec_neon<T>(n, CmpRhs<T>{op, in, comparand, cmp_broadcast(comparand)},          \
                               OneValidity{validity}, out);                                        \
  }                                                                                                \
  std::int64_t k1_compare_selvec2(CompareOp op, const T* a, const T* b, std::int64_t n,            \
                                  const std::uint8_t* a_validity, const std::uint8_t* b_validity,  \
                                  std::uint32_t* out) noexcept {                                   \
    return emit_selvec_neon<T>(n, CmpBatch<T>{op, a, b}, TwoValidity{a_validity, b_validity},      \
                               out);                                                               \
  }                                                                                                \
  std::int64_t k1_compare_between_selvec(const T* in, std::int64_t n, T lo, T hi,                  \
                                         const std::uint8_t* validity,                             \
                                         std::uint32_t* out) noexcept {                            \
    return emit_selvec_neon<T>(n, CmpBetween<T>{in, lo, hi, cmp_broadcast(lo), cmp_broadcast(hi)}, \
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

}  // namespace detail::neon
QUIVER_END_NAMESPACE

#endif  // aarch64
