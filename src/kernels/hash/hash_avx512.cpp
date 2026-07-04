// K7 hash — AVX-512 backend (ADR-012 frozen). Vertical fmix64 over 8 lanes; the 64x64->low64
// multiply is a NATIVE `vpmullq` (`_mm512_mullo_epi64`, AVX-512DQ — in the base required set),
// so no 3x-vpmuludq decomposition (the AVX2 path) is needed. key64 widening per type; floats
// canonicalize -0.0 to +0.0 via an opmask blend, exactly like the scalar reference. Tails are
// scalar (ADR-015). Results are bit-identical to hash_scalar_impl.h (REQ-K7-002) — validated
// against the committed golden vectors under Intel SDE.
// Base set F+BW+DQ+VL only (REQ-SIMD-004); correct on SDE -skx.
// Module: MOD-K7-HASH | REQs: REQ-K7-001..002, REQ-SIMD-001..004 | ADR-003, ADR-012
#include "src/kernels/common/target_regions.h"
#include "src/kernels/hash/hash_scalar_impl.h"

#if defined(__x86_64__) || defined(_M_X64)

#include <cstring>
#include <immintrin.h>
#include <type_traits>

QUIVER_BEGIN_NAMESPACE
QUIVER_TARGET_AVX512_BEGIN
namespace detail::avx512 {

namespace {

QUIVER_FORCE_INLINE __m512i fmix64_vec(__m512i x) noexcept {
  const __m512i c1 = _mm512_set1_epi64(static_cast<long long>(scalar_impl::kHashC1));
  const __m512i c2 = _mm512_set1_epi64(static_cast<long long>(scalar_impl::kHashC2));
  x = _mm512_xor_si512(x, _mm512_srli_epi64(x, 33));
  x = _mm512_mullo_epi64(x, c1);  // native vpmullq (AVX-512DQ)
  x = _mm512_xor_si512(x, _mm512_srli_epi64(x, 33));
  x = _mm512_mullo_epi64(x, c2);
  x = _mm512_xor_si512(x, _mm512_srli_epi64(x, 33));
  return x;
}

// key64 for 8 consecutive elements as 64-bit lanes (zero-extension; floats canonicalize
// -0.0 -> +0.0 first, exactly like the scalar reference).
template <class T>
QUIVER_FORCE_INLINE __m512i key64_vec(const T* p) noexcept {
  if constexpr (std::is_same_v<T, float>) {
    __m256 v = _mm256_loadu_ps(p);
    const __mmask8 zero = _mm256_cmp_ps_mask(v, _mm256_setzero_ps(), _CMP_EQ_OQ);
    v = _mm256_mask_mov_ps(v, zero, _mm256_setzero_ps());  // +-0.0 -> +0.0
    return _mm512_cvtepu32_epi64(_mm256_castps_si256(v));
  } else if constexpr (std::is_same_v<T, double>) {
    __m512d v = _mm512_loadu_pd(p);
    const __mmask8 zero = _mm512_cmp_pd_mask(v, _mm512_setzero_pd(), _CMP_EQ_OQ);
    v = _mm512_mask_mov_pd(v, zero, _mm512_setzero_pd());
    return _mm512_castpd_si512(v);
  } else if constexpr (sizeof(T) == 8) {
    return _mm512_loadu_si512(reinterpret_cast<const void*>(p));
  } else if constexpr (sizeof(T) == 4) {
    return _mm512_cvtepu32_epi64(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(p)));
  } else if constexpr (sizeof(T) == 2) {
    return _mm512_cvtepu16_epi64(_mm_loadu_si128(reinterpret_cast<const __m128i*>(p)));
  } else {  // 1-byte
    return _mm512_cvtepu8_epi64(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(p)));
  }
}

template <class T>
void hash64_impl(const T* in, std::int64_t n, std::uint64_t seed, std::uint64_t* out) noexcept {
  const std::uint64_t premix = seed + scalar_impl::kHashGolden;
  const __m512i premix_v = _mm512_set1_epi64(static_cast<long long>(premix));
  std::int64_t i = 0;
  for (; i + 8 <= n; i += 8) {
    const __m512i h = fmix64_vec(_mm512_xor_si512(key64_vec(in + i), premix_v));
    _mm512_storeu_si512(reinterpret_cast<void*>(out + i), h);
  }
  for (; i < n; ++i) {  // scalar tail (ADR-015), identical arithmetic
    out[i] = scalar_impl::fmix64(scalar_impl::key64(in[i]) ^ premix);
  }
}

}  // namespace

// NOLINTBEGIN(bugprone-macro-parentheses): T expands to type names inside declarators.
#define QUIVER_K7_DEFINE(T)                                                                        \
  void k7_hash64(const T* in, std::int64_t n, std::uint64_t seed, std::uint64_t* out) noexcept {   \
    hash64_impl<T>(in, n, seed, out);                                                              \
  }

QUIVER_K7_DEFINE(std::int8_t)
QUIVER_K7_DEFINE(std::int16_t)
QUIVER_K7_DEFINE(std::int32_t)
QUIVER_K7_DEFINE(std::int64_t)
QUIVER_K7_DEFINE(std::uint8_t)
QUIVER_K7_DEFINE(std::uint16_t)
QUIVER_K7_DEFINE(std::uint32_t)
QUIVER_K7_DEFINE(std::uint64_t)
QUIVER_K7_DEFINE(float)
QUIVER_K7_DEFINE(double)
#undef QUIVER_K7_DEFINE
// NOLINTEND(bugprone-macro-parentheses)

void k7_hash64_combine(const std::uint64_t* a, const std::uint64_t* b, std::int64_t n,
                       std::uint64_t* out) noexcept {
  const __m512i golden = _mm512_set1_epi64(static_cast<long long>(scalar_impl::kHashGolden));
  std::int64_t i = 0;
  for (; i + 8 <= n; i += 8) {
    const __m512i va = _mm512_loadu_si512(reinterpret_cast<const void*>(a + i));
    const __m512i vb = _mm512_loadu_si512(reinterpret_cast<const void*>(b + i));
    // b + GOLDEN + (a << 6) + (a >> 2), then a ^ ..., then fmix64 — the frozen combine.
    __m512i t = _mm512_add_epi64(vb, golden);
    t = _mm512_add_epi64(t, _mm512_slli_epi64(va, 6));
    t = _mm512_add_epi64(t, _mm512_srli_epi64(va, 2));
    const __m512i h = fmix64_vec(_mm512_xor_si512(va, t));
    _mm512_storeu_si512(reinterpret_cast<void*>(out + i), h);
  }
  for (; i < n; ++i) {
    out[i] = scalar_impl::combine_one(a[i], b[i]);
  }
}

}  // namespace detail::avx512
QUIVER_TARGET_AVX512_END
QUIVER_END_NAMESPACE

#endif  // x86-64
