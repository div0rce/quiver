// K8 unpack — AVX2 backend (PRD 08 §5 K8; ADR-026). Specialized kernels for the
// byte-aligned widths w ∈ {8, 16, 32} (vector widening loads: cvtepu8/16/32 as Out
// requires) and w == 8*sizeof(Out) (straight copy + FOR add); every other width — the
// sub-byte {1, 2, 4} set and all irregular widths — routes through the scalar reference's
// per-value bit gather (bit-identical by definition). Vectorizing the sub-byte widths via
// pshufb expansion is ledger-gated follow-up recorded on the family page and gate M6.
// SECURITY: every specialized block reads exactly its own w*lanes/8 bytes, inside the
// ceil(n*w/8) bound (REQ-K8-002/REQ-SEC-004); tails are scalar (ADR-015).
// Module: MOD-K8-UNPACK | REQs: REQ-K8-001..004, REQ-SEC-004 | ADR-003, ADR-026
#include "src/kernels/common/target_regions.h"
#include "src/kernels/unpack/unpack_scalar_impl.h"

#if defined(__x86_64__) || defined(_M_X64)

#include <cstring>
#include <immintrin.h>
#include <type_traits>

QUIVER_BEGIN_NAMESPACE
QUIVER_TARGET_AVX2_BEGIN
namespace detail::avx2 {

namespace {

template <class Out>
QUIVER_FORCE_INLINE __m256i broadcast_out(Out v) noexcept {
  if constexpr (sizeof(Out) == 1) {
    return _mm256_set1_epi8(static_cast<char>(v));
  } else if constexpr (sizeof(Out) == 2) {
    return _mm256_set1_epi16(static_cast<short>(v));
  } else if constexpr (sizeof(Out) == 4) {
    return _mm256_set1_epi32(static_cast<int>(v));
  } else {
    return _mm256_set1_epi64x(static_cast<long long>(v));
  }
}

template <class Out>
QUIVER_FORCE_INLINE __m256i add_out(__m256i a, __m256i b) noexcept {
  if constexpr (sizeof(Out) == 1) {
    return _mm256_add_epi8(a, b);
  } else if constexpr (sizeof(Out) == 2) {
    return _mm256_add_epi16(a, b);
  } else if constexpr (sizeof(Out) == 4) {
    return _mm256_add_epi32(a, b);
  } else {
    return _mm256_add_epi64(a, b);
  }
}

// One vector of Out lanes from `src_width`-byte packed values (zero-extension); the load
// touches exactly src_width * lanes bytes.
template <class Out, int kSrcBytes>
QUIVER_FORCE_INLINE __m256i widen_load(const std::uint8_t* p) noexcept {
  constexpr int kLanes = static_cast<int>(32 / sizeof(Out));
  constexpr int kLoad = kLanes * kSrcBytes;
  if constexpr (kSrcBytes == static_cast<int>(sizeof(Out))) {
    return _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p));
  } else if constexpr (kLoad == 16) {
    const __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p));
    if constexpr (kSrcBytes == 1) {
      return _mm256_cvtepu8_epi16(v);
    } else if constexpr (kSrcBytes == 2) {
      return _mm256_cvtepu16_epi32(v);
    } else {
      return _mm256_cvtepu32_epi64(v);
    }
  } else if constexpr (kLoad == 8) {
    const __m128i v = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(p));
    if constexpr (kSrcBytes == 1) {
      return _mm256_cvtepu8_epi32(v);
    } else {
      return _mm256_cvtepu16_epi64(v);
    }
  } else {  // kLoad == 4: u8 -> u64 lanes
    std::uint32_t raw = 0;
    std::memcpy(&raw, p, 4);
    return _mm256_cvtepu8_epi64(_mm_cvtsi32_si128(static_cast<int>(raw)));
  }
}

// Byte-aligned width kernel: w = 8*kSrcBytes bits per value.
template <class Out, int kSrcBytes>
void unpack_bytes(const std::uint8_t* packed, std::int64_t n, Out base, Out* out) noexcept {
  constexpr std::int64_t kLanes = static_cast<std::int64_t>(32 / sizeof(Out));
  const __m256i vbase = broadcast_out(base);
  std::int64_t i = 0;
  for (; i + kLanes <= n; i += kLanes) {
    const __m256i v = widen_load<Out, kSrcBytes>(packed + i * kSrcBytes);
    _mm256_storeu_si256(reinterpret_cast<__m256i*>(out + i), add_out<Out>(v, vbase));
  }
  for (; i < n; ++i) {  // scalar tail, same gather arithmetic
    const std::uint64_t v =
        scalar_impl::gather_bits(packed, i * (std::int64_t{8} * kSrcBytes), 8u * kSrcBytes);
    out[i] = static_cast<Out>(base + static_cast<Out>(v));
  }
}

template <class Out>
void unpack_impl(const std::uint8_t* packed, std::int64_t n, int bit_width, Out base,
                 Out* out) noexcept {
  switch (bit_width) {
  case 8:
    unpack_bytes<Out, 1>(packed, n, base, out);
    return;
  case 16:
    if constexpr (sizeof(Out) >= 2) {
      unpack_bytes<Out, 2>(packed, n, base, out);
      return;
    }
    break;
  case 32:
    if constexpr (sizeof(Out) >= 4) {
      unpack_bytes<Out, 4>(packed, n, base, out);
      return;
    }
    break;
  case 64:
    if constexpr (sizeof(Out) == 8) {
      unpack_bytes<Out, 8>(packed, n, base, out);
      return;
    }
    break;
  default:
    break;
  }
  scalar_impl::unpack<Out>(packed, n, bit_width, base, out);  // generic gather path
}

}  // namespace

// NOLINTBEGIN(bugprone-macro-parentheses): T expands to type names inside declarators.
#define QUIVER_K8_DEFINE(T)                                                                        \
  void k8_unpack(const std::uint8_t* packed, std::int64_t n, int bit_width, T base,                \
                 T* out) noexcept {                                                                \
    unpack_impl<T>(packed, n, bit_width, base, out);                                               \
  }

QUIVER_K8_DEFINE(std::uint8_t)
QUIVER_K8_DEFINE(std::uint16_t)
QUIVER_K8_DEFINE(std::uint32_t)
QUIVER_K8_DEFINE(std::uint64_t)
#undef QUIVER_K8_DEFINE
// NOLINTEND(bugprone-macro-parentheses)

}  // namespace detail::avx2
QUIVER_TARGET_AVX2_END
QUIVER_END_NAMESPACE

#endif  // x86-64
