// K10 arith_guarded — AVX2 backend (PRD 08 §5 K10; ADR-014). Checked add/sub vectorize the
// sign trick (((a^r) & (b^r)) < 0 signed; carry compare unsigned) with overflow lanes
// packed to the position bitmap exactly like the K1 emit core. Saturating add/sub are
// native for 8/16-bit (paddsb/paddusb...) and checked+directed-clamp for 32/64-bit. ALL
// multiplies (checked and saturating) delegate to the scalar core: 64-bit is the documented
// REQ-K10-003 concession, and vectorizing the narrow widening-multiply forms is
// ledger-gated follow-up recorded on the family page. Bit-identical to
// arith_guarded_scalar_impl.h everywhere (REQ-KERNEL-002).
// Module: MOD-K10-ARITH-GUARDED | REQs: REQ-K10-001..003, REQ-SIMD-001..003 | ADR-014
#include "src/kernels/arith_guarded/arith_guarded_scalar_impl.h"
#include "src/kernels/common/target_regions.h"

#if defined(__x86_64__) || defined(_M_X64)

#include <cstring>
#include <immintrin.h>
#include <type_traits>

QUIVER_BEGIN_NAMESPACE
QUIVER_TARGET_AVX2_BEGIN
namespace detail::avx2 {

namespace {

// Lane-mask -> LSB-first bits for one 256-bit block at T's width (as in the K1 backend).
template <class T>
QUIVER_FORCE_INLINE std::uint32_t movemask_bits(__m256i lane_mask) noexcept {
  if constexpr (sizeof(T) == 1) {
    return static_cast<std::uint32_t>(_mm256_movemask_epi8(lane_mask));
  } else if constexpr (sizeof(T) == 2) {
    return _pext_u32(static_cast<std::uint32_t>(_mm256_movemask_epi8(lane_mask)), 0xAAAAAAAAu);
  } else if constexpr (sizeof(T) == 4) {
    return static_cast<std::uint32_t>(_mm256_movemask_ps(_mm256_castsi256_ps(lane_mask)));
  } else {
    return static_cast<std::uint32_t>(_mm256_movemask_pd(_mm256_castsi256_pd(lane_mask)));
  }
}

// One AVX2 lane width's instruction set. Selecting the width once here keeps every operation
// below a single expression instead of its own four-way sizeof(T) ladder. The saturating and
// bound members exist only at the widths that use them: 8/16-bit have native saturating
// add/sub, 32/64-bit instead need the signed bounds for the directed clamp.
template <int Bytes>
struct AgLanes;

template <>
struct AgLanes<1> {
  static QUIVER_FORCE_INLINE __m256i add(__m256i a, __m256i b) noexcept {
    return _mm256_add_epi8(a, b);
  }
  static QUIVER_FORCE_INLINE __m256i sub(__m256i a, __m256i b) noexcept {
    return _mm256_sub_epi8(a, b);
  }
  static QUIVER_FORCE_INLINE __m256i cmpgt(__m256i a, __m256i b) noexcept {
    return _mm256_cmpgt_epi8(a, b);
  }
  static QUIVER_FORCE_INLINE __m256i sign_bias() noexcept {
    return _mm256_set1_epi8(static_cast<char>(0x80));
  }
  static QUIVER_FORCE_INLINE __m256i adds_signed(__m256i a, __m256i b) noexcept {
    return _mm256_adds_epi8(a, b);
  }
  static QUIVER_FORCE_INLINE __m256i subs_signed(__m256i a, __m256i b) noexcept {
    return _mm256_subs_epi8(a, b);
  }
  static QUIVER_FORCE_INLINE __m256i adds_unsigned(__m256i a, __m256i b) noexcept {
    return _mm256_adds_epu8(a, b);
  }
  static QUIVER_FORCE_INLINE __m256i subs_unsigned(__m256i a, __m256i b) noexcept {
    return _mm256_subs_epu8(a, b);
  }
};

template <>
struct AgLanes<2> {
  static QUIVER_FORCE_INLINE __m256i add(__m256i a, __m256i b) noexcept {
    return _mm256_add_epi16(a, b);
  }
  static QUIVER_FORCE_INLINE __m256i sub(__m256i a, __m256i b) noexcept {
    return _mm256_sub_epi16(a, b);
  }
  static QUIVER_FORCE_INLINE __m256i cmpgt(__m256i a, __m256i b) noexcept {
    return _mm256_cmpgt_epi16(a, b);
  }
  static QUIVER_FORCE_INLINE __m256i sign_bias() noexcept {
    return _mm256_set1_epi16(static_cast<short>(0x8000));
  }
  static QUIVER_FORCE_INLINE __m256i adds_signed(__m256i a, __m256i b) noexcept {
    return _mm256_adds_epi16(a, b);
  }
  static QUIVER_FORCE_INLINE __m256i subs_signed(__m256i a, __m256i b) noexcept {
    return _mm256_subs_epi16(a, b);
  }
  static QUIVER_FORCE_INLINE __m256i adds_unsigned(__m256i a, __m256i b) noexcept {
    return _mm256_adds_epu16(a, b);
  }
  static QUIVER_FORCE_INLINE __m256i subs_unsigned(__m256i a, __m256i b) noexcept {
    return _mm256_subs_epu16(a, b);
  }
};

template <>
struct AgLanes<4> {
  static QUIVER_FORCE_INLINE __m256i add(__m256i a, __m256i b) noexcept {
    return _mm256_add_epi32(a, b);
  }
  static QUIVER_FORCE_INLINE __m256i sub(__m256i a, __m256i b) noexcept {
    return _mm256_sub_epi32(a, b);
  }
  static QUIVER_FORCE_INLINE __m256i cmpgt(__m256i a, __m256i b) noexcept {
    return _mm256_cmpgt_epi32(a, b);
  }
  static QUIVER_FORCE_INLINE __m256i sign_bias() noexcept {
    return _mm256_set1_epi32(static_cast<int>(0x80000000u));
  }
  static QUIVER_FORCE_INLINE __m256i signed_max() noexcept { return _mm256_set1_epi32(0x7FFFFFFF); }
  static QUIVER_FORCE_INLINE __m256i signed_min() noexcept {
    return _mm256_set1_epi32(static_cast<int>(0x80000000u));
  }
};

template <>
struct AgLanes<8> {
  static QUIVER_FORCE_INLINE __m256i add(__m256i a, __m256i b) noexcept {
    return _mm256_add_epi64(a, b);
  }
  static QUIVER_FORCE_INLINE __m256i sub(__m256i a, __m256i b) noexcept {
    return _mm256_sub_epi64(a, b);
  }
  static QUIVER_FORCE_INLINE __m256i cmpgt(__m256i a, __m256i b) noexcept {
    return _mm256_cmpgt_epi64(a, b);
  }
  static QUIVER_FORCE_INLINE __m256i sign_bias() noexcept {
    return _mm256_set1_epi64x(static_cast<long long>(0x8000000000000000ull));
  }
  static QUIVER_FORCE_INLINE __m256i signed_max() noexcept {
    return _mm256_set1_epi64x(0x7FFFFFFFFFFFFFFFll);
  }
  static QUIVER_FORCE_INLINE __m256i signed_min() noexcept {
    return _mm256_set1_epi64x(static_cast<long long>(0x8000000000000000ull));
  }
};

template <class T>
using AgW = AgLanes<static_cast<int>(sizeof(T))>;

// Sign bit of each lane as an all-ones/all-zero lane mask (cmpgt against zero, which covers
// the widths with no vpsra*).
template <class T>
QUIVER_FORCE_INLINE __m256i sign_mask(__m256i v) noexcept {
  return AgW<T>::cmpgt(_mm256_setzero_si256(), v);
}

// Unsigned lane compare a < b via sign-bias (as in the K1 backend).
template <class T>
QUIVER_FORCE_INLINE __m256i ult_mask(__m256i a, __m256i b) noexcept {
  using W = AgW<T>;
  const __m256i bias = W::sign_bias();
  return W::cmpgt(_mm256_xor_si256(b, bias), _mm256_xor_si256(a, bias));
}

// Checked add/sub for one block: {wrapped result, overflow lane mask}.
template <class T>
QUIVER_FORCE_INLINE __m256i checked_block(ArithOp op, __m256i a, __m256i b,
                                          __m256i* overflow) noexcept {
  using W = AgW<T>;
  if constexpr (std::is_signed_v<T>) {
    if (op == ArithOp::kAdd) {
      const __m256i r = W::add(a, b);
      // ((a^r) & (b^r)) sign bit set => overflow
      *overflow = sign_mask<T>(_mm256_and_si256(_mm256_xor_si256(a, r), _mm256_xor_si256(b, r)));
      return r;
    }
    const __m256i r = W::sub(a, b);
    *overflow = sign_mask<T>(_mm256_and_si256(_mm256_xor_si256(a, b), _mm256_xor_si256(a, r)));
    return r;
  } else {
    if (op == ArithOp::kAdd) {
      const __m256i r = W::add(a, b);
      *overflow = ult_mask<T>(r, a);  // carry: r < a
      return r;
    }
    const __m256i r = W::sub(a, b);
    *overflow = ult_mask<T>(a, b);  // borrow: a < b
    return r;
  }
}

// The left-hand batch a guarded op reads.
template <class T>
struct AgBatch {
  const T* a;
  std::int64_t n;
};

// Where a checked op writes: values, plus the optional per-element overflow bitmap.
template <class T>
struct AgCheckedOut {
  T* out;
  std::uint8_t* overflow_bits;
};

// Vectorized checked add/sub over 8-element-aligned groups (bitmap byte granularity), with
// count and optional position bits; multiplies fall through to the scalar core. Holding the
// per-call invariants keeps each step small enough to read on its own.
template <class T, class LoadB>
struct AgCheckedRun {
  static constexpr std::int64_t kW = static_cast<std::int64_t>(32 / sizeof(T));
  static constexpr std::int64_t kGroup = (kW < 8) ? 8 : kW;  // >= one bitmap byte per group

  ArithOp op;
  AgBatch<T> in;
  LoadB load_b;
  AgCheckedOut<T> sink;

  std::int64_t tail_start() const noexcept { return in.n / kGroup * kGroup; }

  void store_byte(std::int64_t idx, std::uint8_t byte) const noexcept {
    if (sink.overflow_bits != nullptr) {
      sink.overflow_bits[idx] = byte;
    }
  }

  // One kGroup-element group; returns its overflow mask, LSB-first.
  std::uint32_t group(std::int64_t i) const noexcept {
    std::uint32_t bits = 0;
    for (std::int64_t v = 0; v < kGroup; v += kW) {
      const __m256i va = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(in.a + i + v));
      __m256i ov;
      const __m256i r = checked_block<T>(op, va, load_b(i + v), &ov);
      _mm256_storeu_si256(reinterpret_cast<__m256i*>(sink.out + i + v), r);
      bits |= movemask_bits<T>(ov) << v;
    }
    return bits;
  }

  std::int64_t blocks() const noexcept {
    std::int64_t count = 0;
    for (std::int64_t i = 0; i + kGroup <= in.n; i += kGroup) {
      const std::uint32_t bits = group(i);
      count += std::popcount(bits);
      if (sink.overflow_bits != nullptr) {
        std::memcpy(sink.overflow_bits + (i >> 3), &bits, kGroup / 8);
      }
    }
    return count;
  }

  // Scalar tail with byte assembly (ADR-015/ADR-016); tail bits zero by construction.
  std::int64_t tail() const noexcept {
    std::int64_t count = 0;
    std::int64_t byte_idx = tail_start() >> 3;
    std::uint8_t byte = 0;
    int k = 0;
    for (std::int64_t i = tail_start(); i < in.n; ++i) {
      T r;
      const bool ov = scalar_impl::checked_one(op, in.a[i], load_b.tail(i), &r);
      sink.out[i] = r;
      count += ov ? 1 : 0;
      byte = static_cast<std::uint8_t>(byte | (static_cast<std::uint8_t>(ov) << k));
      if (++k == 8) {
        store_byte(byte_idx++, byte);
        byte = 0;
        k = 0;
      }
    }
    if (k != 0) {
      store_byte(byte_idx, byte);
    }
    return count;
  }
};

template <class T, class LoadB>
std::int64_t checked_addsub_impl(ArithOp op, AgBatch<T> in, LoadB load_b,
                                 AgCheckedOut<T> sink) noexcept {
  const AgCheckedRun<T, LoadB> run{op, in, load_b, sink};
  std::int64_t count = run.blocks();
  count += run.tail();
  return count;
}

// 8/16-bit lanes have native saturating add/sub.
template <class T>
QUIVER_FORCE_INLINE __m256i saturating_native(ArithOp op, __m256i a, __m256i b) noexcept {
  using W = AgW<T>;
  if constexpr (std::is_signed_v<T>) {
    return op == ArithOp::kAdd ? W::adds_signed(a, b) : W::subs_signed(a, b);
  } else {
    return op == ArithOp::kAdd ? W::adds_unsigned(a, b) : W::subs_unsigned(a, b);
  }
}

// 32/64-bit lanes have none, so take the checked result and blend a directed clamp on the
// overflow mask (PRD 08 §5 K10).
template <class T>
QUIVER_FORCE_INLINE __m256i saturating_clamped(ArithOp op, __m256i a, __m256i b) noexcept {
  using W = AgW<T>;
  __m256i ov;
  const __m256i r = checked_block<T>(op, a, b, &ov);
  __m256i limit;
  if constexpr (std::is_signed_v<T>) {
    // Overflow direction: add -> sign(a) picks MIN; sub -> (a < b) picks MIN.
    const __m256i toward_min = (op == ArithOp::kAdd) ? sign_mask<T>(a) : W::cmpgt(b, a);
    limit = _mm256_blendv_epi8(W::signed_max(), W::signed_min(), toward_min);
  } else {
    // Unsigned: add overflows toward MAX, sub toward 0.
    limit =
        (op == ArithOp::kAdd) ? _mm256_set1_epi8(static_cast<char>(0xFF)) : _mm256_setzero_si256();
  }
  return _mm256_blendv_epi8(r, limit, ov);
}

// Saturating add/sub for one block: native instructions for 8/16-bit; checked result +
// directed clamp for 32/64-bit (PRD 08 §5 K10).
template <class T>
QUIVER_FORCE_INLINE __m256i saturating_block(ArithOp op, __m256i a, __m256i b) noexcept {
  if constexpr (sizeof(T) <= 2) {
    return saturating_native<T>(op, a, b);
  } else {
    return saturating_clamped<T>(op, a, b);
  }
}

template <class T, class LoadB>
void saturating_addsub_impl(ArithOp op, AgBatch<T> in, LoadB load_b, T* out) noexcept {
  constexpr std::int64_t kW = static_cast<std::int64_t>(32 / sizeof(T));
  std::int64_t i = 0;
  for (; i + kW <= in.n; i += kW) {
    const __m256i va = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(in.a + i));
    _mm256_storeu_si256(reinterpret_cast<__m256i*>(out + i),
                        saturating_block<T>(op, va, load_b(i)));
  }
  for (; i < in.n; ++i) {
    out[i] = scalar_impl::saturate_one(op, in.a[i], load_b.tail(i));
  }
}

template <class T>
struct ag_BatchRhs {
  const T* b;
  __m256i operator()(std::int64_t i) const noexcept {
    return _mm256_loadu_si256(reinterpret_cast<const __m256i*>(b + i));
  }
  T tail(std::int64_t i) const noexcept { return b[i]; }
};

template <class T>
struct ag_ScalarRhs {
  T b;
  __m256i operator()(std::int64_t) const noexcept {
    if constexpr (sizeof(T) == 1) {
      return _mm256_set1_epi8(static_cast<char>(b));
    } else if constexpr (sizeof(T) == 2) {
      return _mm256_set1_epi16(static_cast<short>(b));
    } else if constexpr (sizeof(T) == 4) {
      return _mm256_set1_epi32(static_cast<int>(b));
    } else {
      return _mm256_set1_epi64x(static_cast<long long>(b));
    }
  }
  T tail(std::int64_t) const noexcept { return b; }
};

}  // namespace

// NOLINTBEGIN(bugprone-macro-parentheses): T expands to type names inside declarators.
#define QUIVER_K10_DEFINE(T)                                                                       \
  std::int64_t k10_arith_checked(ArithOp op, const T* a, const T* b, std::int64_t n, T* out,       \
                                 std::uint8_t* overflow_bits) noexcept {                           \
    if (op == ArithOp::kMul) {                                                                     \
      return scalar_impl::arith_checked<T>(op, a, b, n, out, overflow_bits);                       \
    }                                                                                              \
    return checked_addsub_impl<T>(op, {a, n}, ag_BatchRhs<T>{b}, {out, overflow_bits});            \
  }                                                                                                \
  std::int64_t k10_arith_checked_scalar_rhs(ArithOp op, const T* a, T b, std::int64_t n, T* out,   \
                                            std::uint8_t* overflow_bits) noexcept {                \
    if (op == ArithOp::kMul) {                                                                     \
      return scalar_impl::arith_checked_scalar_rhs<T>(op, a, b, n, out, overflow_bits);            \
    }                                                                                              \
    return checked_addsub_impl<T>(op, {a, n}, ag_ScalarRhs<T>{b}, {out, overflow_bits});           \
  }                                                                                                \
  void k10_arith_saturating(ArithOp op, const T* a, const T* b, std::int64_t n, T* out) noexcept { \
    if (op == ArithOp::kMul) {                                                                     \
      scalar_impl::arith_saturating<T>(op, a, b, n, out); /* REQ-K10-003 concession */             \
      return;                                                                                      \
    }                                                                                              \
    saturating_addsub_impl<T>(op, {a, n}, ag_BatchRhs<T>{b}, out);                                 \
  }                                                                                                \
  void k10_arith_saturating_scalar_rhs(ArithOp op, const T* a, T b, std::int64_t n,                \
                                       T* out) noexcept {                                          \
    if (op == ArithOp::kMul) {                                                                     \
      scalar_impl::arith_saturating_scalar_rhs<T>(op, a, b, n, out);                               \
      return;                                                                                      \
    }                                                                                              \
    saturating_addsub_impl<T>(op, {a, n}, ag_ScalarRhs<T>{b}, out);                                \
  }

QUIVER_K10_DEFINE(std::int8_t)
QUIVER_K10_DEFINE(std::int16_t)
QUIVER_K10_DEFINE(std::int32_t)
QUIVER_K10_DEFINE(std::int64_t)
QUIVER_K10_DEFINE(std::uint8_t)
QUIVER_K10_DEFINE(std::uint16_t)
QUIVER_K10_DEFINE(std::uint32_t)
QUIVER_K10_DEFINE(std::uint64_t)
#undef QUIVER_K10_DEFINE
// NOLINTEND(bugprone-macro-parentheses)

}  // namespace detail::avx2
QUIVER_TARGET_AVX2_END
QUIVER_END_NAMESPACE

#endif  // x86-64
