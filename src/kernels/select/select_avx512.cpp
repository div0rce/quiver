// K3 sel_convert — AVX-512 backend (ADR-025). `bitmap_to_selvec` compresses an index iota:
// load the group's selection bits into an opmask, COMPRESS TO A REGISTER the lane-index vector
// (`_mm512_maskz_compress_epi32`) then store, advancing by the mask popcount. Compress-to-
// register (not `compressstoreu`) is mandatory (Zen 4 microcode, Survey §4.1). The full store
// lands in [count, count+16) ⊆ [0, n) (count ≤ i, i+16 ≤ n) — inside capacity (REQ-MEM-008).
//
// ponytail: `selvec_to_bitmap` is a scatter with no clean AVX-512 win — it falls through to
// the AVX2 backend. Base set F+BW+DQ+VL; -skx-correct. Bit-identical to select_scalar_impl.h.
// Module: MOD-K3-SELECT | REQs: REQ-K3-001..002, REQ-SIMD-001..004 | ADR-003, ADR-025
#include "src/kernels/common/target_regions.h"
#include "src/kernels/select/select_scalar_impl.h"

#if defined(__x86_64__) || defined(_M_X64)

#include <bit>
#include <cstring>
#include <immintrin.h>

QUIVER_BEGIN_NAMESPACE
QUIVER_TARGET_AVX512_BEGIN
namespace detail::avx512 {

std::int64_t k3_bitmap_to_selvec(const std::uint8_t* selection, std::int64_t n,
                                 std::uint32_t* out) noexcept {
  const __m512i lane = _mm512_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
  std::int64_t count = 0;
  std::int64_t i = 0;
  for (; i + 16 <= n; i += 16) {
    __mmask16 m = 0;
    std::memcpy(&m, selection + (i >> 3), 2);
    const __m512i idx = _mm512_add_epi32(_mm512_set1_epi32(static_cast<int>(i)), lane);
    _mm512_storeu_si512(reinterpret_cast<void*>(out + count),
                        _mm512_maskz_compress_epi32(m, idx));  // compress to register, then store
    count += std::popcount(static_cast<std::uint16_t>(m));
  }
  for (; i < n; ++i) {  // scalar tail (ADR-015): unconditional index store within [0, n)
    out[count] = static_cast<std::uint32_t>(i);
    count += bitmap_get(selection, i) ? 1 : 0;
  }
  return count;
}

}  // namespace detail::avx512
QUIVER_TARGET_AVX512_END
QUIVER_END_NAMESPACE

#endif  // x86-64
