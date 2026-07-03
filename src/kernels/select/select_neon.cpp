// K3 sel_convert — NEON backend. bitmap_to_selvec uses the emulated-compress core: for each
// selection byte, the kCompactLut32 row IS the compacted lane-index list — add the broadcast
// base and store 8 indices (two 128-bit stores), advancing by popcount (PRD 09 §6; the
// full-width stores stay within the n-element capacity region, REQ-MEM-008). selvec_to_bitmap
// is scalar-dominant by design (sorted scatter; PRD 08 K3) and delegates to the scalar core.
// Bit-identical to select_scalar_impl.h on defined output regions (REQ-KERNEL-002).
// Module: MOD-K3-SELECT | REQs: REQ-K3-001..002, REQ-SIMD-001..003/-005 | ADR-003
#include "src/kernels/common/luts.h"
#include "src/kernels/select/select_scalar_impl.h"

#if defined(__aarch64__) || defined(_M_ARM64)

#include <arm_neon.h>

QUIVER_BEGIN_NAMESPACE
namespace detail::neon {

std::int64_t k3_bitmap_to_selvec(const std::uint8_t* selection, std::int64_t n,
                                 std::uint32_t* out) noexcept {
  std::int64_t count = 0;
  const std::int64_t full_bytes = n >> 3;
  for (std::int64_t b = 0; b < full_bytes; ++b) {
    const std::uint8_t byte = selection[b];
    // The LUT row already lists the set-bit lanes front-packed; unset lanes trail as
    // scratch within the capacity region (REQ-MEM-008).
    const std::uint32_t* row = kCompactLut32.perm[byte];
    const uint32x4_t base = vdupq_n_u32(static_cast<std::uint32_t>(b << 3));
    vst1q_u32(out + count, vaddq_u32(vld1q_u32(row), base));
    vst1q_u32(out + count + 4, vaddq_u32(vld1q_u32(row + 4), base));
    count += kPopcountLut.count[byte];
  }
  // Scalar tail (< 8 elements), identical to the reference (ADR-015).
  for (std::int64_t i = full_bytes << 3; i < n; ++i) {
    out[count] = static_cast<std::uint32_t>(i);
    count += ((selection[i >> 3] >> (i & 7)) & 1u) != 0 ? 1 : 0;
  }
  return count;
}

void k3_selvec_to_bitmap(const std::uint32_t* sel, std::int64_t sel_len, std::int64_t n,
                         std::uint8_t* out) noexcept {
  scalar_impl::selvec_to_bitmap(sel, sel_len, n, out);  // scalar-dominant (PRD 08 K3)
}

}  // namespace detail::neon
QUIVER_END_NAMESPACE

#endif  // aarch64
