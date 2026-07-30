// K8 unpack — AVX-512 backend (ADR-026). Byte-aligned widths w ∈ {8, 16, 32, 64=full} use
// 512-bit widening loads (`_mm512_cvtepu*`); every sub-byte / irregular width routes through
// the scalar reference's per-value bit gather (bit-identical by definition). Each specialized
// block reads exactly w*lanes/8 bytes, inside the ceil(n*w/8) bound (REQ-SEC-004); tails are
// scalar (ADR-015). Structurally identical to the AVX2 backend at 512-bit width.
// Base set F+BW+DQ+VL only (REQ-SIMD-004); correct on SDE -skx.
// Module: MOD-K8-UNPACK | REQs: REQ-K8-001..004, REQ-SEC-004 | ADR-003, ADR-026
#include "src/kernels/common/target_regions.h"
#include "src/kernels/unpack/unpack_scalar_impl.h"

#if defined(__x86_64__) || defined(_M_X64)

#include <immintrin.h>
#include <type_traits>

QUIVER_BEGIN_NAMESPACE
QUIVER_TARGET_AVX512_BEGIN
namespace detail::avx512 {

namespace {

template <class Out>
QUIVER_FORCE_INLINE __m512i broadcast_out(Out v) noexcept {
  if constexpr (sizeof(Out) == 1) {
    return _mm512_set1_epi8(static_cast<char>(v));
  } else if constexpr (sizeof(Out) == 2) {
    return _mm512_set1_epi16(static_cast<short>(v));
  } else if constexpr (sizeof(Out) == 4) {
    return _mm512_set1_epi32(static_cast<int>(v));
  } else {
    return _mm512_set1_epi64(static_cast<long long>(v));
  }
}

template <class Out>
QUIVER_FORCE_INLINE __m512i add_out(__m512i a, __m512i b) noexcept {
  if constexpr (sizeof(Out) == 1) {
    return _mm512_add_epi8(a, b);
  } else if constexpr (sizeof(Out) == 2) {
    return _mm512_add_epi16(a, b);
  } else if constexpr (sizeof(Out) == 4) {
    return _mm512_add_epi32(a, b);
  } else {
    return _mm512_add_epi64(a, b);
  }
}

// One 512-bit vector of Out lanes from `kSrcBytes`-byte packed values (zero-extension); the
// load touches exactly kSrcBytes * (64/sizeof(Out)) bytes.
template <class Out, int kSrcBytes>
QUIVER_FORCE_INLINE __m512i widen_load(const std::uint8_t* p) noexcept {
  if constexpr (kSrcBytes == static_cast<int>(sizeof(Out))) {
    return _mm512_loadu_si512(reinterpret_cast<const void*>(p));
  } else if constexpr (kSrcBytes == 1 && sizeof(Out) == 2) {
    return _mm512_cvtepu8_epi16(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(p)));
  } else if constexpr (kSrcBytes == 1 && sizeof(Out) == 4) {
    return _mm512_cvtepu8_epi32(_mm_loadu_si128(reinterpret_cast<const __m128i*>(p)));
  } else if constexpr (kSrcBytes == 1 && sizeof(Out) == 8) {
    return _mm512_cvtepu8_epi64(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(p)));
  } else if constexpr (kSrcBytes == 2 && sizeof(Out) == 4) {
    return _mm512_cvtepu16_epi32(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(p)));
  } else if constexpr (kSrcBytes == 2 && sizeof(Out) == 8) {
    return _mm512_cvtepu16_epi64(_mm_loadu_si128(reinterpret_cast<const __m128i*>(p)));
  } else {  // kSrcBytes == 4 && sizeof(Out) == 8
    return _mm512_cvtepu32_epi64(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(p)));
  }
}

// Byte-aligned width kernel: w = 8*kSrcBytes bits per value.
template <class Out, int kSrcBytes>
void unpack_bytes(const std::uint8_t* packed, std::int64_t n, Out base, Out* out) noexcept {
  constexpr std::int64_t kLanes = static_cast<std::int64_t>(64 / sizeof(Out));
  const __m512i vbase = broadcast_out(base);
  std::int64_t i = 0;
  for (; i + kLanes <= n; i += kLanes) {
    const __m512i v = widen_load<Out, kSrcBytes>(packed + i * kSrcBytes);
    _mm512_storeu_si512(reinterpret_cast<void*>(out + i), add_out<Out>(v, vbase));
  }
  for (; i < n; ++i) {  // scalar tail, same gather arithmetic
    const std::uint64_t v =
        scalar_impl::gather_bits(packed, i * (std::int64_t{8} * kSrcBytes), 8u * kSrcBytes);
    out[i] = static_cast<Out>(base + static_cast<Out>(v));
  }
}

template <class Out>
void unpack_impl(scalar_impl::PackedStream in, Out base, Out* out) noexcept {
  const std::uint8_t* packed = in.packed;
  const std::int64_t n = in.n;
  const int bit_width = in.bit_width;
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
  scalar_impl::unpack<Out>({packed, n, bit_width}, base, out);  // generic gather path
}

}  // namespace

// NOLINTBEGIN(bugprone-macro-parentheses): T expands to type names inside declarators.
#define QUIVER_K8_DEFINE(T)                                                                        \
  void k8_unpack(const std::uint8_t* packed, std::int64_t n, int bit_width, T base,                \
                 T* out) noexcept {                                                                \
    unpack_impl<T>({packed, n, bit_width}, base, out);                                             \
  }

QUIVER_K8_DEFINE(std::uint8_t)
QUIVER_K8_DEFINE(std::uint16_t)
QUIVER_K8_DEFINE(std::uint32_t)
QUIVER_K8_DEFINE(std::uint64_t)
#undef QUIVER_K8_DEFINE
// NOLINTEND(bugprone-macro-parentheses)

}  // namespace detail::avx512
QUIVER_TARGET_AVX512_END
QUIVER_END_NAMESPACE

#endif  // x86-64
