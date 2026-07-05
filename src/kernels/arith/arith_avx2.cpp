// K9 arith — AVX2 backend (PRD 08 §5 K9): direct SIMD add/sub/mul per lane width; 64-bit
// multiply via the same three-vpmuludq decomposition as K7; floats native. Integer wrapping
// is the vector instructions' native behavior (paddb/psubw/pmullw... are modular), matching
// the reference's unsigned-internal wrap exactly. 8-bit multiply has no direct instruction:
// widen to 16-bit, multiply, and repack the low bytes (even/odd interleave — exact modular
// product). Exact aliasing out == a/b is safe (loads complete before the store per block).
// Tails scalar (ADR-015). Bit-identical to arith_scalar_impl.h (REQ-KERNEL-002).
// Module: MOD-K9-ARITH | REQs: REQ-K9-001, REQ-SIMD-001..003/-007 | ADR-003
#include "src/kernels/arith/arith_scalar_impl.h"
#include "src/kernels/common/target_regions.h"

#if defined(__x86_64__) || defined(_M_X64)

#include <immintrin.h>
#include <type_traits>

QUIVER_BEGIN_NAMESPACE
QUIVER_TARGET_AVX2_BEGIN
namespace detail::avx2 {

namespace {

QUIVER_FORCE_INLINE __m256i ar_mul64_lo(__m256i a, __m256i b) noexcept {
  const __m256i cross = _mm256_add_epi64(_mm256_mul_epu32(a, _mm256_srli_epi64(b, 32)),
                                         _mm256_mul_epu32(_mm256_srli_epi64(a, 32), b));
  return _mm256_add_epi64(_mm256_mul_epu32(a, b), _mm256_slli_epi64(cross, 32));
}

// Modular 8-bit multiply: widen even/odd bytes to 16-bit via masking, multiply, repack lows.
QUIVER_FORCE_INLINE __m256i mul8_lo(__m256i a, __m256i b) noexcept {
  const __m256i lo_mask = _mm256_set1_epi16(0x00FF);
  const __m256i even =
      _mm256_mullo_epi16(_mm256_and_si256(a, lo_mask), _mm256_and_si256(b, lo_mask));
  const __m256i odd = _mm256_mullo_epi16(_mm256_srli_epi16(a, 8), _mm256_srli_epi16(b, 8));
  return _mm256_or_si256(_mm256_and_si256(even, lo_mask), _mm256_slli_epi16(odd, 8));
}

template <class T>
QUIVER_FORCE_INLINE __m256i apply_op(ArithOp op, __m256i a, __m256i b) noexcept {
  if constexpr (sizeof(T) == 1) {
    switch (op) {
    case ArithOp::kAdd:
      return _mm256_add_epi8(a, b);
    case ArithOp::kSub:
      return _mm256_sub_epi8(a, b);
    case ArithOp::kMul:
      return mul8_lo(a, b);
    }
  } else if constexpr (sizeof(T) == 2) {
    switch (op) {
    case ArithOp::kAdd:
      return _mm256_add_epi16(a, b);
    case ArithOp::kSub:
      return _mm256_sub_epi16(a, b);
    case ArithOp::kMul:
      return _mm256_mullo_epi16(a, b);
    }
  } else if constexpr (sizeof(T) == 4) {
    switch (op) {
    case ArithOp::kAdd:
      return _mm256_add_epi32(a, b);
    case ArithOp::kSub:
      return _mm256_sub_epi32(a, b);
    case ArithOp::kMul:
      return _mm256_mullo_epi32(a, b);
    }
  } else {
    switch (op) {
    case ArithOp::kAdd:
      return _mm256_add_epi64(a, b);
    case ArithOp::kSub:
      return _mm256_sub_epi64(a, b);
    case ArithOp::kMul:
      return ar_mul64_lo(a, b);
    }
  }
  return a;  // unreachable for in-contract op values
}

template <class T, class LoadB>
void arith_impl(ArithOp op, const T* a, LoadB load_b, std::int64_t n, T* out) noexcept {
  constexpr std::int64_t kW = static_cast<std::int64_t>(32 / sizeof(T));
  std::int64_t i = 0;
  if constexpr (std::is_same_v<T, float>) {
    for (; i + 8 <= n; i += 8) {
      const __m256 va = _mm256_loadu_ps(a + i);
      const __m256 vb = load_b(i);
      __m256 r;
      switch (op) {
      case ArithOp::kAdd:
        r = _mm256_add_ps(va, vb);
        break;
      case ArithOp::kSub:
        r = _mm256_sub_ps(va, vb);
        break;
      default:
        r = _mm256_mul_ps(va, vb);
        break;
      }
      _mm256_storeu_ps(out + i, r);
    }
  } else if constexpr (std::is_same_v<T, double>) {
    for (; i + 4 <= n; i += 4) {
      const __m256d va = _mm256_loadu_pd(a + i);
      const __m256d vb = load_b(i);
      __m256d r;
      switch (op) {
      case ArithOp::kAdd:
        r = _mm256_add_pd(va, vb);
        break;
      case ArithOp::kSub:
        r = _mm256_sub_pd(va, vb);
        break;
      default:
        r = _mm256_mul_pd(va, vb);
        break;
      }
      _mm256_storeu_pd(out + i, r);
    }
  } else {
    for (; i + kW <= n; i += kW) {
      const __m256i va = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(a + i));
      const __m256i vb = load_b(i);
      _mm256_storeu_si256(reinterpret_cast<__m256i*>(out + i), apply_op<T>(op, va, vb));
    }
  }
  for (; i < n; ++i) {  // scalar tail, identical arithmetic (floats: same IEEE op)
    out[i] = scalar_impl::arith_one(op, a[i], load_b.tail(i));
  }
}

template <class T>
struct ar_BatchRhs {
  const T* b;
  auto operator()(std::int64_t i) const noexcept {
    if constexpr (std::is_same_v<T, float>) {
      return _mm256_loadu_ps(b + i);
    } else if constexpr (std::is_same_v<T, double>) {
      return _mm256_loadu_pd(b + i);
    } else {
      return _mm256_loadu_si256(reinterpret_cast<const __m256i*>(b + i));
    }
  }
  T tail(std::int64_t i) const noexcept { return b[i]; }
};

template <class T>
struct ar_ScalarRhs {
  T b;
  auto operator()(std::int64_t) const noexcept {
    if constexpr (std::is_same_v<T, float>) {
      return _mm256_set1_ps(b);
    } else if constexpr (std::is_same_v<T, double>) {
      return _mm256_set1_pd(b);
    } else if constexpr (sizeof(T) == 1) {
      return _mm256_set1_epi8(static_cast<char>(b));
    } else if constexpr (sizeof(T) == 2) {
      return _mm256_set1_epi16(static_cast<short>(b));
    } else if constexpr (sizeof(T) == 4) {
      return _mm256_set1_epi32(static_cast<int>(b));
    } else {
      return _mm256_set1_epi64x(static_cast<long long>(b));
    }
  }
  T tail(std::int64_t) const noexcept { return b; }
};

}  // namespace

// NOLINTBEGIN(bugprone-macro-parentheses): T expands to type names inside declarators.
#define QUIVER_K9_DEFINE(T)                                                                        \
  void k9_arith(ArithOp op, const T* a, const T* b, std::int64_t n, T* out) noexcept {             \
    arith_impl<T>(op, a, ar_BatchRhs<T>{b}, n, out);                                               \
  }                                                                                                \
  void k9_arith_scalar_rhs(ArithOp op, const T* a, T b, std::int64_t n, T* out) noexcept {         \
    arith_impl<T>(op, a, ar_ScalarRhs<T>{b}, n, out);                                              \
  }

QUIVER_K9_DEFINE(std::int8_t)
QUIVER_K9_DEFINE(std::int16_t)
QUIVER_K9_DEFINE(std::int32_t)
QUIVER_K9_DEFINE(std::int64_t)
QUIVER_K9_DEFINE(std::uint8_t)
QUIVER_K9_DEFINE(std::uint16_t)
QUIVER_K9_DEFINE(std::uint32_t)
QUIVER_K9_DEFINE(std::uint64_t)
QUIVER_K9_DEFINE(float)
QUIVER_K9_DEFINE(double)
#undef QUIVER_K9_DEFINE
// NOLINTEND(bugprone-macro-parentheses)

}  // namespace detail::avx2
QUIVER_TARGET_AVX2_END
QUIVER_END_NAMESPACE

#endif  // x86-64
