// K5 take / dict_decode — AVX2 backend. The K5 technique question is EVIDENCE-GATED
// (PRD 08 §5 K5): hardware gather (vpgatherdd/q) vs scalar unrolled independent loads.
// Both paths are compiled in this TU; kUseGatherTake selects the shipped default =
// SCALAR (Survey §4.2: gather does not beat scalar MLP loads on Haswell..Zen 3 for
// cache-resident batches; no ledger hardware is available yet to overturn that prior —
// decision + reopening criteria recorded in the M4 gate). Note the gather path's extra
// domain constraint: vpgatherd* sign-extends 32-bit indices, so it is only usable while
// idx values < 2^31 (debug-asserted); the scalar default has no such limit.
// dict_decode delegates to the scalar core: the fused selected-decode is random-access
// dominated and its bounds-check structure must stay identical (REQ-K5-002/-003).
// All paths are bit-identical to take_scalar_impl.h (REQ-KERNEL-002).
// Module: MOD-K5-TAKE | REQs: REQ-K5-001..003, REQ-SIMD-001..003 | ADR-003
#include "src/kernels/common/target_regions.h"
#include "src/kernels/take/take_scalar_impl.h"

#if defined(__x86_64__) || defined(_M_X64)

#include <immintrin.h>

QUIVER_BEGIN_NAMESPACE
QUIVER_TARGET_AVX2_BEGIN
namespace detail::avx2 {

namespace {

// Evidence-gated technique switch (PRD 08 K5): flip only with ledger data recorded in the
// family doc; both branches below stay compiled either way.
constexpr bool kUseGatherTake = false;

template <class T>
void take_gather(const T* values, std::int64_t values_len, const std::uint32_t* idx,
                 std::int64_t idx_len, T* out) noexcept {
  static_assert(sizeof(T) == 4 || sizeof(T) == 8, "gather path is 32/64-bit lanes only");
#if defined(QUIVER_ENABLE_ASSERTS)
  for (std::int64_t j = 0; j < idx_len; ++j) {
    QUIVER_ASSERT(idx[j] < static_cast<std::uint64_t>(values_len),
                  "take: index out of bounds [REQ-K5-002]");
    QUIVER_ASSERT(idx[j] < (1u << 31), "take(gather): vpgatherd* sign-extends 32-bit indices");
  }
#else
  (void)values_len;
#endif
  std::int64_t j = 0;
  if constexpr (sizeof(T) == 4) {
    for (; j + 8 <= idx_len; j += 8) {
      const __m256i vidx = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(idx + j));
      if constexpr (std::is_same_v<T, float>) {
        _mm256_storeu_ps(out + j, _mm256_i32gather_ps(values, vidx, 4));
      } else {
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(out + j),
                            _mm256_i32gather_epi32(reinterpret_cast<const int*>(values), vidx, 4));
      }
    }
  } else {
    for (; j + 4 <= idx_len; j += 4) {
      const __m128i vidx = _mm_loadu_si128(reinterpret_cast<const __m128i*>(idx + j));
      if constexpr (std::is_same_v<T, double>) {
        _mm256_storeu_pd(out + j, _mm256_i32gather_pd(values, vidx, 8));
      } else {
        _mm256_storeu_si256(
            reinterpret_cast<__m256i*>(out + j),
            _mm256_i32gather_epi64(reinterpret_cast<const long long*>(values), vidx, 8));
      }
    }
  }
  for (; j < idx_len; ++j) {
    out[j] = values[idx[j]];
  }
}

template <class T>
QUIVER_FORCE_INLINE void take_dispatch(const T* values, std::int64_t values_len,
                                       const std::uint32_t* idx, std::int64_t idx_len,
                                       T* out) noexcept {
  if constexpr (sizeof(T) >= 4) {
    if (kUseGatherTake) {
      take_gather<T>(values, values_len, idx, idx_len, out);
      return;
    }
  }
  scalar_impl::take<T>(values, values_len, idx, idx_len, out);
}

}  // namespace

// NOLINTBEGIN(bugprone-macro-parentheses): T/C expand to type names inside declarators.
#define QUIVER_K5_TAKE_DEFINE(T)                                                                   \
  void k5_take(const T* values, std::int64_t values_len, const std::uint32_t* idx,                 \
               std::int64_t idx_len, T* out) noexcept {                                            \
    take_dispatch<T>(values, values_len, idx, idx_len, out);                                       \
  }

#define QUIVER_K5_DECODE_DEFINE(T, C)                                                              \
  void k5_dict_decode(const T* dict, std::int64_t dict_len, const C* codes, std::int64_t n,        \
                      const std::uint32_t* sel, std::int64_t sel_len, T* out) noexcept {           \
    return sel == nullptr                                                                          \
               ? scalar_impl::dict_decode<T, C>(dict, dict_len, codes, n, out)                     \
               : scalar_impl::dict_decode_sel<T, C>(dict, dict_len, codes, sel, sel_len, out);     \
  }

#define QUIVER_K5_DEFINE(T)                                                                        \
  QUIVER_K5_TAKE_DEFINE(T)                                                                         \
  QUIVER_K5_DECODE_DEFINE(T, std::uint8_t)                                                         \
  QUIVER_K5_DECODE_DEFINE(T, std::uint16_t)                                                        \
  QUIVER_K5_DECODE_DEFINE(T, std::uint32_t)

QUIVER_K5_DEFINE(std::int8_t)
QUIVER_K5_DEFINE(std::int16_t)
QUIVER_K5_DEFINE(std::int32_t)
QUIVER_K5_DEFINE(std::int64_t)
QUIVER_K5_DEFINE(std::uint8_t)
QUIVER_K5_DEFINE(std::uint16_t)
QUIVER_K5_DEFINE(std::uint32_t)
QUIVER_K5_DEFINE(std::uint64_t)
QUIVER_K5_DEFINE(float)
QUIVER_K5_DEFINE(double)
#undef QUIVER_K5_DEFINE
#undef QUIVER_K5_DECODE_DEFINE
#undef QUIVER_K5_TAKE_DEFINE
// NOLINTEND(bugprone-macro-parentheses)

}  // namespace detail::avx2
QUIVER_TARGET_AVX2_END
QUIVER_END_NAMESPACE

#endif  // x86-64
