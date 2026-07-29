// K6 reduce / SMA — NEON backend (PRD 08 §5 K6; ADR-013). Dense (sel == nullptr) shapes are
// vectorized with validity lane-mask expansion; selected shapes delegate to the scalar core
// (random-access dominated; the NEON backend's selected float sum is therefore the strict
// sequential fold — documented per-backend policy, ADR-013).
//
// min/max: FLOATS use native vector min/max plus the same two exactness rescues as the AVX2
// backend: any valid NaN forces the canonical qNaN return (PRD 08 §3.3), and a folded result
// equal to 0.0 triggers a rescan for the first participating zero (±0.0 is the only bit-visible
// float tie). The explicit float path beats the strict-order baseline 4.0x on Apple M2 (the
// compiler cannot reassociate float min/max past NaN semantics). INTEGER dense min/max/SMA
// instead delegates to the autovectorized scalar reference (REQ-KERNEL-007): integer min is
// associative-exact, the compiler reassociates it into a multi-accumulator loop, and that loop
// beat the handwritten single-accumulator chain 3.7-3.9x (ledger 20260710-2339ddc1554b /
// -e98f97623a54; docs/api/reduce.md has the verdict). Lane order is irrelevant either way.
//
// Integer sums: the PRD's staged pairwise-widening technique — per 128-bit vector,
// vpaddlq chains widen to 64-bit lanes exactly (no intermediate can overflow), then a
// wrapping 64-bit accumulate; wrapping addition is commutative, so blocking is exact.
// Masked lanes AND to zero at source width before widening. Checked sums delegate to the
// scalar 128-bit core.
//
// FLOAT SUM POLICY (ADR-013, NEON): A=4 vector accumulators × W lanes (f32 W=4, f64 W=2),
// masked lanes add -0.0 (the exact neutral); combine = the frozen pairwise order (0+2),
// (1+3), then +, lanewise; strict low->high lane fold from +0.0; sequential valid-only tail.
// Mirrored exactly by the testkit blocked-sum oracle for non-NaN results (REQ-TEST-004);
// NaN results compare as a class (payloads follow hardware operand order — gate M4).
// Module: MOD-K6-REDUCE | REQs: REQ-K6-001..005, REQ-SIMD-001..003/-008 | ADR-003, ADR-013
#include "src/kernels/reduce/reduce_scalar_impl.h"

#if defined(__aarch64__) || defined(_M_ARM64)

#include <arm_neon.h>
#include <bit>
#include <cstring>
#include <limits>
#include <type_traits>

QUIVER_BEGIN_NAMESPACE
namespace detail::neon {

namespace {

// --- Validity lane-mask expansion (PRD 08 K6 technique) --------------------------------------

// W validity bits for the W-element group starting at element i (i is W-aligned).
template <int W>
QUIVER_FORCE_INLINE std::uint32_t validity_bits(const std::uint8_t* v, std::int64_t i) noexcept {
  if constexpr (W == 2) {
    return (static_cast<std::uint32_t>(v[i >> 3]) >> (i & 6)) & 0x3u;
  } else if constexpr (W == 4) {
    return (static_cast<std::uint32_t>(v[i >> 3]) >> (i & 4)) & 0xFu;
  } else {
    std::uint32_t x = 0;
    std::memcpy(&x, v + (i >> 3), W / 8);
    return x;
  }
}

QUIVER_FORCE_INLINE uint8x16_t lane_mask_u8(std::uint32_t bits16) noexcept {
  const uint8x16_t v = vcombine_u8(vdup_n_u8(static_cast<std::uint8_t>(bits16 & 0xFFu)),
                                   vdup_n_u8(static_cast<std::uint8_t>(bits16 >> 8)));
  const uint8x16_t w = {1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128};
  return vceqq_u8(vandq_u8(v, w), w);
}
QUIVER_FORCE_INLINE uint16x8_t lane_mask_u16(std::uint32_t byte) noexcept {
  const uint16x8_t w = {1, 2, 4, 8, 16, 32, 64, 128};
  return vceqq_u16(vandq_u16(vdupq_n_u16(static_cast<std::uint16_t>(byte)), w), w);
}
QUIVER_FORCE_INLINE uint32x4_t lane_mask_u32(std::uint32_t nibble) noexcept {
  const uint32x4_t w = {1, 2, 4, 8};
  return vceqq_u32(vandq_u32(vdupq_n_u32(nibble), w), w);
}
QUIVER_FORCE_INLINE uint64x2_t lane_mask_u64(std::uint32_t pair) noexcept {
  const uint64x2_t w = {1, 2};
  return vceqq_u64(vandq_u64(vdupq_n_u64(pair), w), w);
}

// Participating (selected ∧ valid) count for the dense shape.
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

// --- Type-dispatched vector plumbing ----------------------------------------------------------

template <class T>
struct red_VecOf;
template <>
struct red_VecOf<std::int8_t> {
  using type = int8x16_t;
};
template <>
struct red_VecOf<std::uint8_t> {
  using type = uint8x16_t;
};
template <>
struct red_VecOf<std::int16_t> {
  using type = int16x8_t;
};
template <>
struct red_VecOf<std::uint16_t> {
  using type = uint16x8_t;
};
template <>
struct red_VecOf<std::int32_t> {
  using type = int32x4_t;
};
template <>
struct red_VecOf<std::uint32_t> {
  using type = uint32x4_t;
};
template <>
struct red_VecOf<std::int64_t> {
  using type = int64x2_t;
};
template <>
struct red_VecOf<std::uint64_t> {
  using type = uint64x2_t;
};
template <>
struct red_VecOf<float> {
  using type = float32x4_t;
};
template <>
struct red_VecOf<double> {
  using type = float64x2_t;
};
template <class T>
using red_Vec = typename red_VecOf<T>::type;

template <class T>
constexpr std::int64_t red_vec_lanes() noexcept {
  return static_cast<std::int64_t>(16 / sizeof(T));
}

template <class T>
QUIVER_FORCE_INLINE red_Vec<T> red_load_vec(const T* p) noexcept {
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
QUIVER_FORCE_INLINE red_Vec<T> red_broadcast(T v) noexcept {
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

// Lane mask (at T's width) for the vector-sized group at element i.
template <class T>
QUIVER_FORCE_INLINE auto group_mask(const std::uint8_t* validity, std::int64_t i) noexcept {
  if constexpr (sizeof(T) == 1) {
    return lane_mask_u8(validity_bits<16>(validity, i));
  } else if constexpr (sizeof(T) == 2) {
    return lane_mask_u16(validity_bits<8>(validity, i));
  } else if constexpr (sizeof(T) == 4) {
    return lane_mask_u32(validity_bits<4>(validity, i));
  } else {
    return lane_mask_u64(validity_bits<2>(validity, i));
  }
}

// Byte-level blend (mask lanes are all-ones/all-zero, so vbslq_u8 is width-agnostic).
// memcpy bit-copies replace per-type vreinterpretq chains; they compile to nothing.
template <class T, class M>
QUIVER_FORCE_INLINE red_Vec<T> blend(M lane_mask, red_Vec<T> on_true,
                                     red_Vec<T> on_false) noexcept {
  uint8x16_t m;
  uint8x16_t t;
  uint8x16_t f;
  std::memcpy(&m, &lane_mask, 16);
  std::memcpy(&t, &on_true, 16);
  std::memcpy(&f, &on_false, 16);
  const uint8x16_t r = vbslq_u8(m, t, f);
  red_Vec<T> out;
  std::memcpy(&out, &r, 16);
  return out;
}

// --- Dense min/max ------------------------------------------------------------------------------

template <class T, bool IsMin>
QUIVER_FORCE_INLINE red_Vec<T> minmax_step(red_Vec<T> acc, red_Vec<T> v) noexcept {
  if constexpr (std::is_same_v<T, std::int8_t>) {
    return IsMin ? vminq_s8(acc, v) : vmaxq_s8(acc, v);
  } else if constexpr (std::is_same_v<T, std::uint8_t>) {
    return IsMin ? vminq_u8(acc, v) : vmaxq_u8(acc, v);
  } else if constexpr (std::is_same_v<T, std::int16_t>) {
    return IsMin ? vminq_s16(acc, v) : vmaxq_s16(acc, v);
  } else if constexpr (std::is_same_v<T, std::uint16_t>) {
    return IsMin ? vminq_u16(acc, v) : vmaxq_u16(acc, v);
  } else if constexpr (std::is_same_v<T, std::int32_t>) {
    return IsMin ? vminq_s32(acc, v) : vmaxq_s32(acc, v);
  } else if constexpr (std::is_same_v<T, std::uint32_t>) {
    return IsMin ? vminq_u32(acc, v) : vmaxq_u32(acc, v);
  } else if constexpr (std::is_same_v<T, std::int64_t>) {
    const uint64x2_t gt = vcgtq_s64(acc, v);  // no 64-bit min/max: compare + blend
    return IsMin ? blend<T>(gt, v, acc) : blend<T>(gt, acc, v);
  } else if constexpr (std::is_same_v<T, std::uint64_t>) {
    const uint64x2_t gt = vcgtq_u64(acc, v);
    return IsMin ? blend<T>(gt, v, acc) : blend<T>(gt, acc, v);
  } else if constexpr (std::is_same_v<T, float>) {
    return IsMin ? vminq_f32(acc, v) : vmaxq_f32(acc, v);
  } else {
    return IsMin ? vminq_f64(acc, v) : vmaxq_f64(acc, v);
  }
}

template <class T>
struct MinMax {
  T min;
  T max;
};

// First participating value comparing equal to 0.0 — exactly the reference's result whenever
// its fold ends on a zero (±0.0 is the only bit-visible float tie; see the AVX2 backend).
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
MinMax<T> dense_minmax(const T* in, std::int64_t n, const std::uint8_t* validity) noexcept {
  constexpr std::int64_t kW = red_vec_lanes<T>();
  constexpr T kIdMin = std::numeric_limits<T>::max();
  constexpr T kIdMax = std::numeric_limits<T>::lowest();
  T bmin = kIdMin;
  T bmax = kIdMax;
  bool saw_nan = false;
  std::int64_t i = 0;
  if (n >= kW) {
    const red_Vec<T> idmin = red_broadcast(kIdMin);
    const red_Vec<T> idmax = red_broadcast(kIdMax);
    red_Vec<T> accmin = idmin;
    red_Vec<T> accmax = idmax;
    [[maybe_unused]] uint8x16_t nans = vdupq_n_u8(0);
    for (; i + kW <= n; i += kW) {
      const red_Vec<T> v = red_load_vec(in + i);
      red_Vec<T> vn = v;
      red_Vec<T> vx = v;
      if (validity != nullptr) {
        const auto lm = group_mask<T>(validity, i);
        vn = blend<T>(lm, v, idmin);
        if constexpr (WantMax) {
          vx = blend<T>(lm, v, idmax);
        }
      }
      if constexpr (std::is_floating_point_v<T>) {
        // NaN detection sees only VALID lanes (masked lanes hold finite identities).
        if constexpr (std::is_same_v<T, float>) {
          nans = vorrq_u8(nans, vreinterpretq_u8_u32(vmvnq_u32(vceqq_f32(vn, vn))));
        } else {
          const uint64x2_t eq = vceqq_f64(vn, vn);
          uint8x16_t eqb;
          std::memcpy(&eqb, &eq, 16);
          nans = vorrq_u8(nans, vmvnq_u8(eqb));
        }
      }
      if constexpr (WantMin) {
        accmin = minmax_step<T, true>(accmin, vn);
      }
      if constexpr (WantMax) {
        accmax = minmax_step<T, false>(accmax, vx);
      }
    }
    if constexpr (std::is_floating_point_v<T>) {
      saw_nan = vmaxvq_u8(nans) != 0;
    }
    alignas(16) T lanes[static_cast<std::size_t>(kW)];
    if constexpr (WantMin) {
      std::memcpy(lanes, &accmin, sizeof(accmin));
      for (std::int64_t k = 0; k < kW; ++k) {
        bmin = (lanes[k] < bmin) ? lanes[k] : bmin;
      }
    }
    if constexpr (WantMax) {
      std::memcpy(lanes, &accmax, sizeof(accmax));
      for (std::int64_t k = 0; k < kW; ++k) {
        bmax = (lanes[k] > bmax) ? lanes[k] : bmax;
      }
    }
  }
  for (; i < n; ++i) {  // scalar tail, same fold rule as the reference
    if (is_valid(validity, i)) {
      const T v = in[i];
      if constexpr (std::is_floating_point_v<T>) {
        saw_nan = saw_nan || (v != v);
      }
      if constexpr (WantMin) {
        bmin = (v < bmin) ? v : bmin;
      }
      if constexpr (WantMax) {
        bmax = (v > bmax) ? v : bmax;
      }
    }
  }
  if constexpr (std::is_floating_point_v<T>) {
    if (saw_nan) {  // NaN propagation, payload-normalized (PRD 08 §3.3)
      return {scalar_impl::canonical_qnan<T>(), scalar_impl::canonical_qnan<T>()};
    }
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
  }
  return {bmin, bmax};
}

// --- Dense wrapping integer sum: staged pairwise widening (vpaddl chains, PRD 08 K6) ---------

template <class T>
SumType<T> dense_sum_wrap_int(const T* in, std::int64_t n, const std::uint8_t* validity) noexcept {
  using S = SumType<T>;
  using U = std::make_unsigned_t<S>;
  constexpr std::int64_t kW = red_vec_lanes<T>();
  constexpr bool kSigned = std::is_signed_v<T>;
  // One vector's worth of elements, fully widened to 64-bit lanes (exact — no intermediate
  // stage can overflow; the 64-bit wrap IS the spec).
  const auto widened = [&](std::int64_t at) -> uint64x2_t {
    red_Vec<T> v = red_load_vec(in + at);
    if (validity != nullptr) {
      const auto lm = group_mask<T>(validity, at);
      v = blend<T>(lm, v, red_broadcast(T{0}));
    }
    if constexpr (sizeof(T) == 1) {
      if constexpr (kSigned) {
        return vreinterpretq_u64_s64(vpaddlq_s32(vpaddlq_s16(vpaddlq_s8(v))));
      } else {
        return vpaddlq_u32(vpaddlq_u16(vpaddlq_u8(v)));
      }
    } else if constexpr (sizeof(T) == 2) {
      if constexpr (kSigned) {
        return vreinterpretq_u64_s64(vpaddlq_s32(vpaddlq_s16(v)));
      } else {
        return vpaddlq_u32(vpaddlq_u16(v));
      }
    } else if constexpr (sizeof(T) == 4) {
      if constexpr (kSigned) {
        return vreinterpretq_u64_s64(vpaddlq_s32(v));
      } else {
        return vpaddlq_u32(v);
      }
    } else {
      if constexpr (kSigned) {
        return vreinterpretq_u64_s64(v);
      } else {
        return v;
      }
    }
  };
  // Four independent accumulators (REQ-SIMD-008: >=4 128-bit ops in flight — the first
  // ledger run measured the single-accumulator loop ~4x behind autovec at nulls=0);
  // wrapping addition is commutative, so any blocking is bit-exact.
  uint64x2_t acc0 = vdupq_n_u64(0);
  uint64x2_t acc1 = vdupq_n_u64(0);
  uint64x2_t acc2 = vdupq_n_u64(0);
  uint64x2_t acc3 = vdupq_n_u64(0);
  std::int64_t i = 0;
  for (; i + 4 * kW <= n; i += 4 * kW) {
    acc0 = vaddq_u64(acc0, widened(i));
    acc1 = vaddq_u64(acc1, widened(i + kW));
    acc2 = vaddq_u64(acc2, widened(i + 2 * kW));
    acc3 = vaddq_u64(acc3, widened(i + 3 * kW));
  }
  for (; i + kW <= n; i += kW) {
    acc0 = vaddq_u64(acc0, widened(i));
  }
  const uint64x2_t acc = vaddq_u64(vaddq_u64(acc0, acc1), vaddq_u64(acc2, acc3));
  U s = static_cast<U>(vgetq_lane_u64(acc, 0) + vgetq_lane_u64(acc, 1));
  for (; i < n; ++i) {
    if (is_valid(validity, i)) {
      s = static_cast<U>(s + static_cast<U>(static_cast<S>(in[i])));
    }
  }
  return static_cast<S>(s);
}

// --- Dense float sum: the ADR-013 NEON policy (see file comment) ------------------------------

template <class T>
T dense_sum_float(const T* in, std::int64_t n, const std::uint8_t* validity) noexcept {
  constexpr std::int64_t kW = red_vec_lanes<T>();  // f32: 4, f64: 2
  constexpr int kA = 4;
  T s = T{0};
  std::int64_t i = 0;
  red_Vec<T> acc[kA] = {red_broadcast(T{0}), red_broadcast(T{0}), red_broadcast(T{0}),
                        red_broadcast(T{0})};
  const red_Vec<T> neg0 = red_broadcast(T{-0.0});
  for (; i + kA * kW <= n; i += kA * kW) {
    for (int k = 0; k < kA; ++k) {
      red_Vec<T> v = red_load_vec(in + i + k * kW);
      if (validity != nullptr) {
        const auto lm = group_mask<T>(validity, i + k * kW);
        v = blend<T>(lm, v, neg0);
      }
      if constexpr (std::is_same_v<T, float>) {
        acc[k] = vaddq_f32(acc[k], v);
      } else {
        acc[k] = vaddq_f64(acc[k], v);
      }
    }
  }
  red_Vec<T> vsum;
  if constexpr (std::is_same_v<T, float>) {  // ADR-013 frozen combine: (0+2),(1+3), then +
    vsum = vaddq_f32(vaddq_f32(acc[0], acc[2]), vaddq_f32(acc[1], acc[3]));
  } else {
    vsum = vaddq_f64(vaddq_f64(acc[0], acc[2]), vaddq_f64(acc[1], acc[3]));
  }
  alignas(16) T lanes[static_cast<std::size_t>(kW)];
  std::memcpy(lanes, &vsum, sizeof(vsum));
  for (std::int64_t k = 0; k < kW; ++k) {  // strict low->high lane fold from +0.0
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
// INTEGER dense min/max/SMA delegates to the autovectorized scalar reference (REQ-KERNEL-007):
// integer min is associative-exact, so the compiler reassociates it into a multi-accumulator
// vector loop that beat the handwritten single-accumulator chain 3.7-3.9x on Apple M2 (ledger
// run 20260710-2339ddc1554b: i64 0.27x dense / 0.67x masked; i32 0.26x). Floats stay on the
// handwritten path: the compiler cannot reassociate float min/max past NaN semantics, and the
// explicit f64 path measured a 4.0x WIN over that strict baseline. See docs/api/reduce.md.
#define QUIVER_K6_MINMAX_SMA_DEFINE(T)                                                             \
  T k6_reduce_min(const T* in, std::int64_t n, const std::uint8_t* validity,                       \
                  const std::uint32_t* sel, std::int64_t sel_len) noexcept {                       \
    if constexpr (std::is_integral_v<T>) {                                                         \
      return scalar_impl::reduce_min<T>(in, {n, validity, sel, sel_len});                          \
    } else {                                                                                       \
      if (sel != nullptr) {                                                                        \
        return scalar_impl::reduce_min<T>(in, {n, validity, sel, sel_len});                        \
      }                                                                                            \
      return dense_minmax<T, true, false>(in, n, validity).min;                                    \
    }                                                                                              \
  }                                                                                                \
  T k6_reduce_max(const T* in, std::int64_t n, const std::uint8_t* validity,                       \
                  const std::uint32_t* sel, std::int64_t sel_len) noexcept {                       \
    if constexpr (std::is_integral_v<T>) {                                                         \
      return scalar_impl::reduce_max<T>(in, {n, validity, sel, sel_len});                          \
    } else {                                                                                       \
      if (sel != nullptr) {                                                                        \
        return scalar_impl::reduce_max<T>(in, {n, validity, sel, sel_len});                        \
      }                                                                                            \
      return dense_minmax<T, false, true>(in, n, validity).max;                                    \
    }                                                                                              \
  }                                                                                                \
  MinMaxSummary<T> k6_compute_sma(const T* in, std::int64_t n, const std::uint8_t* validity,       \
                                  const std::uint32_t* sel, std::int64_t sel_len) noexcept {       \
    if constexpr (std::is_integral_v<T>) {                                                         \
      return scalar_impl::compute_sma<T>(in, {n, validity, sel, sel_len});                         \
    } else {                                                                                       \
      if (sel != nullptr) {                                                                        \
        return scalar_impl::compute_sma<T>(in, {n, validity, sel, sel_len});                       \
      }                                                                                            \
      const MinMax<T> mm = dense_minmax<T, true, true>(in, n, validity);                           \
      return MinMaxSummary<T>{mm.min, mm.max, n - valid_count(validity, n)};                       \
    }                                                                                              \
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

}  // namespace detail::neon
QUIVER_END_NAMESPACE

#endif  // aarch64
