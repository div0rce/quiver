// K4 mask_algebra — NEON backend: 128-bit bitwise ops over the byte stream, 4×-unrolled
// (REQ-SIMD-008: ≥4 independent 128-bit ops in flight for Firestorm's 4×128 pipes);
// popcount and queries reuse the scalar word cores (bit-identical by construction; the
// honest-verdict hypothesis for this family is that autovec ties explicit SIMD — Survey
// §4.4). Bit-identical to mask_scalar_impl.h (REQ-KERNEL-002); tails per ADR-015/ADR-016.
// NEON needs no target region on ARM64 (baseline ISA, REQ-SIMD-002).
// Module: MOD-K4-MASK | REQs: REQ-K4-001..003, REQ-SIMD-001..003/-008 | ADR-003
#include "src/kernels/mask/mask_scalar_impl.h"

#if defined(__aarch64__) || defined(_M_ARM64)

#include <arm_neon.h>

QUIVER_BEGIN_NAMESPACE
namespace detail::neon {

namespace {

QUIVER_FORCE_INLINE uint8x16_t apply_op128(MaskOp op, uint8x16_t a, uint8x16_t b) noexcept {
  switch (op) {
  case MaskOp::kAnd:
    return vandq_u8(a, b);
  case MaskOp::kOr:
    return vorrq_u8(a, b);
  case MaskOp::kAndNot:
    return vbicq_u8(a, b);  // a & ~b
  case MaskOp::kXor:
    return veorq_u8(a, b);
  }
  return vdupq_n_u8(0);  // unreachable for in-contract op values
}

}  // namespace

void k4_mask_combine(MaskOp op, const std::uint8_t* a, const std::uint8_t* b, std::int64_t n,
                     std::uint8_t* out) noexcept {
  const std::int64_t bytes = bitmap_bytes(n);
  std::int64_t i = 0;
  for (; i + 64 <= bytes; i += 64) {  // 4 independent 128-bit ops per iteration
    vst1q_u8(out + i, apply_op128(op, vld1q_u8(a + i), vld1q_u8(b + i)));
    vst1q_u8(out + i + 16, apply_op128(op, vld1q_u8(a + i + 16), vld1q_u8(b + i + 16)));
    vst1q_u8(out + i + 32, apply_op128(op, vld1q_u8(a + i + 32), vld1q_u8(b + i + 32)));
    vst1q_u8(out + i + 48, apply_op128(op, vld1q_u8(a + i + 48), vld1q_u8(b + i + 48)));
  }
  for (; i + 16 <= bytes; i += 16) {
    vst1q_u8(out + i, apply_op128(op, vld1q_u8(a + i), vld1q_u8(b + i)));
  }
  for (; i < bytes; ++i) {  // scalar byte tail (ADR-015: no over-read)
    out[i] = static_cast<std::uint8_t>(scalar_impl::apply_mask_op(op, a[i], b[i]));
  }
  zero_tail_bits(out, n);
}

void k4_mask_not(const std::uint8_t* a, std::int64_t n, std::uint8_t* out) noexcept {
  const std::int64_t bytes = bitmap_bytes(n);
  std::int64_t i = 0;
  for (; i + 64 <= bytes; i += 64) {
    vst1q_u8(out + i, vmvnq_u8(vld1q_u8(a + i)));
    vst1q_u8(out + i + 16, vmvnq_u8(vld1q_u8(a + i + 16)));
    vst1q_u8(out + i + 32, vmvnq_u8(vld1q_u8(a + i + 32)));
    vst1q_u8(out + i + 48, vmvnq_u8(vld1q_u8(a + i + 48)));
  }
  for (; i + 16 <= bytes; i += 16) {
    vst1q_u8(out + i, vmvnq_u8(vld1q_u8(a + i)));
  }
  for (; i < bytes; ++i) {
    out[i] = static_cast<std::uint8_t>(~a[i]);
  }
  zero_tail_bits(out, n);
}

// Word-loop cores are already the specified technique for counting/queries (PRD 08 K4);
// the backend delegates so results are identical by construction.
std::int64_t k4_mask_popcount(const std::uint8_t* a, std::int64_t n) noexcept {
  return scalar_impl::mask_popcount(a, n);
}
bool k4_mask_all(const std::uint8_t* a, std::int64_t n) noexcept {
  return scalar_impl::mask_all(a, n);
}
bool k4_mask_any(const std::uint8_t* a, std::int64_t n) noexcept {
  return scalar_impl::mask_any(a, n);
}
bool k4_mask_none(const std::uint8_t* a, std::int64_t n) noexcept {
  return scalar_impl::mask_none(a, n);
}

}  // namespace detail::neon
QUIVER_END_NAMESPACE

#endif  // aarch64
