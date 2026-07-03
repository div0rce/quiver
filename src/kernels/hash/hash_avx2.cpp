// K7 hash — AVX2 backend (PRD 08 §5 K7; ADR-012). Vertical fmix64 over 4 lanes: the 64x64
// multiply AVX2 lacks is decomposed into three vpmuludq 32x32 partial products (the
// documented standard technique): lo(a)*lo(b) + ((lo(a)*hi(b) + hi(a)*lo(b)) << 32) — the
// hi*hi term only feeds bits >= 64 and drops out of the wrapping product. Constants are the
// ADR-012 frozen set; results are bit-identical to hash_scalar_impl.h on every input
// (REQ-K7-002 — verified against the committed golden vectors and the differential suite).
// key64 widening per type: integers zero-extend (cvtepu*), floats canonicalize -0.0 to +0.0
// then reinterpret. Tails are scalar (ADR-015).
// Module: MOD-K7-HASH | REQs: REQ-K7-001..002, REQ-SIMD-001..003/-007 | ADR-003, ADR-012
#include "src/kernels/common/target_regions.h"
#include "src/kernels/hash/hash_scalar_impl.h"

#if defined(__x86_64__) || defined(_M_X64)

#include <cstring>
#include <immintrin.h>
#include <type_traits>

QUIVER_BEGIN_NAMESPACE
QUIVER_TARGET_AVX2_BEGIN
namespace detail::avx2 {

namespace {

// Wrapping 64x64 -> low-64 multiply via three vpmuludq partials (no vpmullq before AVX-512).
QUIVER_FORCE_INLINE __m256i mul64_lo(__m256i a, __m256i b) noexcept {
  const __m256i a_hi = _mm256_srli_epi64(a, 32);
  const __m256i b_hi = _mm256_srli_epi64(b, 32);
  const __m256i lo_lo = _mm256_mul_epu32(a, b);        // lo(a)*lo(b), full 64
  const __m256i lo_hi = _mm256_mul_epu32(a, b_hi);     // lo(a)*hi(b)
  const __m256i hi_lo = _mm256_mul_epu32(a_hi, b);     // hi(a)*lo(b)
  const __m256i cross = _mm256_add_epi64(lo_hi, hi_lo);
  return _mm256_add_epi64(lo_lo, _mm256_slli_epi64(cross, 32));
}

QUIVER_FORCE_INLINE __m256i fmix64_vec(__m256i x) noexcept {
  const __m256i c1 = _mm256_set1_epi64x(static_cast<long long>(scalar_impl::kHashC1));
  const __m256i c2 = _mm256_set1_epi64x(static_cast<long long>(scalar_impl::kHashC2));
  x = _mm256_xor_si256(x, _mm256_srli_epi64(x, 33));
  x = mul64_lo(x, c1);
  x = _mm256_xor_si256(x, _mm256_srli_epi64(x, 33));
  x = mul64_lo(x, c2);
  x = _mm256_xor_si256(x, _mm256_srli_epi64(x, 33));
  return x;
}

// key64 for 4 consecutive elements, widened to 64-bit lanes (zero-extension; floats
// canonicalize -0.0 -> +0.0 first, exactly like the scalar reference).
template <class T>
QUIVER_FORCE_INLINE __m256i key64_vec(const T* p) noexcept {
  if constexpr (std::is_same_v<T, float>) {
    __m128 v = _mm_loadu_ps(p);
    const __m128 zero_mask = _mm_cmpeq_ps(v, _mm_setzero_ps());  // true for +-0.0 only
    v = _mm_andnot_ps(zero_mask, v);                             // zeros -> +0.0 bit pattern
    return _mm256_cvtepu32_epi64(_mm_castps_si128(v));
  } else if constexpr (std::is_same_v<T, double>) {
    __m256d v = _mm256_loadu_pd(p);
    const __m256d zero_mask = _mm256_cmp_pd(v, _mm256_setzero_pd(), _CMP_EQ_OQ);
    v = _mm256_andnot_pd(zero_mask, v);
    return _mm256_castpd_si256(v);
  } else if constexpr (sizeof(T) == 8) {
    return _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p));
  } else if constexpr (sizeof(T) == 4) {
    return _mm256_cvtepu32_epi64(_mm_loadu_si128(reinterpret_cast<const __m128i*>(p)));
  } else if constexpr (sizeof(T) == 2) {
    return _mm256_cvtepu16_epi64(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(p)));
  } else {
    std::uint32_t raw = 0;
    std::memcpy(&raw, p, 4);
    return _mm256_cvtepu8_epi64(_mm_cvtsi32_si128(static_cast<int>(raw)));
  }
}

template <class T>
void hash64_impl(const T* in, std::int64_t n, std::uint64_t seed, std::uint64_t* out) noexcept {
  const std::uint64_t premix = seed + scalar_impl::kHashGolden;
  const __m256i premix_v = _mm256_set1_epi64x(static_cast<long long>(premix));
  std::int64_t i = 0;
  for (; i + 4 <= n; i += 4) {
    const __m256i keys = key64_vec(in + i);
    const __m256i h = fmix64_vec(_mm256_xor_si256(keys, premix_v));
    _mm256_storeu_si256(reinterpret_cast<__m256i*>(out + i), h);
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
  const __m256i golden = _mm256_set1_epi64x(static_cast<long long>(scalar_impl::kHashGolden));
  std::int64_t i = 0;
  for (; i + 4 <= n; i += 4) {
    const __m256i va = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(a + i));
    const __m256i vb = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(b + i));
    // b + GOLDEN + (a << 6) + (a >> 2), then a ^ ..., then fmix64 — the frozen combine.
    __m256i t = _mm256_add_epi64(vb, golden);
    t = _mm256_add_epi64(t, _mm256_slli_epi64(va, 6));
    t = _mm256_add_epi64(t, _mm256_srli_epi64(va, 2));
    const __m256i h = fmix64_vec(_mm256_xor_si256(va, t));
    _mm256_storeu_si256(reinterpret_cast<__m256i*>(out + i), h);
  }
  for (; i < n; ++i) {
    out[i] = scalar_impl::combine_one(a[i], b[i]);
  }
}

}  // namespace detail::avx2
QUIVER_TARGET_AVX2_END
QUIVER_END_NAMESPACE

#endif  // x86-64
