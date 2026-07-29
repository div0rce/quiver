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

// One element type's NEON vector operations. Grouping them by type — rather than repeating a
// ten-way std::is_same_v ladder inside every operation — keeps each operation one expression.
template <class T>
struct RedOps;

#define QUIVER_RED_NEON_OPS(T, SUF)                                                                \
  template <>                                                                                      \
  struct RedOps<T> {                                                                               \
    static QUIVER_FORCE_INLINE red_Vec<T> load(const T* p) noexcept {                              \
      return vld1q_##SUF(p);                                                                       \
    }                                                                                              \
    static QUIVER_FORCE_INLINE red_Vec<T> broadcast(T v) noexcept {                                \
      return vdupq_n_##SUF(v);                                                                     \
    }                                                                                              \
    static QUIVER_FORCE_INLINE red_Vec<T> add(red_Vec<T> a, red_Vec<T> b) noexcept {               \
      return vaddq_##SUF(a, b);                                                                    \
    }                                                                                              \
    static QUIVER_FORCE_INLINE red_Vec<T> min(red_Vec<T> a, red_Vec<T> b) noexcept {               \
      return vminq_##SUF(a, b);                                                                    \
    }                                                                                              \
    static QUIVER_FORCE_INLINE red_Vec<T> max(red_Vec<T> a, red_Vec<T> b) noexcept {               \
      return vmaxq_##SUF(a, b);                                                                    \
    }                                                                                              \
  };

QUIVER_RED_NEON_OPS(std::int8_t, s8)
QUIVER_RED_NEON_OPS(std::uint8_t, u8)
QUIVER_RED_NEON_OPS(std::int16_t, s16)
QUIVER_RED_NEON_OPS(std::uint16_t, u16)
QUIVER_RED_NEON_OPS(std::int32_t, s32)
QUIVER_RED_NEON_OPS(std::uint32_t, u32)
QUIVER_RED_NEON_OPS(float, f32)
QUIVER_RED_NEON_OPS(double, f64)
#undef QUIVER_RED_NEON_OPS

// 64-bit lanes have no vminq/vmaxq: compare then blend.
#define QUIVER_RED_NEON_OPS64(T, SUF)                                                              \
  template <>                                                                                      \
  struct RedOps<T> {                                                                               \
    static QUIVER_FORCE_INLINE red_Vec<T> load(const T* p) noexcept {                              \
      return vld1q_##SUF(p);                                                                       \
    }                                                                                              \
    static QUIVER_FORCE_INLINE red_Vec<T> broadcast(T v) noexcept {                                \
      return vdupq_n_##SUF(v);                                                                     \
    }                                                                                              \
    static QUIVER_FORCE_INLINE red_Vec<T> add(red_Vec<T> a, red_Vec<T> b) noexcept {               \
      return vaddq_##SUF(a, b);                                                                    \
    }                                                                                              \
    static QUIVER_FORCE_INLINE red_Vec<T> min(red_Vec<T> a, red_Vec<T> b) noexcept {               \
      return blend<T>(vcgtq_##SUF(a, b), b, a);                                                    \
    }                                                                                              \
    static QUIVER_FORCE_INLINE red_Vec<T> max(red_Vec<T> a, red_Vec<T> b) noexcept {               \
      return blend<T>(vcgtq_##SUF(a, b), a, b);                                                    \
    }                                                                                              \
  };

QUIVER_RED_NEON_OPS64(std::int64_t, s64)
QUIVER_RED_NEON_OPS64(std::uint64_t, u64)
#undef QUIVER_RED_NEON_OPS64

// --- Dense min/max ------------------------------------------------------------------------------

template <class T, bool IsMin>
QUIVER_FORCE_INLINE red_Vec<T> minmax_step(red_Vec<T> acc, red_Vec<T> v) noexcept {
  return IsMin ? RedOps<T>::min(acc, v) : RedOps<T>::max(acc, v);
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

// Live min/max accumulators, the running NaN disjunction, and the blend identities.
template <class T>
struct RedAcc {
  red_Vec<T> min;
  red_Vec<T> max;
  uint8x16_t nans;
  red_Vec<T> idmin;
  red_Vec<T> idmax;
};

// NaN lanes of an already valid-masked vector, OR'd into the running disjunction.
template <class T>
QUIVER_FORCE_INLINE uint8x16_t red_nan_bits(uint8x16_t nans, red_Vec<T> vn) noexcept {
  if constexpr (std::is_same_v<T, float>) {
    return vorrq_u8(nans, vreinterpretq_u8_u32(vmvnq_u32(vceqq_f32(vn, vn))));
  } else {
    const uint64x2_t eq = vceqq_f64(vn, vn);
    uint8x16_t eqb;
    std::memcpy(&eqb, &eq, 16);
    return vorrq_u8(nans, vmvnq_u8(eqb));
  }
}

template <class T, bool WantMin, bool WantMax>
QUIVER_FORCE_INLINE void red_minmax_step(RedAcc<T>& acc, const T* in, std::int64_t i,
                                         const std::uint8_t* validity) noexcept {
  const red_Vec<T> v = RedOps<T>::load(in + i);
  red_Vec<T> vn = v;
  red_Vec<T> vx = v;
  if (validity != nullptr) {
    const auto lm = group_mask<T>(validity, i);
    vn = blend<T>(lm, v, acc.idmin);
    if constexpr (WantMax) {
      vx = blend<T>(lm, v, acc.idmax);
    }
  }
  if constexpr (std::is_floating_point_v<T>) {
    // NaN detection sees only VALID lanes (masked lanes hold finite identities).
    acc.nans = red_nan_bits<T>(acc.nans, vn);
  }
  if constexpr (WantMin) {
    acc.min = minmax_step<T, true>(acc.min, vn);
  }
  if constexpr (WantMax) {
    acc.max = minmax_step<T, false>(acc.max, vx);
  }
}

// Horizontal fold of one accumulator. Masked-off and never-written lanes hold the identity, so
// folding an untouched accumulator yields the identity itself.
template <class T, bool IsMin>
QUIVER_FORCE_INLINE T red_fold_lanes(red_Vec<T> acc) noexcept {
  constexpr std::int64_t kW = red_vec_lanes<T>();
  alignas(16) T lanes[static_cast<std::size_t>(kW)];
  std::memcpy(lanes, &acc, sizeof(acc));
  T r = IsMin ? std::numeric_limits<T>::max() : std::numeric_limits<T>::lowest();
  for (std::int64_t k = 0; k < kW; ++k) {
    r = (IsMin ? (lanes[k] < r) : (lanes[k] > r)) ? lanes[k] : r;
  }
  return r;
}

// One participating element folded in; the Want* selection lives here, at depth 1.
template <class T, bool WantMin, bool WantMax>
QUIVER_FORCE_INLINE void red_fold_one(MinMax<T>& mm, T v) noexcept {
  if constexpr (WantMin) {
    mm.min = (v < mm.min) ? v : mm.min;
  }
  if constexpr (WantMax) {
    mm.max = (v > mm.max) ? v : mm.max;
  }
}

template <class T>
struct RedFold {
  MinMax<T> mm;
  bool saw_nan;
};

// Scalar tail: same fold rule as the reference, plus the valid-only NaN watch.
template <class T, bool WantMin, bool WantMax>
QUIVER_FORCE_INLINE void red_scalar_tail(RedFold<T>& f, const T* in, std::int64_t n,
                                         const std::uint8_t* validity) noexcept {
  constexpr std::int64_t kW = red_vec_lanes<T>();
  for (std::int64_t i = n / kW * kW; i < n; ++i) {
    if (is_valid(validity, i)) {
      const T v = in[i];
      if constexpr (std::is_floating_point_v<T>) {
        f.saw_nan = f.saw_nan || (v != v);
      }
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
MinMax<T> dense_minmax(const T* in, std::int64_t n, const std::uint8_t* validity) noexcept {
  constexpr std::int64_t kW = red_vec_lanes<T>();
  const red_Vec<T> idmin = RedOps<T>::broadcast(std::numeric_limits<T>::max());
  const red_Vec<T> idmax = RedOps<T>::broadcast(std::numeric_limits<T>::lowest());
  RedAcc<T> acc{idmin, idmax, vdupq_n_u8(0), idmin, idmax};
  for (std::int64_t i = 0; i + kW <= n; i += kW) {
    red_minmax_step<T, WantMin, WantMax>(acc, in, i, validity);
  }
  RedFold<T> f{{red_fold_lanes<T, true>(acc.min), red_fold_lanes<T, false>(acc.max)}, false};
  if constexpr (std::is_floating_point_v<T>) {
    f.saw_nan = vmaxvq_u8(acc.nans) != 0;
  }
  red_scalar_tail<T, WantMin, WantMax>(f, in, n, validity);
  if constexpr (std::is_floating_point_v<T>) {
    if (f.saw_nan) {  // NaN propagation, payload-normalized (PRD 08 §3.3)
      return {scalar_impl::canonical_qnan<T>(), scalar_impl::canonical_qnan<T>()};
    }
    red_rescue_zero(f.mm, in, n, validity);
  }
  return f.mm;
}

// --- Dense wrapping integer sum: staged pairwise widening (vpaddl chains, PRD 08 K6) ---------

// Staged pairwise widening to 64-bit lanes. Exact: no intermediate stage can overflow, and the
// 64-bit wrap IS the spec.
template <class T>
QUIVER_FORCE_INLINE uint64x2_t red_widen_signed(red_Vec<T> v) noexcept {
  if constexpr (sizeof(T) == 1) {
    return vreinterpretq_u64_s64(vpaddlq_s32(vpaddlq_s16(vpaddlq_s8(v))));
  } else if constexpr (sizeof(T) == 2) {
    return vreinterpretq_u64_s64(vpaddlq_s32(vpaddlq_s16(v)));
  } else if constexpr (sizeof(T) == 4) {
    return vreinterpretq_u64_s64(vpaddlq_s32(v));
  } else {
    return vreinterpretq_u64_s64(v);
  }
}

template <class T>
QUIVER_FORCE_INLINE uint64x2_t red_widen_unsigned(red_Vec<T> v) noexcept {
  if constexpr (sizeof(T) == 1) {
    return vpaddlq_u32(vpaddlq_u16(vpaddlq_u8(v)));
  } else if constexpr (sizeof(T) == 2) {
    return vpaddlq_u32(vpaddlq_u16(v));
  } else if constexpr (sizeof(T) == 4) {
    return vpaddlq_u32(v);
  } else {
    return v;
  }
}

// One vector's worth of elements, invalid lanes cleared to 0 (the wrap-neutral), widened.
template <class T>
QUIVER_FORCE_INLINE uint64x2_t red_widen_at(const T* in, std::int64_t at,
                                            const std::uint8_t* validity) noexcept {
  red_Vec<T> v = RedOps<T>::load(in + at);
  if (validity != nullptr) {
    v = blend<T>(group_mask<T>(validity, at), v, RedOps<T>::broadcast(T{0}));
  }
  if constexpr (std::is_signed_v<T>) {
    return red_widen_signed<T>(v);
  } else {
    return red_widen_unsigned<T>(v);
  }
}

template <class T>
SumType<T> dense_sum_wrap_int(const T* in, std::int64_t n, const std::uint8_t* validity) noexcept {
  using S = SumType<T>;
  using U = std::make_unsigned_t<S>;
  constexpr std::int64_t kW = red_vec_lanes<T>();
  // Four independent accumulators (REQ-SIMD-008: >=4 128-bit ops in flight — the first
  // ledger run measured the single-accumulator loop ~4x behind autovec at nulls=0);
  // wrapping addition is commutative, so any blocking is bit-exact.
  uint64x2_t acc0 = vdupq_n_u64(0);
  uint64x2_t acc1 = vdupq_n_u64(0);
  uint64x2_t acc2 = vdupq_n_u64(0);
  uint64x2_t acc3 = vdupq_n_u64(0);
  std::int64_t i = 0;
  for (; i + 4 * kW <= n; i += 4 * kW) {
    acc0 = vaddq_u64(acc0, red_widen_at(in, i, validity));
    acc1 = vaddq_u64(acc1, red_widen_at(in, i + kW, validity));
    acc2 = vaddq_u64(acc2, red_widen_at(in, i + 2 * kW, validity));
    acc3 = vaddq_u64(acc3, red_widen_at(in, i + 3 * kW, validity));
  }
  for (; i + kW <= n; i += kW) {
    acc0 = vaddq_u64(acc0, red_widen_at(in, i, validity));
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

// A = 4 independent vector accumulators (ADR-013). Masked lanes add -0.0, the exact neutral.
constexpr int kRedSumAccs = 4;

// One A-way block: each accumulator takes its own vector, valid lanes only.
template <class T>
QUIVER_FORCE_INLINE void red_sum_step(red_Vec<T>* acc, const T* in, std::int64_t i,
                                      const std::uint8_t* validity) noexcept {
  constexpr std::int64_t kW = red_vec_lanes<T>();
  const red_Vec<T> neg0 = RedOps<T>::broadcast(static_cast<T>(-0.0));
  for (int k = 0; k < kRedSumAccs; ++k) {
    const std::int64_t at = i + k * kW;
    red_Vec<T> v = RedOps<T>::load(in + at);
    if (validity != nullptr) {
      v = blend<T>(group_mask<T>(validity, at), v, neg0);
    }
    acc[k] = RedOps<T>::add(acc[k], v);
  }
}

template <class T>
T dense_sum_float(const T* in, std::int64_t n, const std::uint8_t* validity) noexcept {
  constexpr std::int64_t kW = red_vec_lanes<T>();  // f32: 4, f64: 2
  constexpr std::int64_t kBlock = kRedSumAccs * kW;
  T s = T{0};
  std::int64_t i = 0;
  red_Vec<T> acc[kRedSumAccs] = {RedOps<T>::broadcast(T{0}), RedOps<T>::broadcast(T{0}),
                                 RedOps<T>::broadcast(T{0}), RedOps<T>::broadcast(T{0})};
  for (; i + kBlock <= n; i += kBlock) {
    red_sum_step<T>(acc, in, i, validity);
  }
  // ADR-013 frozen combine: (0+2),(1+3), then +
  const red_Vec<T> vsum =
      RedOps<T>::add(RedOps<T>::add(acc[0], acc[2]), RedOps<T>::add(acc[1], acc[3]));
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
