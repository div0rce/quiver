// K6 reduce / SMA — AVX2 backend (PRD 08 §5 K6; ADR-013). Dense (sel == nullptr) shapes are
// vectorized with validity lane-mask expansion; selected shapes delegate to the scalar core
// (random-access dominated; the AVX2 backend's selected float sum is therefore the strict
// sequential fold — documented per-backend policy, ADR-013).
//
// min/max: native vector min/max plus two exactness rescues that make ANY lane blocking
// bit-identical to the scalar first-encountered fold: (1) any valid NaN forces the canonical
// qNaN return (PRD 08 §3.3), so NaN poisoning of min/max lanes is irrelevant; (2) every
// remaining float tie is bit-identical EXCEPT ±0.0, so when the folded result equals 0.0 a
// scalar rescan returns the first participating zero — exactly the value the scalar spec
// keeps. Integers are total orders: lane order cannot change the value.
//
// Integer sums: elements widen (sign/zero-extend) to 64-bit lanes BEFORE accumulation —
// wrapping is only defined at SumType width (REQ-STD-008) — then wrap-add lanewise; wrapping
// addition is commutative, so lane blocking is exact. Checked sums delegate to the scalar
// 128-bit core.
//
// FLOAT SUM POLICY (ADR-013, AVX2): A=4 vector accumulators × W lanes (f32 W=8, f64 W=4),
// masked lanes add -0.0 (the exact neutral: x + -0.0 == x for every x under round-to-
// nearest, including x == -0.0); combine = the ADR-013 frozen pairwise order (0+2),(1+3),
// then +, lanewise; then a strict low->high fold over lanes starting from +0.0 (lanes cannot
// be -0.0: they start at +0.0 and RN addition only yields -0.0 from two -0.0 operands); the
// tail (n mod 4W) adds sequentially, valid-only. The testkit blocked-sum oracle mirrors this
// arithmetic exactly for non-NaN results (REQ-TEST-004); NaN results compare as a class —
// payload selection follows hardware operand order, which C++ does not pin (gate M4).
// Module: MOD-K6-REDUCE | REQs: REQ-K6-001..005, REQ-SIMD-001..003/-005 | ADR-003, ADR-013
#include "src/kernels/common/target_regions.h"
#include "src/kernels/reduce/reduce_scalar_impl.h"

#if defined(__x86_64__) || defined(_M_X64)

#include <bit>
#include <cstring>
#include <immintrin.h>
#include <limits>
#include <type_traits>

QUIVER_BEGIN_NAMESPACE
QUIVER_TARGET_AVX2_BEGIN
namespace detail::avx2 {

namespace {

// --- Validity lane-mask expansion (PRD 08 K6 technique) --------------------------------------

// W validity bits for the W-element group starting at element i (i is W-aligned).
template <int W>
QUIVER_FORCE_INLINE std::uint32_t validity_bits(const std::uint8_t* v, std::int64_t i) noexcept {
  if constexpr (W == 4) {
    return (static_cast<std::uint32_t>(v[i >> 3]) >> (i & 4)) & 0xFu;
  } else {
    std::uint32_t x = 0;
    std::memcpy(&x, v + (i >> 3), W / 8);
    return x;
  }
}

QUIVER_FORCE_INLINE __m256i lane_mask64(std::uint32_t nibble) noexcept {
  const __m256i bit = _mm256_setr_epi64x(1, 2, 4, 8);
  const __m256i v = _mm256_set1_epi64x(static_cast<long long>(nibble));
  return _mm256_cmpeq_epi64(_mm256_and_si256(v, bit), bit);
}

QUIVER_FORCE_INLINE __m256i lane_mask32(std::uint32_t byte) noexcept {
  const __m256i bit = _mm256_setr_epi32(1, 2, 4, 8, 16, 32, 64, 128);
  const __m256i v = _mm256_set1_epi32(static_cast<int>(byte));
  return _mm256_cmpeq_epi32(_mm256_and_si256(v, bit), bit);
}

QUIVER_FORCE_INLINE __m256i lane_mask16(std::uint32_t bits16) noexcept {
  // Words 0..7 take validity byte 0, words 8..15 byte 1 (shuffle_epi8 zero-extends via the
  // 0x80 control bytes), then each word tests its bit weight.
  const __m256i v = _mm256_set1_epi16(static_cast<short>(bits16));
  const __m256i ctrl =
      _mm256_setr_epi8(0, -128, 0, -128, 0, -128, 0, -128, 0, -128, 0, -128, 0, -128, 0, -128, 1,
                       -128, 1, -128, 1, -128, 1, -128, 1, -128, 1, -128, 1, -128, 1, -128);
  const __m256i bytes = _mm256_shuffle_epi8(v, ctrl);
  const __m256i bit = _mm256_setr_epi16(1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128);
  return _mm256_cmpeq_epi16(_mm256_and_si256(bytes, bit), bit);
}

QUIVER_FORCE_INLINE __m256i lane_mask8(std::uint32_t bits32) noexcept {
  // Byte lane g takes validity byte g/8, then tests bit weight g%8.
  const __m256i v = _mm256_set1_epi32(static_cast<int>(bits32));
  const __m256i ctrl = _mm256_setr_epi8(0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2,
                                        2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3);
  const __m256i bytes = _mm256_shuffle_epi8(v, ctrl);
  const __m256i bit = _mm256_setr_epi8(1, 2, 4, 8, 16, 32, 64, -128, 1, 2, 4, 8, 16, 32, 64, -128,
                                       1, 2, 4, 8, 16, 32, 64, -128, 1, 2, 4, 8, 16, 32, 64, -128);
  return _mm256_cmpeq_epi8(_mm256_and_si256(bytes, bit), bit);
}

template <class T>
QUIVER_FORCE_INLINE __m256i lane_mask_for(std::uint32_t bits) noexcept {
  if constexpr (sizeof(T) == 1) {
    return lane_mask8(bits);
  } else if constexpr (sizeof(T) == 2) {
    return lane_mask16(bits);
  } else if constexpr (sizeof(T) == 4) {
    return lane_mask32(bits);
  } else {
    return lane_mask64(bits);
  }
}

// Participating (selected ∧ valid) count for the dense shape: n minus cleared validity bits.
QUIVER_FORCE_INLINE std::int64_t valid_count(const std::uint8_t* v, std::int64_t n) noexcept {
  if (v == nullptr) {
    return n;
  }
  std::int64_t c = 0;
  const std::int64_t full = n >> 3;
  for (std::int64_t b = 0; b < full; ++b) {
    c += std::popcount(v[b]);
  }
  if ((n & 7) != 0) {
    c += std::popcount(static_cast<std::uint8_t>(v[full] & tail_mask(n)));
  }
  return c;
}

// --- Integer dense min/max --------------------------------------------------------------------

template <class T, bool IsMin>
QUIVER_FORCE_INLINE __m256i int_minmax_step(__m256i acc, __m256i v) noexcept {
  constexpr bool kSigned = std::is_signed_v<T>;
  if constexpr (sizeof(T) == 1) {
    if constexpr (IsMin) {
      return kSigned ? _mm256_min_epi8(acc, v) : _mm256_min_epu8(acc, v);
    } else {
      return kSigned ? _mm256_max_epi8(acc, v) : _mm256_max_epu8(acc, v);
    }
  } else if constexpr (sizeof(T) == 2) {
    if constexpr (IsMin) {
      return kSigned ? _mm256_min_epi16(acc, v) : _mm256_min_epu16(acc, v);
    } else {
      return kSigned ? _mm256_max_epi16(acc, v) : _mm256_max_epu16(acc, v);
    }
  } else if constexpr (sizeof(T) == 4) {
    if constexpr (IsMin) {
      return kSigned ? _mm256_min_epi32(acc, v) : _mm256_min_epu32(acc, v);
    } else {
      return kSigned ? _mm256_max_epi32(acc, v) : _mm256_max_epu32(acc, v);
    }
  } else {
    // No 64-bit min/max in AVX2: compare (sign-biased for unsigned) then blend.
    __m256i ca = acc;
    __m256i cv = v;
    if constexpr (!kSigned) {
      const __m256i bias = _mm256_set1_epi64x(static_cast<long long>(0x8000000000000000ull));
      ca = _mm256_xor_si256(ca, bias);
      cv = _mm256_xor_si256(cv, bias);
    }
    const __m256i acc_gt = _mm256_cmpgt_epi64(ca, cv);
    return IsMin ? _mm256_blendv_epi8(acc, v, acc_gt) : _mm256_blendv_epi8(v, acc, acc_gt);
  }
}

template <class T>
QUIVER_FORCE_INLINE __m256i broadcast_int(T value) noexcept {
  if constexpr (sizeof(T) == 1) {
    return _mm256_set1_epi8(static_cast<char>(value));
  } else if constexpr (sizeof(T) == 2) {
    return _mm256_set1_epi16(static_cast<short>(value));
  } else if constexpr (sizeof(T) == 4) {
    return _mm256_set1_epi32(static_cast<int>(value));
  } else {
    return _mm256_set1_epi64x(static_cast<long long>(value));
  }
}

template <class T>
struct MinMax {
  T min;
  T max;
};

// The identity pair every fold starts from, and the value masked-off lanes contribute.
template <class T>
QUIVER_FORCE_INLINE MinMax<T> red_identity() noexcept {
  return {std::numeric_limits<T>::max(), std::numeric_limits<T>::lowest()};
}

// One participating element folded in. The Want* selection lives here, at depth 1, so the
// driving loops below stay flat.
template <class T, bool WantMin, bool WantMax>
QUIVER_FORCE_INLINE void red_fold_one(MinMax<T>& mm, T v) noexcept {
  if constexpr (WantMin) {
    mm.min = (v < mm.min) ? v : mm.min;
  }
  if constexpr (WantMax) {
    mm.max = (v > mm.max) ? v : mm.max;
  }
}

// The elements past the last full vector block, folded by the reference rule.
template <class T, std::int64_t kW, bool WantMin, bool WantMax>
QUIVER_FORCE_INLINE void red_scalar_tail(MinMax<T>& mm, const T* in, std::int64_t n,
                                         const std::uint8_t* validity) noexcept {
  for (std::int64_t i = n / kW * kW; i < n; ++i) {
    if (is_valid(validity, i)) {
      red_fold_one<T, WantMin, WantMax>(mm, in[i]);
    }
  }
}

// Live min/max accumulators plus the identities masked-off lanes are blended with.
struct RedIntAcc {
  __m256i min;
  __m256i max;
  __m256i idmin;
  __m256i idmax;
};

// Invalid lanes replaced by `ident`. `lm` is the block's expanded lane mask, computed once by
// the caller (so the min and max blends share it) and unused when there is no validity bitmap.
QUIVER_FORCE_INLINE __m256i red_int_masked(__m256i v, __m256i ident, __m256i lm,
                                           const std::uint8_t* validity) noexcept {
  return validity != nullptr ? _mm256_blendv_epi8(ident, v, lm) : v;
}

template <class T, bool WantMin, bool WantMax>
QUIVER_FORCE_INLINE void red_int_step(RedIntAcc& acc, const T* in, std::int64_t i,
                                      const std::uint8_t* validity) noexcept {
  constexpr int kW = static_cast<int>(32 / sizeof(T));
  const __m256i v = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(in + i));
  const __m256i lm = validity != nullptr ? lane_mask_for<T>(validity_bits<kW>(validity, i))
                                         : _mm256_setzero_si256();
  if constexpr (WantMin) {
    acc.min = int_minmax_step<T, true>(acc.min, red_int_masked(v, acc.idmin, lm, validity));
  }
  if constexpr (WantMax) {
    acc.max = int_minmax_step<T, false>(acc.max, red_int_masked(v, acc.idmax, lm, validity));
  }
}

// Horizontal fold of one accumulator. Masked-off and never-written lanes hold the identity, so
// they cannot win; folding an untouched accumulator therefore yields the identity itself.
template <class T, bool IsMin>
QUIVER_FORCE_INLINE T red_int_fold_lanes(__m256i acc) noexcept {
  constexpr std::int64_t kW = static_cast<std::int64_t>(32 / sizeof(T));
  alignas(32) T lanes[static_cast<std::size_t>(kW)];
  _mm256_storeu_si256(reinterpret_cast<__m256i*>(lanes), acc);
  T r = IsMin ? std::numeric_limits<T>::max() : std::numeric_limits<T>::lowest();
  for (std::int64_t k = 0; k < kW; ++k) {
    r = (IsMin ? (lanes[k] < r) : (lanes[k] > r)) ? lanes[k] : r;
  }
  return r;
}

template <class T, bool WantMin, bool WantMax>
MinMax<T> dense_minmax_int(const T* in, std::int64_t n, const std::uint8_t* validity) noexcept {
  constexpr std::int64_t kW = static_cast<std::int64_t>(32 / sizeof(T));
  const MinMax<T> id = red_identity<T>();
  const __m256i idmin = broadcast_int(id.min);
  const __m256i idmax = broadcast_int(id.max);
  RedIntAcc acc{idmin, idmax, idmin, idmax};
  for (std::int64_t i = 0; i + kW <= n; i += kW) {
    red_int_step<T, WantMin, WantMax>(acc, in, i, validity);
  }
  MinMax<T> mm{red_int_fold_lanes<T, true>(acc.min), red_int_fold_lanes<T, false>(acc.max)};
  red_scalar_tail<T, kW, WantMin, WantMax>(mm, in, n, validity);
  return mm;
}

// --- Float dense min/max ------------------------------------------------------------------------

// First participating value that compares equal to 0.0 — by the scalar fold, this is exactly
// the bit pattern the reference returns whenever its result is a zero (see file comment).
template <class T>
T first_participating_zero(const T* in, std::int64_t n, const std::uint8_t* validity) noexcept {
  for (std::int64_t i = 0; i < n; ++i) {
    if (is_valid(validity, i) && in[i] == T{0}) {
      return in[i];
    }
  }
  return T{0};  // unreachable when the caller's folded result equals 0.0
}

// Thin shim over the two AVX2 float widths so the folds below are written once. Every member is
// exactly the intrinsic the width-specific code used: no float operation is added, removed, or
// reordered by this indirection (reassociation is frozen — ADR-013, PRD 08 §3.3).
template <class T>
struct RedFloatVec;

template <>
struct RedFloatVec<float> {
  using V = __m256;
  static constexpr std::int64_t kW = 8;
  static QUIVER_FORCE_INLINE V set1(float x) noexcept { return _mm256_set1_ps(x); }
  static QUIVER_FORCE_INLINE V zero() noexcept { return _mm256_setzero_ps(); }
  static QUIVER_FORCE_INLINE V load(const float* p) noexcept { return _mm256_loadu_ps(p); }
  static QUIVER_FORCE_INLINE void store(float* p, V v) noexcept { _mm256_storeu_ps(p, v); }
  static QUIVER_FORCE_INLINE V mask_of(const std::uint8_t* b, std::int64_t i) noexcept {
    return _mm256_castsi256_ps(lane_mask32(validity_bits<8>(b, i)));
  }
  static QUIVER_FORCE_INLINE V blend(V a, V b, V m) noexcept { return _mm256_blendv_ps(a, b, m); }
  static QUIVER_FORCE_INLINE V add(V a, V b) noexcept { return _mm256_add_ps(a, b); }
  static QUIVER_FORCE_INLINE V min(V a, V b) noexcept { return _mm256_min_ps(a, b); }
  static QUIVER_FORCE_INLINE V max(V a, V b) noexcept { return _mm256_max_ps(a, b); }
  static QUIVER_FORCE_INLINE V unord(V a) noexcept { return _mm256_cmp_ps(a, a, _CMP_UNORD_Q); }
  static QUIVER_FORCE_INLINE V or_bits(V a, V b) noexcept { return _mm256_or_ps(a, b); }
  static QUIVER_FORCE_INLINE bool any(V a) noexcept { return _mm256_movemask_ps(a) != 0; }
};

template <>
struct RedFloatVec<double> {
  using V = __m256d;
  static constexpr std::int64_t kW = 4;
  static QUIVER_FORCE_INLINE V set1(double x) noexcept { return _mm256_set1_pd(x); }
  static QUIVER_FORCE_INLINE V zero() noexcept { return _mm256_setzero_pd(); }
  static QUIVER_FORCE_INLINE V load(const double* p) noexcept { return _mm256_loadu_pd(p); }
  static QUIVER_FORCE_INLINE void store(double* p, V v) noexcept { _mm256_storeu_pd(p, v); }
  static QUIVER_FORCE_INLINE V mask_of(const std::uint8_t* b, std::int64_t i) noexcept {
    return _mm256_castsi256_pd(lane_mask64(validity_bits<4>(b, i)));
  }
  static QUIVER_FORCE_INLINE V blend(V a, V b, V m) noexcept { return _mm256_blendv_pd(a, b, m); }
  static QUIVER_FORCE_INLINE V add(V a, V b) noexcept { return _mm256_add_pd(a, b); }
  static QUIVER_FORCE_INLINE V min(V a, V b) noexcept { return _mm256_min_pd(a, b); }
  static QUIVER_FORCE_INLINE V max(V a, V b) noexcept { return _mm256_max_pd(a, b); }
  static QUIVER_FORCE_INLINE V unord(V a) noexcept { return _mm256_cmp_pd(a, a, _CMP_UNORD_Q); }
  static QUIVER_FORCE_INLINE V or_bits(V a, V b) noexcept { return _mm256_or_pd(a, b); }
  static QUIVER_FORCE_INLINE bool any(V a) noexcept { return _mm256_movemask_pd(a) != 0; }
};

// Live min/max accumulators, the running NaN disjunction, and the blend identities.
template <class T>
struct RedFloatAcc {
  using V = typename RedFloatVec<T>::V;
  V min;
  V max;
  V nans;
  V idmin;
  V idmax;
};

// Invalid lanes replaced by `ident`; `lm` is the block's lane mask, shared by both blends.
template <class T>
QUIVER_FORCE_INLINE typename RedFloatVec<T>::V
red_float_masked(typename RedFloatVec<T>::V v, typename RedFloatVec<T>::V ident,
                 typename RedFloatVec<T>::V lm, const std::uint8_t* validity) noexcept {
  return validity != nullptr ? RedFloatVec<T>::blend(ident, v, lm) : v;
}

template <class T, bool WantMin, bool WantMax>
QUIVER_FORCE_INLINE void red_float_step(RedFloatAcc<T>& acc, const T* in, std::int64_t i,
                                        const std::uint8_t* validity) noexcept {
  using F = RedFloatVec<T>;
  using V = typename F::V;
  const V v = F::load(in + i);
  const V lm = validity != nullptr ? F::mask_of(validity, i) : F::zero();
  // NaN detection must see only VALID lanes; masked lanes hold finite identities.
  const V vn = red_float_masked<T>(v, acc.idmin, lm, validity);
  acc.nans = F::or_bits(acc.nans, F::unord(vn));
  if constexpr (WantMin) {
    acc.min = F::min(acc.min, vn);
  }
  if constexpr (WantMax) {
    acc.max = F::max(acc.max, red_float_masked<T>(v, acc.idmax, lm, validity));
  }
}

template <class T, bool IsMin>
QUIVER_FORCE_INLINE T red_float_fold_lanes(typename RedFloatVec<T>::V acc) noexcept {
  using F = RedFloatVec<T>;
  alignas(32) T lanes[static_cast<std::size_t>(F::kW)];
  F::store(lanes, acc);
  T r = IsMin ? std::numeric_limits<T>::max() : std::numeric_limits<T>::lowest();
  for (std::int64_t k = 0; k < F::kW; ++k) {
    r = (IsMin ? (lanes[k] < r) : (lanes[k] > r)) ? lanes[k] : r;
  }
  return r;
}

template <class T>
struct RedFloatFold {
  MinMax<T> mm;
  bool saw_nan;
};

// Scalar tail: the reference fold rule plus the valid-only NaN watch.
template <class T, bool WantMin, bool WantMax>
QUIVER_FORCE_INLINE void red_float_tail(RedFloatFold<T>& f, const T* in, std::int64_t n,
                                        const std::uint8_t* validity) noexcept {
  constexpr std::int64_t kW = RedFloatVec<T>::kW;
  for (std::int64_t i = n / kW * kW; i < n; ++i) {
    if (is_valid(validity, i)) {
      const T v = in[i];
      f.saw_nan = f.saw_nan || (v != v);
      red_fold_one<T, WantMin, WantMax>(f.mm, v);
    }
  }
}

// ±0.0 is the only bit-visible tie left; recover the reference's first-encountered zero. An
// unwanted bound still holds its identity here, which is never 0.0, so it is left alone.
template <class T>
QUIVER_FORCE_INLINE void red_rescue_zero(MinMax<T>& mm, const T* in, std::int64_t n,
                                         const std::uint8_t* validity) noexcept {
  if (mm.min != T{0} && mm.max != T{0}) {
    return;
  }
  const T z = first_participating_zero(in, n, validity);
  mm.min = (mm.min == T{0}) ? z : mm.min;
  mm.max = (mm.max == T{0}) ? z : mm.max;
}

template <class T, bool WantMin, bool WantMax>
MinMax<T> dense_minmax_float(const T* in, std::int64_t n, const std::uint8_t* validity) noexcept {
  using F = RedFloatVec<T>;
  const MinMax<T> id = red_identity<T>();
  const typename F::V idmin = F::set1(id.min);
  const typename F::V idmax = F::set1(id.max);
  RedFloatAcc<T> acc{idmin, idmax, F::zero(), idmin, idmax};
  for (std::int64_t i = 0; i + F::kW <= n; i += F::kW) {
    red_float_step<T, WantMin, WantMax>(acc, in, i, validity);
  }
  RedFloatFold<T> f{
      {red_float_fold_lanes<T, true>(acc.min), red_float_fold_lanes<T, false>(acc.max)},
      F::any(acc.nans)};
  red_float_tail<T, WantMin, WantMax>(f, in, n, validity);
  if (f.saw_nan) {  // NaN propagation, payload-normalized (PRD 08 §3.3)
    return {scalar_impl::canonical_qnan<T>(), scalar_impl::canonical_qnan<T>()};
  }
  red_rescue_zero(f.mm, in, n, validity);
  return f.mm;
}

template <class T, bool WantMin, bool WantMax>
QUIVER_FORCE_INLINE MinMax<T> dense_minmax(const T* in, std::int64_t n,
                                           const std::uint8_t* validity) noexcept {
  if constexpr (std::is_floating_point_v<T>) {
    return dense_minmax_float<T, WantMin, WantMax>(in, n, validity);
  } else {
    return dense_minmax_int<T, WantMin, WantMax>(in, n, validity);
  }
}

// --- Integer dense wrapping sum -----------------------------------------------------------------

// Four elements widened to 64-bit lanes per step; invalid lanes AND to 0 (the wrap-neutral).
template <class T>
QUIVER_FORCE_INLINE __m256i widen4(const T* p) noexcept {
  constexpr bool kSigned = std::is_signed_v<T>;
  if constexpr (sizeof(T) == 8) {
    return _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p));
  } else if constexpr (sizeof(T) == 4) {
    const __m128i x = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p));
    return kSigned ? _mm256_cvtepi32_epi64(x) : _mm256_cvtepu32_epi64(x);
  } else if constexpr (sizeof(T) == 2) {
    const __m128i x = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(p));
    return kSigned ? _mm256_cvtepi16_epi64(x) : _mm256_cvtepu16_epi64(x);
  } else {
    std::uint32_t raw = 0;
    std::memcpy(&raw, p, 4);
    const __m128i x = _mm_cvtsi32_si128(static_cast<int>(raw));
    return kSigned ? _mm256_cvtepi8_epi64(x) : _mm256_cvtepu8_epi64(x);
  }
}

// Four widened elements with invalid lanes cleared to 0, the wrap-neutral.
template <class T>
QUIVER_FORCE_INLINE __m256i red_widen_masked(const T* p, const std::uint8_t* validity,
                                             std::int64_t i) noexcept {
  const __m256i w = widen4(p);
  if (validity == nullptr) {
    return w;
  }
  return _mm256_and_si256(w, lane_mask64(validity_bits<4>(validity, i)));
}

template <class T>
SumType<T> dense_sum_wrap_int(const T* in, std::int64_t n, const std::uint8_t* validity) noexcept {
  using S = SumType<T>;
  using U = std::make_unsigned_t<S>;
  __m256i acc = _mm256_setzero_si256();
  std::int64_t i = 0;
  for (; i + 4 <= n; i += 4) {
    // wrapping lanewise; commutative, so lane blocking is exact
    acc = _mm256_add_epi64(acc, red_widen_masked(in + i, validity, i));
  }
  alignas(32) U lanes[4];
  _mm256_storeu_si256(reinterpret_cast<__m256i*>(lanes), acc);
  U s = static_cast<U>(static_cast<U>(lanes[0] + lanes[1]) + static_cast<U>(lanes[2] + lanes[3]));
  for (; i < n; ++i) {
    if (is_valid(validity, i)) {
      s = static_cast<U>(s + static_cast<U>(static_cast<S>(in[i])));
    }
  }
  return static_cast<S>(s);
}

// --- Float dense sum: the ADR-013 AVX2 policy (see file comment) --------------------------------

// A = 4 independent vector accumulators (ADR-013). Masked lanes add -0.0, the exact neutral.
constexpr int kRedSumAccs = 4;

// One A-way block: each accumulator takes its own vector, valid lanes only.
template <class T>
QUIVER_FORCE_INLINE void red_sum_step(typename RedFloatVec<T>::V* acc, const T* in, std::int64_t i,
                                      const std::uint8_t* validity) noexcept {
  using F = RedFloatVec<T>;
  const typename F::V neg0 = F::set1(static_cast<T>(-0.0));
  for (int k = 0; k < kRedSumAccs; ++k) {
    const std::int64_t at = i + k * F::kW;
    const typename F::V lm = validity != nullptr ? F::mask_of(validity, at) : F::zero();
    acc[k] = F::add(acc[k], red_float_masked<T>(F::load(in + at), neg0, lm, validity));
  }
}

template <class T>
T dense_sum_float(const T* in, std::int64_t n, const std::uint8_t* validity) noexcept {
  using F = RedFloatVec<T>;
  constexpr std::int64_t kBlock = kRedSumAccs * F::kW;
  typename F::V acc[kRedSumAccs] = {F::zero(), F::zero(), F::zero(), F::zero()};
  std::int64_t i = 0;
  for (; i + kBlock <= n; i += kBlock) {
    red_sum_step<T>(acc, in, i, validity);
  }
  const typename F::V vsum =  // ADR-013 frozen combine: (0+2),(1+3), then +
      F::add(F::add(acc[0], acc[2]), F::add(acc[1], acc[3]));
  alignas(32) T lanes[static_cast<std::size_t>(F::kW)];
  F::store(lanes, vsum);
  T s = T{0};
  for (std::int64_t k = 0; k < F::kW; ++k) {  // strict low->high lane fold from +0.0
    s += lanes[k];
  }
  for (; i < n; ++i) {  // sequential valid-only tail (part of the documented policy)
    if (is_valid(validity, i)) {
      s += in[i];
    }
  }
  return s;
}

}  // namespace

// --- Concrete overloads (mirroring the scalar backend set; ADR-006). Selected shapes
// --- (sel != nullptr) delegate to the scalar core — see file comment. -----------------------

// NOLINTBEGIN(bugprone-macro-parentheses): T/S expand to type names inside declarators.
#define QUIVER_K6_MINMAX_SMA_DEFINE(T)                                                             \
  T k6_reduce_min(const T* in, std::int64_t n, const std::uint8_t* validity,                       \
                  const std::uint32_t* sel, std::int64_t sel_len) noexcept {                       \
    if (sel != nullptr) {                                                                          \
      return scalar_impl::reduce_min<T>(in, {n, validity, sel, sel_len});                          \
    }                                                                                              \
    return dense_minmax<T, true, false>(in, n, validity).min;                                      \
  }                                                                                                \
  T k6_reduce_max(const T* in, std::int64_t n, const std::uint8_t* validity,                       \
                  const std::uint32_t* sel, std::int64_t sel_len) noexcept {                       \
    if (sel != nullptr) {                                                                          \
      return scalar_impl::reduce_max<T>(in, {n, validity, sel, sel_len});                          \
    }                                                                                              \
    return dense_minmax<T, false, true>(in, n, validity).max;                                      \
  }                                                                                                \
  MinMaxSummary<T> k6_compute_sma(const T* in, std::int64_t n, const std::uint8_t* validity,       \
                                  const std::uint32_t* sel, std::int64_t sel_len) noexcept {       \
    if (sel != nullptr) {                                                                          \
      return scalar_impl::compute_sma<T>(in, {n, validity, sel, sel_len});                         \
    }                                                                                              \
    const MinMax<T> mm = dense_minmax<T, true, true>(in, n, validity);                             \
    return MinMaxSummary<T>{mm.min, mm.max, n - valid_count(validity, n)};                         \
  }

#define QUIVER_K6_INT_DEFINE(T, S)                                                                 \
  QUIVER_K6_MINMAX_SMA_DEFINE(T)                                                                   \
  S k6_reduce_sum_wrap(const T* in, std::int64_t n, const std::uint8_t* validity,                  \
                       const std::uint32_t* sel, std::int64_t sel_len) noexcept {                  \
    if (sel != nullptr) {                                                                          \
      return scalar_impl::reduce_sum_wrap<T>(in, {n, validity, sel, sel_len});                     \
    }                                                                                              \
    return dense_sum_wrap_int<T>(in, n, validity);                                                 \
  }                                                                                                \
  bool k6_reduce_sum_checked(const T* in, std::int64_t n, const std::uint8_t* validity,            \
                             const std::uint32_t* sel, std::int64_t sel_len,                       \
                             S* out_sum) noexcept {                                                \
    return scalar_impl::reduce_sum_checked<T>(in, {n, validity, sel, sel_len}, out_sum);           \
  }

#define QUIVER_K6_FLOAT_DEFINE(T)                                                                  \
  QUIVER_K6_MINMAX_SMA_DEFINE(T)                                                                   \
  T k6_reduce_sum_wrap(const T* in, std::int64_t n, const std::uint8_t* validity,                  \
                       const std::uint32_t* sel, std::int64_t sel_len) noexcept {                  \
    if (sel != nullptr) {                                                                          \
      return scalar_impl::reduce_sum_wrap<T>(in, {n, validity, sel, sel_len});                     \
    }                                                                                              \
    return dense_sum_float<T>(in, n, validity);                                                    \
  }

QUIVER_K6_INT_DEFINE(std::int8_t, std::int64_t)
QUIVER_K6_INT_DEFINE(std::int16_t, std::int64_t)
QUIVER_K6_INT_DEFINE(std::int32_t, std::int64_t)
QUIVER_K6_INT_DEFINE(std::int64_t, std::int64_t)
QUIVER_K6_INT_DEFINE(std::uint8_t, std::uint64_t)
QUIVER_K6_INT_DEFINE(std::uint16_t, std::uint64_t)
QUIVER_K6_INT_DEFINE(std::uint32_t, std::uint64_t)
QUIVER_K6_INT_DEFINE(std::uint64_t, std::uint64_t)
QUIVER_K6_FLOAT_DEFINE(float)
QUIVER_K6_FLOAT_DEFINE(double)
#undef QUIVER_K6_FLOAT_DEFINE
#undef QUIVER_K6_INT_DEFINE
#undef QUIVER_K6_MINMAX_SMA_DEFINE
// NOLINTEND(bugprone-macro-parentheses)

}  // namespace detail::avx2
QUIVER_TARGET_AVX2_END
QUIVER_END_NAMESPACE

#endif  // x86-64
