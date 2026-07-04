// K2 filter — AVX-512 backend (PRD 08 §5 K2; ADR-023). `filter_bitmap` for 32- and 64-bit
// lanes uses native compaction: load the group's selection bits into an opmask, COMPRESS TO A
// REGISTER (`_mm512_maskz_compress_epi32/64`) then a full-width store, advancing by the mask
// popcount. Compress-to-register-then-store (not `compressstoreu`) is mandatory — the store
// form is microcoded on Zen 4 (Survey §4.1). The full-vector store lands in [count, count+N)
// ⊆ [0, n): count ≤ i and i+N ≤ n, so it stays inside the capacity region (REQ-MEM-008).
//
// ponytail: only 32/64-bit bitmap compaction is wired here — the genuine avx512f win over
// AVX2's LUT permute. 8/16-bit compaction needs `vpcompressb/w` (VBMI2, a resolution-time
// variant) and `filter_selvec` is a gather; both are evidence-gated and fall through to the
// AVX2 backend until AVX-512 hardware justifies them (R-06). Base set F+BW+DQ+VL; -skx-correct.
// Bit-identical to filter_scalar_impl.h (REQ-KERNEL-002). Tails scalar (ADR-015).
// Module: MOD-K2-FILTER | REQs: REQ-K2-001..003, REQ-SIMD-001..004 | ADR-003, ADR-023
#include "src/kernels/common/target_regions.h"
#include "src/kernels/filter/filter_scalar_impl.h"

#if defined(__x86_64__) || defined(_M_X64)

#include <bit>
#include <cstring>
#include <immintrin.h>
#include <type_traits>

QUIVER_BEGIN_NAMESPACE
QUIVER_TARGET_AVX512_BEGIN
namespace detail::avx512 {

namespace {

// 32/64-bit dense compaction via compress-to-register.
template <class T>
std::int64_t filter_bitmap_impl(const T* in, std::int64_t n, const std::uint8_t* selection,
                                T* out) noexcept {
  static_assert(sizeof(T) == 4 || sizeof(T) == 8);
  constexpr std::int64_t kN = static_cast<std::int64_t>(64 / sizeof(T));  // 16 or 8 lanes
  std::int64_t count = 0;
  std::int64_t i = 0;
  for (; i + kN <= n; i += kN) {
    const __m512i v = _mm512_loadu_si512(reinterpret_cast<const void*>(in + i));
    __m512i c;
    unsigned pc;
    if constexpr (sizeof(T) == 4) {
      __mmask16 m = 0;
      std::memcpy(&m, selection + (i >> 3), 2);
      c = _mm512_maskz_compress_epi32(m, v);
      pc = static_cast<unsigned>(std::popcount(static_cast<std::uint16_t>(m)));
    } else {
      __mmask8 m = 0;
      std::memcpy(&m, selection + (i >> 3), 1);
      c = _mm512_maskz_compress_epi64(m, v);
      pc = static_cast<unsigned>(std::popcount(static_cast<std::uint8_t>(m)));
    }
    _mm512_storeu_si512(reinterpret_cast<void*>(out + count), c);  // full store within capacity
    count += pc;
  }
  for (; i < n; ++i) {  // scalar tail (ADR-015): unconditional store within [0, n)
    out[count] = in[i];
    count += bitmap_get(selection, i) ? 1 : 0;
  }
  return count;
}

}  // namespace

// NOLINTBEGIN(bugprone-macro-parentheses): T expands to type names inside declarators.
#define QUIVER_K2_BM_DEFINE(T)                                                                     \
  std::int64_t k2_filter_bitmap(const T* in, std::int64_t n, const std::uint8_t* selection,        \
                                T* out) noexcept {                                                 \
    return filter_bitmap_impl<T>(in, n, selection, out);                                           \
  }

QUIVER_K2_BM_DEFINE(std::int32_t)
QUIVER_K2_BM_DEFINE(std::int64_t)
QUIVER_K2_BM_DEFINE(std::uint32_t)
QUIVER_K2_BM_DEFINE(std::uint64_t)
QUIVER_K2_BM_DEFINE(float)
QUIVER_K2_BM_DEFINE(double)
#undef QUIVER_K2_BM_DEFINE
// NOLINTEND(bugprone-macro-parentheses)

}  // namespace detail::avx512
QUIVER_TARGET_AVX512_END
QUIVER_END_NAMESPACE

#endif  // x86-64
