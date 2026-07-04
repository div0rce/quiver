// K9 arith — NEON backend (PRD 08 §5 K9): direct SIMD add/sub/mul per lane width (all
// native and modular on NEON except the 64-bit multiply, which decomposes into umull/umlal
// 32x32 partials exactly as in K7); floats native. Exact aliasing out == a/b is safe (loads
// complete before the store per block). Tails scalar (ADR-015). Bit-identical to
// arith_scalar_impl.h (REQ-KERNEL-002).
// Module: MOD-K9-ARITH | REQs: REQ-K9-001, REQ-SIMD-001..003/-007 | ADR-003
#include "src/kernels/arith/arith_scalar_impl.h"

#if defined(__aarch64__) || defined(_M_ARM64)

#include <arm_neon.h>
#include <cstring>
#include <type_traits>

QUIVER_BEGIN_NAMESPACE
namespace detail::neon {

namespace {

QUIVER_FORCE_INLINE uint64x2_t mul64_lo(uint64x2_t a, uint64x2_t b) noexcept {
  const uint32x2_t a_lo = vmovn_u64(a);
  const uint32x2_t b_lo = vmovn_u64(b);
  const uint32x2_t a_hi = vshrn_n_u64(a, 32);
  const uint32x2_t b_hi = vshrn_n_u64(b, 32);
  uint64x2_t cross = vmull_u32(a_lo, b_hi);
  cross = vmlal_u32(cross, a_hi, b_lo);
  return vaddq_u64(vmull_u32(a_lo, b_lo), vshlq_n_u64(cross, 32));
}

// One 128-bit block of the op at T's width, on the raw byte image (wrapping is modular, so
// signedness does not change the bits; floats use their own loads below).
template <class T>
QUIVER_FORCE_INLINE uint8x16_t apply_op_int(ArithOp op, uint8x16_t a, uint8x16_t b) noexcept {
  if constexpr (sizeof(T) == 1) {
    switch (op) {
    case ArithOp::kAdd:
      return vaddq_u8(a, b);
    case ArithOp::kSub:
      return vsubq_u8(a, b);
    case ArithOp::kMul:
      return vmulq_u8(a, b);
    }
  } else if constexpr (sizeof(T) == 2) {
    const uint16x8_t x = vreinterpretq_u16_u8(a);
    const uint16x8_t y = vreinterpretq_u16_u8(b);
    switch (op) {
    case ArithOp::kAdd:
      return vreinterpretq_u8_u16(vaddq_u16(x, y));
    case ArithOp::kSub:
      return vreinterpretq_u8_u16(vsubq_u16(x, y));
    case ArithOp::kMul:
      return vreinterpretq_u8_u16(vmulq_u16(x, y));
    }
  } else if constexpr (sizeof(T) == 4) {
    const uint32x4_t x = vreinterpretq_u32_u8(a);
    const uint32x4_t y = vreinterpretq_u32_u8(b);
    switch (op) {
    case ArithOp::kAdd:
      return vreinterpretq_u8_u32(vaddq_u32(x, y));
    case ArithOp::kSub:
      return vreinterpretq_u8_u32(vsubq_u32(x, y));
    case ArithOp::kMul:
      return vreinterpretq_u8_u32(vmulq_u32(x, y));
    }
  } else {
    const uint64x2_t x = vreinterpretq_u64_u8(a);
    const uint64x2_t y = vreinterpretq_u64_u8(b);
    switch (op) {
    case ArithOp::kAdd:
      return vreinterpretq_u8_u64(vaddq_u64(x, y));
    case ArithOp::kSub:
      return vreinterpretq_u8_u64(vsubq_u64(x, y));
    case ArithOp::kMul:
      return vreinterpretq_u8_u64(mul64_lo(x, y));
    }
  }
  return a;  // unreachable for in-contract op values
}

template <class T, class LoadB>
void arith_impl(ArithOp op, const T* a, LoadB load_b, std::int64_t n, T* out) noexcept {
  constexpr std::int64_t kW = static_cast<std::int64_t>(16 / sizeof(T));
  std::int64_t i = 0;
  if constexpr (std::is_same_v<T, float>) {
    for (; i + 4 <= n; i += 4) {
      const float32x4_t va = vld1q_f32(a + i);
      const float32x4_t vb = load_b.f32(i);
      float32x4_t r;
      switch (op) {
      case ArithOp::kAdd:
        r = vaddq_f32(va, vb);
        break;
      case ArithOp::kSub:
        r = vsubq_f32(va, vb);
        break;
      default:
        r = vmulq_f32(va, vb);
        break;
      }
      vst1q_f32(out + i, r);
    }
  } else if constexpr (std::is_same_v<T, double>) {
    for (; i + 2 <= n; i += 2) {
      const float64x2_t va = vld1q_f64(a + i);
      const float64x2_t vb = load_b.f64(i);
      float64x2_t r;
      switch (op) {
      case ArithOp::kAdd:
        r = vaddq_f64(va, vb);
        break;
      case ArithOp::kSub:
        r = vsubq_f64(va, vb);
        break;
      default:
        r = vmulq_f64(va, vb);
        break;
      }
      vst1q_f64(out + i, r);
    }
  } else {
    for (; i + kW <= n; i += kW) {
      const uint8x16_t va = vld1q_u8(reinterpret_cast<const std::uint8_t*>(a + i));
      const uint8x16_t vb = load_b.bytes(i);
      vst1q_u8(reinterpret_cast<std::uint8_t*>(out + i), apply_op_int<T>(op, va, vb));
    }
  }
  for (; i < n; ++i) {  // scalar tail, identical arithmetic
    out[i] = scalar_impl::arith_one(op, a[i], load_b.tail(i));
  }
}

template <class T>
struct BatchRhs {
  const T* b;
  uint8x16_t bytes(std::int64_t i) const noexcept {
    return vld1q_u8(reinterpret_cast<const std::uint8_t*>(b + i));
  }
  float32x4_t f32(std::int64_t i) const noexcept {
    return vld1q_f32(reinterpret_cast<const float*>(b) + i);
  }
  float64x2_t f64(std::int64_t i) const noexcept {
    return vld1q_f64(reinterpret_cast<const double*>(b) + i);
  }
  T tail(std::int64_t i) const noexcept { return b[i]; }
};

template <class T>
struct ScalarRhs {
  T b;
  uint8x16_t bytes(std::int64_t) const noexcept {
    uint8x16_t v;
    T lanes[16 / sizeof(T)];
    for (auto& lane : lanes) {
      lane = b;
    }
    std::memcpy(&v, lanes, 16);
    return v;
  }
  float32x4_t f32(std::int64_t) const noexcept {
    if constexpr (std::is_same_v<T, float>) {
      return vdupq_n_f32(b);
    } else {
      return vdupq_n_f32(0.0f);  // unreachable: f32 path only instantiated for float
    }
  }
  float64x2_t f64(std::int64_t) const noexcept {
    if constexpr (std::is_same_v<T, double>) {
      return vdupq_n_f64(b);
    } else {
      return vdupq_n_f64(0.0);  // unreachable: f64 path only instantiated for double
    }
  }
  T tail(std::int64_t) const noexcept { return b; }
};

}  // namespace

// NOLINTBEGIN(bugprone-macro-parentheses): T expands to type names inside declarators.
#define QUIVER_K9_DEFINE(T)                                                                        \
  void k9_arith(ArithOp op, const T* a, const T* b, std::int64_t n, T* out) noexcept {             \
    arith_impl<T>(op, a, BatchRhs<T>{b}, n, out);                                                  \
  }                                                                                                \
  void k9_arith_scalar_rhs(ArithOp op, const T* a, T b, std::int64_t n, T* out) noexcept {         \
    arith_impl<T>(op, a, ScalarRhs<T>{b}, n, out);                                                 \
  }

QUIVER_K9_DEFINE(std::int8_t)
QUIVER_K9_DEFINE(std::int16_t)
QUIVER_K9_DEFINE(std::int32_t)
QUIVER_K9_DEFINE(std::int64_t)
QUIVER_K9_DEFINE(std::uint8_t)
QUIVER_K9_DEFINE(std::uint16_t)
QUIVER_K9_DEFINE(std::uint32_t)
QUIVER_K9_DEFINE(std::uint64_t)
QUIVER_K9_DEFINE(float)
QUIVER_K9_DEFINE(double)
#undef QUIVER_K9_DEFINE
// NOLINTEND(bugprone-macro-parentheses)

}  // namespace detail::neon
QUIVER_END_NAMESPACE

#endif  // aarch64
