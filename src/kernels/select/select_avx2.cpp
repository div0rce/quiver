// K3 sel_convert — AVX2 backend. bitmap_to_selvec uses the emulated-compress core: for each
// selection byte, the kCompactLut32 row IS the compacted lane-index list — add the sel_broadcast
// base and store 8 indices, advancing by popcount (PRD 09 §6; full-vector stores stay within
// the n-element capacity region, REQ-MEM-008). selvec_to_bitmap is scalar-dominant by design
// (sorted scatter; PRD 08 K3) and delegates to the scalar core.
// Bit-identical to select_scalar_impl.h on defined output regions (REQ-KERNEL-002).
// Module: MOD-K3-SELECT | REQs: REQ-K3-001..002, REQ-SIMD-001..003/-005 | ADR-003
#include "src/kernels/common/luts.h"
#include "src/kernels/common/target_regions.h"
#include "src/kernels/select/select_scalar_impl.h"

#if defined(__x86_64__) || defined(_M_X64)

#include <immintrin.h>

QUIVER_BEGIN_NAMESPACE
QUIVER_TARGET_AVX2_BEGIN
namespace detail::avx2 {

std::int64_t k3_bitmap_to_selvec(const std::uint8_t* selection, std::int64_t n,
                                 std::uint32_t* out) noexcept {
  std::int64_t count = 0;
  const std::int64_t full_bytes = n >> 3;
  for (std::int64_t b = 0; b < full_bytes; ++b) {
    const std::uint8_t byte = selection[b];
    // The LUT row already lists the set-bit lanes front-packed; unset lanes trail as
    // scratch within the capacity region (REQ-MEM-008).
    const __m256i lanes =
        _mm256_loadu_si256(reinterpret_cast<const __m256i*>(kCompactLut32.perm[byte]));
    const __m256i base = _mm256_set1_epi32(static_cast<int>(b << 3));
    _mm256_storeu_si256(reinterpret_cast<__m256i*>(out + count), _mm256_add_epi32(lanes, base));
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

}  // namespace detail::avx2
QUIVER_TARGET_AVX2_END
QUIVER_END_NAMESPACE

#endif  // x86-64
