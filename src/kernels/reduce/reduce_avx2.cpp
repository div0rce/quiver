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
  const __m256i ctrl = _mm256_setr_epi8(0, -128, 0, -128, 0, -128, 0, -128, 0, -128, 0, -128,
                                        0, -128, 0, -128, 1, -128, 1, -128, 1, -128, 1, -128,
                                        1, -128, 1, -128, 1, -128, 1, -128);
  const __m256i bytes = _mm256_shuffle_epi8(v, ctrl);
  const __m256i bit =
      _mm256_setr_epi16(1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128);
  return _mm256_cmpeq_epi16(_mm256_and_si256(bytes, bit), bit);
}

QUIVER_FORCE_INLINE __m256i lane_mask8(std::uint32_t bits32) noexcept {
  // Byte lane g takes validity byte g/8, then tests bit weight g%8.
  const __m256i v = _mm256_set1_epi32(static_cast<int>(bits32));
  const __m256i ctrl = _mm256_setr_epi8(0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2,
                                        2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3);
  const __m256i bytes = _mm256_shuffle_epi8(v, ctrl);
  const __m256i bit = _mm256_setr_epi8(1, 2, 4, 8, 16, 32, 64, -128, 1, 2, 4, 8, 16, 32, 64,
                                       -128, 1, 2, 4, 8, 16, 32, 64, -128, 1, 2, 4, 8, 16, 32,
                                       64, -128);
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

template <class T, bool WantMin, bool WantMax>
MinMax<T> dense_minmax_int(const T* in, std::int64_t n, const std::uint8_t* validity) noexcept {
  constexpr std::int64_t kW = static_cast<std::int64_t>(32 / sizeof(T));
  constexpr T kIdMin = std::numeric_limits<T>::max();
  constexpr T kIdMax = std::numeric_limits<T>::lowest();
  T bmin = kIdMin;
  T bmax = kIdMax;
  std::int64_t i = 0;
  if (n >= kW) {
    const __m256i idmin = broadcast_int(kIdMin);
    const __m256i idmax = broadcast_int(kIdMax);
    __m256i accmin = idmin;
    __m256i accmax = idmax;
    for (; i + kW <= n; i += kW) {
      const __m256i v = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(in + i));
      __m256i lm{};
      if (validity != nullptr) {
        lm = lane_mask_for<T>(validity_bits<static_cast<int>(kW)>(validity, i));
      }
      if constexpr (WantMin) {
        const __m256i vm = validity != nullptr ? _mm256_blendv_epi8(idmin, v, lm) : v;
        accmin = int_minmax_step<T, true>(accmin, vm);
      }
      if constexpr (WantMax) {
        const __m256i vm = validity != nullptr ? _mm256_blendv_epi8(idmax, v, lm) : v;
        accmax = int_minmax_step<T, false>(accmax, vm);
      }
    }
    alignas(32) T lanes[kW];
    if constexpr (WantMin) {
      _mm256_storeu_si256(reinterpret_cast<__m256i*>(lanes), accmin);
      for (std::int64_t k = 0; k < kW; ++k) {
        bmin = (lanes[k] < bmin) ? lanes[k] : bmin;
      }
    }
    if constexpr (WantMax) {
      _mm256_storeu_si256(reinterpret_cast<__m256i*>(lanes), accmax);
      for (std::int64_t k = 0; k < kW; ++k) {
        bmax = (lanes[k] > bmax) ? lanes[k] : bmax;
      }
    }
  }
  for (; i < n; ++i) {  // scalar tail, same fold rule as the reference
    if (is_valid(validity, i)) {
      const T v = in[i];
      if constexpr (WantMin) {
        bmin = (v < bmin) ? v : bmin;
      }
      if constexpr (WantMax) {
        bmax = (v > bmax) ? v : bmax;
      }
    }
  }
  return {bmin, bmax};
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

template <class T, bool WantMin, bool WantMax>
MinMax<T> dense_minmax_float(const T* in, std::int64_t n,
                             const std::uint8_t* validity) noexcept {
  constexpr bool kF32 = std::is_same_v<T, float>;
  constexpr std::int64_t kW = kF32 ? 8 : 4;
  constexpr T kIdMin = std::numeric_limits<T>::max();
  constexpr T kIdMax = std::numeric_limits<T>::lowest();
  T bmin = kIdMin;
  T bmax = kIdMax;
  bool saw_nan = false;
  std::int64_t i = 0;
  if (n >= kW) {
    if constexpr (kF32) {
      const __m256 idmin = _mm256_set1_ps(kIdMin);
      const __m256 idmax = _mm256_set1_ps(kIdMax);
      __m256 accmin = idmin;
      __m256 accmax = idmax;
      __m256 nans = _mm256_setzero_ps();
      for (; i + kW <= n; i += kW) {
        const __m256 v = _mm256_loadu_ps(in + i);
        __m256 lm{};
        if (validity != nullptr) {
          lm = _mm256_castsi256_ps(lane_mask32(validity_bits<8>(validity, i)));
        }
        // NaN detection must see only VALID lanes; masked lanes hold finite identities.
        const __m256 vn = validity != nullptr ? _mm256_blendv_ps(idmin, v, lm) : v;
        nans = _mm256_or_ps(nans, _mm256_cmp_ps(vn, vn, _CMP_UNORD_Q));
        if constexpr (WantMin) {
          accmin = _mm256_min_ps(accmin, vn);
        }
        if constexpr (WantMax) {
          const __m256 vx = validity != nullptr ? _mm256_blendv_ps(idmax, v, lm) : v;
          accmax = _mm256_max_ps(accmax, vx);
        }
      }
      saw_nan = _mm256_movemask_ps(nans) != 0;
      alignas(32) T lanes[kW];
      if constexpr (WantMin) {
        _mm256_storeu_ps(lanes, accmin);
        for (std::int64_t k = 0; k < kW; ++k) {
          bmin = (lanes[k] < bmin) ? lanes[k] : bmin;
        }
      }
      if constexpr (WantMax) {
        _mm256_storeu_ps(lanes, accmax);
        for (std::int64_t k = 0; k < kW; ++k) {
          bmax = (lanes[k] > bmax) ? lanes[k] : bmax;
        }
      }
    } else {
      const __m256d idmin = _mm256_set1_pd(kIdMin);
      const __m256d idmax = _mm256_set1_pd(kIdMax);
      __m256d accmin = idmin;
      __m256d accmax = idmax;
      __m256d nans = _mm256_setzero_pd();
      for (; i + kW <= n; i += kW) {
        const __m256d v = _mm256_loadu_pd(in + i);
        __m256d lm{};
        if (validity != nullptr) {
          lm = _mm256_castsi256_pd(lane_mask64(validity_bits<4>(validity, i)));
        }
        const __m256d vn = validity != nullptr ? _mm256_blendv_pd(idmin, v, lm) : v;
        nans = _mm256_or_pd(nans, _mm256_cmp_pd(vn, vn, _CMP_UNORD_Q));
        if constexpr (WantMin) {
          accmin = _mm256_min_pd(accmin, vn);
        }
        if constexpr (WantMax) {
          const __m256d vx = validity != nullptr ? _mm256_blendv_pd(idmax, v, lm) : v;
          accmax = _mm256_max_pd(accmax, vx);
        }
      }
      saw_nan = _mm256_movemask_pd(nans) != 0;
      alignas(32) T lanes[kW];
      if constexpr (WantMin) {
        _mm256_storeu_pd(lanes, accmin);
        for (std::int64_t k = 0; k < kW; ++k) {
          bmin = (lanes[k] < bmin) ? lanes[k] : bmin;
        }
      }
      if constexpr (WantMax) {
        _mm256_storeu_pd(lanes, accmax);
        for (std::int64_t k = 0; k < kW; ++k) {
          bmax = (lanes[k] > bmax) ? lanes[k] : bmax;
        }
      }
    }
  }
  for (; i < n; ++i) {
    if (is_valid(validity, i)) {
      const T v = in[i];
      saw_nan = saw_nan || (v != v);
      if constexpr (WantMin) {
        bmin = (v < bmin) ? v : bmin;
      }
      if constexpr (WantMax) {
        bmax = (v > bmax) ? v : bmax;
      }
    }
  }
  if (saw_nan) {  // NaN propagation, payload-normalized (PRD 08 §3.3)
    return {scalar_impl::canonical_qnan<T>(), scalar_impl::canonical_qnan<T>()};
  }
  // ±0.0 is the only bit-visible tie; recover the reference's first-encountered zero.
  if constexpr (WantMin) {
    if (bmin == T{0}) {
      bmin = first_participating_zero(in, n, validity);
    }
  }
  if constexpr (WantMax) {
    if (bmax == T{0}) {
      bmax = first_participating_zero(in, n, validity);
    }
  }
  return {bmin, bmax};
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

template <class T>
SumType<T> dense_sum_wrap_int(const T* in, std::int64_t n,
                              const std::uint8_t* validity) noexcept {
  using S = SumType<T>;
  using U = std::make_unsigned_t<S>;
  __m256i acc = _mm256_setzero_si256();
  std::int64_t i = 0;
  for (; i + 4 <= n; i += 4) {
    __m256i w = widen4(in + i);
    if (validity != nullptr) {
      w = _mm256_and_si256(w, lane_mask64(validity_bits<4>(validity, i)));
    }
    acc = _mm256_add_epi64(acc, w);  // wrapping lanewise; commutative, so blocking is exact
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

template <class T>
T dense_sum_float(const T* in, std::int64_t n, const std::uint8_t* validity) noexcept {
  constexpr bool kF32 = std::is_same_v<T, float>;
  constexpr std::int64_t kW = kF32 ? 8 : 4;
  constexpr int kA = 4;
  T s = T{0};
  std::int64_t i = 0;
  if constexpr (kF32) {
    __m256 acc[kA] = {_mm256_setzero_ps(), _mm256_setzero_ps(), _mm256_setzero_ps(),
                      _mm256_setzero_ps()};
    const __m256 neg0 = _mm256_set1_ps(-0.0f);
    for (; i + kA * kW <= n; i += kA * kW) {
      for (int k = 0; k < kA; ++k) {
        __m256 v = _mm256_loadu_ps(in + i + k * kW);
        if (validity != nullptr) {
          const __m256 lm =
              _mm256_castsi256_ps(lane_mask32(validity_bits<8>(validity, i + k * kW)));
          v = _mm256_blendv_ps(neg0, v, lm);
        }
        acc[k] = _mm256_add_ps(acc[k], v);
      }
    }
    const __m256 vsum =  // ADR-013 frozen combine: (0+2),(1+3), then +
        _mm256_add_ps(_mm256_add_ps(acc[0], acc[2]), _mm256_add_ps(acc[1], acc[3]));
    alignas(32) T lanes[kW];
    _mm256_storeu_ps(lanes, vsum);
    for (std::int64_t k = 0; k < kW; ++k) {
      s += lanes[k];
    }
  } else {
    __m256d acc[kA] = {_mm256_setzero_pd(), _mm256_setzero_pd(), _mm256_setzero_pd(),
                       _mm256_setzero_pd()};
    const __m256d neg0 = _mm256_set1_pd(-0.0);
    for (; i + kA * kW <= n; i += kA * kW) {
      for (int k = 0; k < kA; ++k) {
        __m256d v = _mm256_loadu_pd(in + i + k * kW);
        if (validity != nullptr) {
          const __m256d lm =
              _mm256_castsi256_pd(lane_mask64(validity_bits<4>(validity, i + k * kW)));
          v = _mm256_blendv_pd(neg0, v, lm);
        }
        acc[k] = _mm256_add_pd(acc[k], v);
      }
    }
    const __m256d vsum =  // ADR-013 frozen combine: (0+2),(1+3), then +
        _mm256_add_pd(_mm256_add_pd(acc[0], acc[2]), _mm256_add_pd(acc[1], acc[3]));
    alignas(32) T lanes[kW];
    _mm256_storeu_pd(lanes, vsum);
    for (std::int64_t k = 0; k < kW; ++k) {
      s += lanes[k];
    }
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
#define QUIVER_K6_MINMAX_SMA_DEFINE(T)                                                        \
  T k6_reduce_min(const T* in, std::int64_t n, const std::uint8_t* validity,                 \
                  const std::uint32_t* sel, std::int64_t sel_len) noexcept {                 \
    if (sel != nullptr) {                                                                    \
      return scalar_impl::reduce_min<T>(in, n, validity, sel, sel_len);                      \
    }                                                                                        \
    return dense_minmax<T, true, false>(in, n, validity).min;                                \
  }                                                                                          \
  T k6_reduce_max(const T* in, std::int64_t n, const std::uint8_t* validity,                 \
                  const std::uint32_t* sel, std::int64_t sel_len) noexcept {                 \
    if (sel != nullptr) {                                                                    \
      return scalar_impl::reduce_max<T>(in, n, validity, sel, sel_len);                      \
    }                                                                                        \
    return dense_minmax<T, false, true>(in, n, validity).max;                                \
  }                                                                                          \
  Sma<T> k6_compute_sma(const T* in, std::int64_t n, const std::uint8_t* validity,           \
                        const std::uint32_t* sel, std::int64_t sel_len) noexcept {           \
    if (sel != nullptr) {                                                                    \
      return scalar_impl::compute_sma<T>(in, n, validity, sel, sel_len);                     \
    }                                                                                        \
    const MinMax<T> mm = dense_minmax<T, true, true>(in, n, validity);                       \
    return Sma<T>{mm.min, mm.max, n - valid_count(validity, n)};                             \
  }

#define QUIVER_K6_INT_DEFINE(T, S)                                                            \
  QUIVER_K6_MINMAX_SMA_DEFINE(T)                                                              \
  S k6_reduce_sum_wrap(const T* in, std::int64_t n, const std::uint8_t* validity,            \
                       const std::uint32_t* sel, std::int64_t sel_len) noexcept {            \
    if (sel != nullptr) {                                                                    \
      return scalar_impl::reduce_sum_wrap<T>(in, n, validity, sel, sel_len);                 \
    }                                                                                        \
    return dense_sum_wrap_int<T>(in, n, validity);                                          \
  }                                                                                          \
  bool k6_reduce_sum_checked(const T* in, std::int64_t n, const std::uint8_t* validity,      \
                             const std::uint32_t* sel, std::int64_t sel_len,                 \
                             S* out_sum) noexcept {                                          \
    return scalar_impl::reduce_sum_checked<T>(in, n, validity, sel, sel_len, out_sum);       \
  }

#define QUIVER_K6_FLOAT_DEFINE(T)                                                             \
  QUIVER_K6_MINMAX_SMA_DEFINE(T)                                                              \
  T k6_reduce_sum_wrap(const T* in, std::int64_t n, const std::uint8_t* validity,            \
                       const std::uint32_t* sel, std::int64_t sel_len) noexcept {            \
    if (sel != nullptr) {                                                                    \
      return scalar_impl::reduce_sum_wrap<T>(in, n, validity, sel, sel_len);                 \
    }                                                                                        \
    return dense_sum_float<T>(in, n, validity);                                             \
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
