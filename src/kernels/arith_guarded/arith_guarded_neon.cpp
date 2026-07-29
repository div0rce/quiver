// K10 arith_guarded — NEON backend (PRD 08 §5 K10; ADR-014). Checked add/sub vectorize the
// sign trick with overflow lanes packed via the weight-AND idiom; saturating add/sub are
// NATIVE at every width on NEON (sqadd/uqadd/sqsub/uqsub — including 64-bit lanes). All
// multiplies (checked and saturating) delegate to the scalar core: 64-bit is the documented
// REQ-K10-003 concession, and vectorizing the narrow widening-multiply forms is
// ledger-gated follow-up recorded on the family page. Bit-identical to
// arith_guarded_scalar_impl.h everywhere (REQ-KERNEL-002).
// Module: MOD-K10-ARITH-GUARDED | REQs: REQ-K10-001..003, REQ-SIMD-001..003 | ADR-014
#include "src/kernels/arith_guarded/arith_guarded_scalar_impl.h"

#if defined(__aarch64__) || defined(_M_ARM64)

#include <arm_neon.h>
#include <cstring>
#include <type_traits>

QUIVER_BEGIN_NAMESPACE
namespace detail::neon {

namespace {

QUIVER_FORCE_INLINE uint8x16_t and_bytes(uint8x16_t a, uint8x16_t b) noexcept {
  return vandq_u8(a, b);
}
QUIVER_FORCE_INLINE uint8x16_t xor_bytes(uint8x16_t a, uint8x16_t b) noexcept {
  return veorq_u8(a, b);
}

// The raw 128-bit block viewed at one lane width. Everything the guarded kernels need per
// width lives here, so the reinterpret ladder is written once per width instead of once per
// operation: wrapping add/sub (modular on the byte image), unsigned lane compare, the sign
// mask, and the native saturating forms (sqadd/uqadd/sqsub/uqsub — all widths on A64).
template <int Bytes>
struct AgLanes;

template <>
struct AgLanes<1> {
  static QUIVER_FORCE_INLINE uint8x16_t add(uint8x16_t a, uint8x16_t b) noexcept {
    return vaddq_u8(a, b);
  }
  static QUIVER_FORCE_INLINE uint8x16_t sub(uint8x16_t a, uint8x16_t b) noexcept {
    return vsubq_u8(a, b);
  }
  static QUIVER_FORCE_INLINE uint8x16_t ult(uint8x16_t a, uint8x16_t b) noexcept {
    return vcltq_u8(a, b);
  }
  static QUIVER_FORCE_INLINE uint8x16_t negative(uint8x16_t v) noexcept {
    return vcltq_s8(vreinterpretq_s8_u8(v), vdupq_n_s8(0));
  }
  static QUIVER_FORCE_INLINE uint8x16_t qadd_signed(uint8x16_t a, uint8x16_t b) noexcept {
    return vreinterpretq_u8_s8(vqaddq_s8(vreinterpretq_s8_u8(a), vreinterpretq_s8_u8(b)));
  }
  static QUIVER_FORCE_INLINE uint8x16_t qsub_signed(uint8x16_t a, uint8x16_t b) noexcept {
    return vreinterpretq_u8_s8(vqsubq_s8(vreinterpretq_s8_u8(a), vreinterpretq_s8_u8(b)));
  }
  static QUIVER_FORCE_INLINE uint8x16_t qadd_unsigned(uint8x16_t a, uint8x16_t b) noexcept {
    return vqaddq_u8(a, b);
  }
  static QUIVER_FORCE_INLINE uint8x16_t qsub_unsigned(uint8x16_t a, uint8x16_t b) noexcept {
    return vqsubq_u8(a, b);
  }
};

template <>
struct AgLanes<2> {
  static QUIVER_FORCE_INLINE uint8x16_t add(uint8x16_t a, uint8x16_t b) noexcept {
    return vreinterpretq_u8_u16(vaddq_u16(vreinterpretq_u16_u8(a), vreinterpretq_u16_u8(b)));
  }
  static QUIVER_FORCE_INLINE uint8x16_t sub(uint8x16_t a, uint8x16_t b) noexcept {
    return vreinterpretq_u8_u16(vsubq_u16(vreinterpretq_u16_u8(a), vreinterpretq_u16_u8(b)));
  }
  static QUIVER_FORCE_INLINE uint8x16_t ult(uint8x16_t a, uint8x16_t b) noexcept {
    return vreinterpretq_u8_u16(vcltq_u16(vreinterpretq_u16_u8(a), vreinterpretq_u16_u8(b)));
  }
  static QUIVER_FORCE_INLINE uint8x16_t negative(uint8x16_t v) noexcept {
    return vreinterpretq_u8_u16(vcltq_s16(vreinterpretq_s16_u8(v), vdupq_n_s16(0)));
  }
  static QUIVER_FORCE_INLINE uint8x16_t qadd_signed(uint8x16_t a, uint8x16_t b) noexcept {
    return vreinterpretq_u8_s16(vqaddq_s16(vreinterpretq_s16_u8(a), vreinterpretq_s16_u8(b)));
  }
  static QUIVER_FORCE_INLINE uint8x16_t qsub_signed(uint8x16_t a, uint8x16_t b) noexcept {
    return vreinterpretq_u8_s16(vqsubq_s16(vreinterpretq_s16_u8(a), vreinterpretq_s16_u8(b)));
  }
  static QUIVER_FORCE_INLINE uint8x16_t qadd_unsigned(uint8x16_t a, uint8x16_t b) noexcept {
    return vreinterpretq_u8_u16(vqaddq_u16(vreinterpretq_u16_u8(a), vreinterpretq_u16_u8(b)));
  }
  static QUIVER_FORCE_INLINE uint8x16_t qsub_unsigned(uint8x16_t a, uint8x16_t b) noexcept {
    return vreinterpretq_u8_u16(vqsubq_u16(vreinterpretq_u16_u8(a), vreinterpretq_u16_u8(b)));
  }
};

template <>
struct AgLanes<4> {
  static QUIVER_FORCE_INLINE uint8x16_t add(uint8x16_t a, uint8x16_t b) noexcept {
    return vreinterpretq_u8_u32(vaddq_u32(vreinterpretq_u32_u8(a), vreinterpretq_u32_u8(b)));
  }
  static QUIVER_FORCE_INLINE uint8x16_t sub(uint8x16_t a, uint8x16_t b) noexcept {
    return vreinterpretq_u8_u32(vsubq_u32(vreinterpretq_u32_u8(a), vreinterpretq_u32_u8(b)));
  }
  static QUIVER_FORCE_INLINE uint8x16_t ult(uint8x16_t a, uint8x16_t b) noexcept {
    return vreinterpretq_u8_u32(vcltq_u32(vreinterpretq_u32_u8(a), vreinterpretq_u32_u8(b)));
  }
  static QUIVER_FORCE_INLINE uint8x16_t negative(uint8x16_t v) noexcept {
    return vreinterpretq_u8_u32(vcltq_s32(vreinterpretq_s32_u8(v), vdupq_n_s32(0)));
  }
  static QUIVER_FORCE_INLINE uint8x16_t qadd_signed(uint8x16_t a, uint8x16_t b) noexcept {
    return vreinterpretq_u8_s32(vqaddq_s32(vreinterpretq_s32_u8(a), vreinterpretq_s32_u8(b)));
  }
  static QUIVER_FORCE_INLINE uint8x16_t qsub_signed(uint8x16_t a, uint8x16_t b) noexcept {
    return vreinterpretq_u8_s32(vqsubq_s32(vreinterpretq_s32_u8(a), vreinterpretq_s32_u8(b)));
  }
  static QUIVER_FORCE_INLINE uint8x16_t qadd_unsigned(uint8x16_t a, uint8x16_t b) noexcept {
    return vreinterpretq_u8_u32(vqaddq_u32(vreinterpretq_u32_u8(a), vreinterpretq_u32_u8(b)));
  }
  static QUIVER_FORCE_INLINE uint8x16_t qsub_unsigned(uint8x16_t a, uint8x16_t b) noexcept {
    return vreinterpretq_u8_u32(vqsubq_u32(vreinterpretq_u32_u8(a), vreinterpretq_u32_u8(b)));
  }
};

template <>
struct AgLanes<8> {
  static QUIVER_FORCE_INLINE uint8x16_t add(uint8x16_t a, uint8x16_t b) noexcept {
    return vreinterpretq_u8_u64(vaddq_u64(vreinterpretq_u64_u8(a), vreinterpretq_u64_u8(b)));
  }
  static QUIVER_FORCE_INLINE uint8x16_t sub(uint8x16_t a, uint8x16_t b) noexcept {
    return vreinterpretq_u8_u64(vsubq_u64(vreinterpretq_u64_u8(a), vreinterpretq_u64_u8(b)));
  }
  static QUIVER_FORCE_INLINE uint8x16_t ult(uint8x16_t a, uint8x16_t b) noexcept {
    return vreinterpretq_u8_u64(vcltq_u64(vreinterpretq_u64_u8(a), vreinterpretq_u64_u8(b)));
  }
  static QUIVER_FORCE_INLINE uint8x16_t negative(uint8x16_t v) noexcept {
    return vreinterpretq_u8_u64(vcltq_s64(vreinterpretq_s64_u8(v), vdupq_n_s64(0)));
  }
  static QUIVER_FORCE_INLINE uint8x16_t qadd_signed(uint8x16_t a, uint8x16_t b) noexcept {
    return vreinterpretq_u8_s64(vqaddq_s64(vreinterpretq_s64_u8(a), vreinterpretq_s64_u8(b)));
  }
  static QUIVER_FORCE_INLINE uint8x16_t qsub_signed(uint8x16_t a, uint8x16_t b) noexcept {
    return vreinterpretq_u8_s64(vqsubq_s64(vreinterpretq_s64_u8(a), vreinterpretq_s64_u8(b)));
  }
  static QUIVER_FORCE_INLINE uint8x16_t qadd_unsigned(uint8x16_t a, uint8x16_t b) noexcept {
    return vreinterpretq_u8_u64(vqaddq_u64(vreinterpretq_u64_u8(a), vreinterpretq_u64_u8(b)));
  }
  static QUIVER_FORCE_INLINE uint8x16_t qsub_unsigned(uint8x16_t a, uint8x16_t b) noexcept {
    return vreinterpretq_u8_u64(vqsubq_u64(vreinterpretq_u64_u8(a), vreinterpretq_u64_u8(b)));
  }
};

template <class T>
using AgW = AgLanes<static_cast<int>(sizeof(T))>;

template <class T>
QUIVER_FORCE_INLINE uint8x16_t checked_block(ArithOp op, uint8x16_t a, uint8x16_t b,
                                             uint8x16_t* overflow) noexcept {
  using W = AgW<T>;
  if constexpr (std::is_signed_v<T>) {
    if (op == ArithOp::kAdd) {
      const uint8x16_t r = W::add(a, b);
      *overflow = W::negative(and_bytes(xor_bytes(a, r), xor_bytes(b, r)));
      return r;
    }
    const uint8x16_t r = W::sub(a, b);
    *overflow = W::negative(and_bytes(xor_bytes(a, b), xor_bytes(a, r)));
    return r;
  } else {
    if (op == ArithOp::kAdd) {
      const uint8x16_t r = W::add(a, b);
      *overflow = W::ult(r, a);
      return r;
    }
    const uint8x16_t r = W::sub(a, b);
    *overflow = W::ult(a, b);
    return r;
  }
}

// Overflow lane mask -> LSB-first bits for the block (weight-AND + vaddv, as in K1).
template <class T>
QUIVER_FORCE_INLINE std::uint32_t mask_bits(uint8x16_t m) noexcept {
  if constexpr (sizeof(T) == 1) {
    const uint8x8_t w = vcreate_u8(0x8040201008040201ull);
    return static_cast<std::uint32_t>(vaddv_u8(vand_u8(vget_low_u8(m), w))) |
           (static_cast<std::uint32_t>(vaddv_u8(vand_u8(vget_high_u8(m), w))) << 8);
  } else if constexpr (sizeof(T) == 2) {
    const uint16x8_t w = {1, 2, 4, 8, 16, 32, 64, 128};
    return vaddvq_u16(vandq_u16(vreinterpretq_u16_u8(m), w));
  } else if constexpr (sizeof(T) == 4) {
    const uint32x4_t w = {1, 2, 4, 8};
    return vaddvq_u32(vandq_u32(vreinterpretq_u32_u8(m), w));
  } else {
    const uint64x2_t w = {1, 2};
    const uint64x2_t t = vandq_u64(vreinterpretq_u64_u8(m), w);
    return static_cast<std::uint32_t>(vgetq_lane_u64(t, 0) | vgetq_lane_u64(t, 1));
  }
}

// Native saturating add/sub at T's width (sqadd/uqadd/sqsub/uqsub — all widths on A64).
template <class T>
QUIVER_FORCE_INLINE uint8x16_t saturating_block(ArithOp op, uint8x16_t a, uint8x16_t b) noexcept {
  using W = AgW<T>;
  if constexpr (std::is_signed_v<T>) {
    return op == ArithOp::kAdd ? W::qadd_signed(a, b) : W::qsub_signed(a, b);
  } else {
    return op == ArithOp::kAdd ? W::qadd_unsigned(a, b) : W::qsub_unsigned(a, b);
  }
}

template <class T>
struct ag_BatchRhs {
  const T* b;
  uint8x16_t operator()(std::int64_t i) const noexcept {
    return vld1q_u8(reinterpret_cast<const std::uint8_t*>(b + i));
  }
  T tail(std::int64_t i) const noexcept { return b[i]; }
};

template <class T>
struct ag_ScalarRhs {
  T b;
  uint8x16_t operator()(std::int64_t) const noexcept {
    uint8x16_t v;
    T lanes[16 / sizeof(T)];
    for (auto& lane : lanes) {
      lane = b;
    }
    std::memcpy(&v, lanes, 16);
    return v;
  }
  T tail(std::int64_t) const noexcept { return b; }
};

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

// Vectorized checked add/sub over 8-element-aligned groups (>= one bitmap byte per group).
// Holding the per-call invariants keeps each step small enough to read on its own.
template <class T, class LoadB>
struct AgCheckedRun {
  static constexpr std::int64_t kW = static_cast<std::int64_t>(16 / sizeof(T));
  static constexpr std::int64_t kGroup = (kW < 8) ? 8 : kW;

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
      const uint8x16_t va = vld1q_u8(reinterpret_cast<const std::uint8_t*>(in.a + i + v));
      uint8x16_t ov;
      const uint8x16_t r = checked_block<T>(op, va, load_b(i + v), &ov);
      vst1q_u8(reinterpret_cast<std::uint8_t*>(sink.out + i + v), r);
      bits |= mask_bits<T>(ov) << v;
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

  // Scalar tail with byte assembly (ADR-015/ADR-016).
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

template <class T, class LoadB>
void saturating_addsub_impl(ArithOp op, AgBatch<T> in, LoadB load_b, T* out) noexcept {
  constexpr std::int64_t kW = static_cast<std::int64_t>(16 / sizeof(T));
  std::int64_t i = 0;
  for (; i + kW <= in.n; i += kW) {
    const uint8x16_t va = vld1q_u8(reinterpret_cast<const std::uint8_t*>(in.a + i));
    vst1q_u8(reinterpret_cast<std::uint8_t*>(out + i), saturating_block<T>(op, va, load_b(i)));
  }
  for (; i < in.n; ++i) {
    out[i] = scalar_impl::saturate_one(op, in.a[i], load_b.tail(i));
  }
}

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

}  // namespace detail::neon
QUIVER_END_NAMESPACE

#endif  // aarch64
