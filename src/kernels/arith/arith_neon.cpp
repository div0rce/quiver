// K9 arith — NEON backend (PRD 08 §5 K9): direct SIMD add/sub/mul per lane width for the
// 1/2/4-byte widths (all native and modular on NEON; floats native). The 8-byte widths
// (i64/u64/f64) delegate to the autovectorized scalar reference: pure elementwise 64-bit
// arithmetic is bandwidth-bound and the compiler's autovectorized loop measured faster than the
// handwritten one on Apple M2 (see the family page). Exact aliasing out == a/b is safe (loads
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

// Integer arithmetic runs on the raw byte image (wrapping is modular, so signedness does not
// change the bits), so one specialization per lane width covers every integer element type.
// There is no 8-byte entry: those delegate to the autovectorized scalar reference (measured
// win, see k9_arith below).
template <int Bytes>
struct ArIntOps;

template <>
struct ArIntOps<1> {
  static QUIVER_FORCE_INLINE uint8x16_t add(uint8x16_t a, uint8x16_t b) noexcept {
    return vaddq_u8(a, b);
  }
  static QUIVER_FORCE_INLINE uint8x16_t sub(uint8x16_t a, uint8x16_t b) noexcept {
    return vsubq_u8(a, b);
  }
  static QUIVER_FORCE_INLINE uint8x16_t mul(uint8x16_t a, uint8x16_t b) noexcept {
    return vmulq_u8(a, b);
  }
};

template <>
struct ArIntOps<2> {
  static QUIVER_FORCE_INLINE uint8x16_t add(uint8x16_t a, uint8x16_t b) noexcept {
    return vreinterpretq_u8_u16(vaddq_u16(vreinterpretq_u16_u8(a), vreinterpretq_u16_u8(b)));
  }
  static QUIVER_FORCE_INLINE uint8x16_t sub(uint8x16_t a, uint8x16_t b) noexcept {
    return vreinterpretq_u8_u16(vsubq_u16(vreinterpretq_u16_u8(a), vreinterpretq_u16_u8(b)));
  }
  static QUIVER_FORCE_INLINE uint8x16_t mul(uint8x16_t a, uint8x16_t b) noexcept {
    return vreinterpretq_u8_u16(vmulq_u16(vreinterpretq_u16_u8(a), vreinterpretq_u16_u8(b)));
  }
};

template <>
struct ArIntOps<4> {
  static QUIVER_FORCE_INLINE uint8x16_t add(uint8x16_t a, uint8x16_t b) noexcept {
    return vreinterpretq_u8_u32(vaddq_u32(vreinterpretq_u32_u8(a), vreinterpretq_u32_u8(b)));
  }
  static QUIVER_FORCE_INLINE uint8x16_t sub(uint8x16_t a, uint8x16_t b) noexcept {
    return vreinterpretq_u8_u32(vsubq_u32(vreinterpretq_u32_u8(a), vreinterpretq_u32_u8(b)));
  }
  static QUIVER_FORCE_INLINE uint8x16_t mul(uint8x16_t a, uint8x16_t b) noexcept {
    return vreinterpretq_u8_u32(vmulq_u32(vreinterpretq_u32_u8(a), vreinterpretq_u32_u8(b)));
  }
};

// One element type's vector view: the register type, its width, unaligned load/store, the
// broadcast, and the three ops. Naming these once lets the kernel below be a single loop.
template <class T>
struct ArOps {
  using V = uint8x16_t;
  static constexpr std::int64_t kW = static_cast<std::int64_t>(16 / sizeof(T));
  static QUIVER_FORCE_INLINE V load(const T* p) noexcept {
    return vld1q_u8(reinterpret_cast<const std::uint8_t*>(p));
  }
  static QUIVER_FORCE_INLINE void store(T* p, V v) noexcept {
    vst1q_u8(reinterpret_cast<std::uint8_t*>(p), v);
  }
  static QUIVER_FORCE_INLINE V broadcast(T x) noexcept {
    V out;
    T lanes[16 / sizeof(T)];
    for (auto& lane : lanes) {
      lane = x;
    }
    std::memcpy(&out, lanes, 16);
    return out;
  }
  static QUIVER_FORCE_INLINE V add(V a, V b) noexcept {
    return ArIntOps<static_cast<int>(sizeof(T))>::add(a, b);
  }
  static QUIVER_FORCE_INLINE V sub(V a, V b) noexcept {
    return ArIntOps<static_cast<int>(sizeof(T))>::sub(a, b);
  }
  static QUIVER_FORCE_INLINE V mul(V a, V b) noexcept {
    return ArIntOps<static_cast<int>(sizeof(T))>::mul(a, b);
  }
};

template <>
struct ArOps<float> {
  using V = float32x4_t;
  static constexpr std::int64_t kW = 4;
  static QUIVER_FORCE_INLINE V load(const float* p) noexcept { return vld1q_f32(p); }
  static QUIVER_FORCE_INLINE void store(float* p, V v) noexcept { vst1q_f32(p, v); }
  static QUIVER_FORCE_INLINE V broadcast(float x) noexcept { return vdupq_n_f32(x); }
  static QUIVER_FORCE_INLINE V add(V a, V b) noexcept { return vaddq_f32(a, b); }
  static QUIVER_FORCE_INLINE V sub(V a, V b) noexcept { return vsubq_f32(a, b); }
  static QUIVER_FORCE_INLINE V mul(V a, V b) noexcept { return vmulq_f32(a, b); }
};

// No ArOps<double>: f64 is an 8-byte width, so k9_arith delegates it to the autovectorized
// scalar reference and never reaches this kernel. Adding one back is what a future
// ledger-backed decision to vectorize f64 would start from.

template <class T>
QUIVER_FORCE_INLINE typename ArOps<T>::V apply_op(ArithOp op, typename ArOps<T>::V a,
                                                  typename ArOps<T>::V b) noexcept {
  using O = ArOps<T>;
  switch (op) {
  case ArithOp::kAdd:
    return O::add(a, b);
  case ArithOp::kSub:
    return O::sub(a, b);
  case ArithOp::kMul:
    return O::mul(a, b);
  }
  return a;  // unreachable for in-contract op values
}

// The left-hand batch an arith op reads.
template <class T>
struct ArBatch {
  const T* a;
  std::int64_t n;
};

template <class T, class LoadB>
void arith_impl(ArithOp op, ArBatch<T> in, LoadB load_b, T* out) noexcept {
  using O = ArOps<T>;
  std::int64_t i = 0;
  for (; i + O::kW <= in.n; i += O::kW) {
    O::store(out + i, apply_op<T>(op, O::load(in.a + i), load_b(i)));
  }
  for (; i < in.n; ++i) {  // scalar tail, identical arithmetic
    out[i] = scalar_impl::arith_one(op, in.a[i], load_b.tail(i));
  }
}

template <class T>
struct ar_BatchRhs {
  const T* b;
  typename ArOps<T>::V operator()(std::int64_t i) const noexcept { return ArOps<T>::load(b + i); }
  T tail(std::int64_t i) const noexcept { return b[i]; }
};

template <class T>
struct ar_ScalarRhs {
  T b;
  typename ArOps<T>::V operator()(std::int64_t) const noexcept { return ArOps<T>::broadcast(b); }
  T tail(std::int64_t) const noexcept { return b; }
};

}  // namespace

// NOLINTBEGIN(bugprone-macro-parentheses): T expands to type names inside declarators.
#define QUIVER_K9_DEFINE(T)                                                                        \
  void k9_arith(ArithOp op, const T* a, const T* b, std::int64_t n, T* out) noexcept {             \
    if constexpr (sizeof(T) == 8) /* 8-byte elementwise is bandwidth-bound; autovec scalar wins */ \
      scalar_impl::arith(op, {a, n}, b, out);                                                      \
    else                                                                                           \
      arith_impl<T>(op, {a, n}, ar_BatchRhs<T>{b}, out);                                           \
  }                                                                                                \
  void k9_arith_scalar_rhs(ArithOp op, const T* a, T b, std::int64_t n, T* out) noexcept {         \
    if constexpr (sizeof(T) == 8)                                                                  \
      scalar_impl::arith_scalar_rhs(op, {a, n}, b, out);                                           \
    else                                                                                           \
      arith_impl<T>(op, {a, n}, ar_ScalarRhs<T>{b}, out);                                          \
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
