// K9 arith — AVX-512 backend (ADR-006): direct vertical add/sub/mul per lane width; the 64-bit
// multiply is a native `vpmullq` (`_mm512_mullo_epi64`, AVX-512DQ — base set), unlike AVX2's
// 3x-vpmuludq decomposition. 8-bit multiply widens even/odd bytes to 16-bit and repacks (no
// direct 8-bit multiply). Integer wrapping is the instructions' native modular behavior,
// matching the reference's unsigned-internal wrap; floats are native IEEE. Exact aliasing
// out == a/b is safe (loads complete before the store). Tails scalar (ADR-015).
// Base set F+BW+DQ+VL only (REQ-SIMD-004); correct on SDE -skx.
// Module: MOD-K9-ARITH | REQs: REQ-K9-001, REQ-SIMD-001..004 | ADR-003
#include "src/kernels/arith/arith_scalar_impl.h"
#include "src/kernels/common/target_regions.h"

#if defined(__x86_64__) || defined(_M_X64)

#include <immintrin.h>
#include <type_traits>

QUIVER_BEGIN_NAMESPACE
QUIVER_TARGET_AVX512_BEGIN
namespace detail::avx512 {

namespace {

// Modular 8-bit multiply: widen even/odd bytes to 16-bit, multiply, repack lows.
QUIVER_FORCE_INLINE __m512i mul8_lo(__m512i a, __m512i b) noexcept {
  const __m512i lo_mask = _mm512_set1_epi16(0x00FF);
  const __m512i even =
      _mm512_mullo_epi16(_mm512_and_si512(a, lo_mask), _mm512_and_si512(b, lo_mask));
  const __m512i odd = _mm512_mullo_epi16(_mm512_srli_epi16(a, 8), _mm512_srli_epi16(b, 8));
  return _mm512_or_si512(_mm512_and_si512(even, lo_mask), _mm512_slli_epi16(odd, 8));
}

template <class T>
QUIVER_FORCE_INLINE __m512i apply_op(ArithOp op, __m512i a, __m512i b) noexcept {
  if constexpr (sizeof(T) == 1) {
    switch (op) {
    case ArithOp::kAdd:
      return _mm512_add_epi8(a, b);
    case ArithOp::kSub:
      return _mm512_sub_epi8(a, b);
    case ArithOp::kMul:
      return mul8_lo(a, b);
    }
  } else if constexpr (sizeof(T) == 2) {
    switch (op) {
    case ArithOp::kAdd:
      return _mm512_add_epi16(a, b);
    case ArithOp::kSub:
      return _mm512_sub_epi16(a, b);
    case ArithOp::kMul:
      return _mm512_mullo_epi16(a, b);
    }
  } else if constexpr (sizeof(T) == 4) {
    switch (op) {
    case ArithOp::kAdd:
      return _mm512_add_epi32(a, b);
    case ArithOp::kSub:
      return _mm512_sub_epi32(a, b);
    case ArithOp::kMul:
      return _mm512_mullo_epi32(a, b);
    }
  } else {
    switch (op) {
    case ArithOp::kAdd:
      return _mm512_add_epi64(a, b);
    case ArithOp::kSub:
      return _mm512_sub_epi64(a, b);
    case ArithOp::kMul:
      return _mm512_mullo_epi64(a, b);  // native vpmullq (AVX-512DQ)
    }
  }
  return a;  // unreachable for in-contract op values
}

template <class T, class LoadB>
void arith_impl(ArithOp op, const T* a, LoadB load_b, std::int64_t n, T* out) noexcept {
  constexpr std::int64_t kW = static_cast<std::int64_t>(64 / sizeof(T));
  std::int64_t i = 0;
  if constexpr (std::is_same_v<T, float>) {
    for (; i + 16 <= n; i += 16) {
      const __m512 va = _mm512_loadu_ps(a + i);
      const __m512 vb = load_b(i);
      __m512 r;
      switch (op) {
      case ArithOp::kAdd:
        r = _mm512_add_ps(va, vb);
        break;
      case ArithOp::kSub:
        r = _mm512_sub_ps(va, vb);
        break;
      default:
        r = _mm512_mul_ps(va, vb);
        break;
      }
      _mm512_storeu_ps(out + i, r);
    }
  } else if constexpr (std::is_same_v<T, double>) {
    for (; i + 8 <= n; i += 8) {
      const __m512d va = _mm512_loadu_pd(a + i);
      const __m512d vb = load_b(i);
      __m512d r;
      switch (op) {
      case ArithOp::kAdd:
        r = _mm512_add_pd(va, vb);
        break;
      case ArithOp::kSub:
        r = _mm512_sub_pd(va, vb);
        break;
      default:
        r = _mm512_mul_pd(va, vb);
        break;
      }
      _mm512_storeu_pd(out + i, r);
    }
  } else {
    for (; i + kW <= n; i += kW) {
      const __m512i va = _mm512_loadu_si512(reinterpret_cast<const void*>(a + i));
      const __m512i vb = load_b(i);
      _mm512_storeu_si512(reinterpret_cast<void*>(out + i), apply_op<T>(op, va, vb));
    }
  }
  for (; i < n; ++i) {  // scalar tail, identical arithmetic (floats: same IEEE op)
    out[i] = scalar_impl::arith_one(op, a[i], load_b.tail(i));
  }
}

template <class T>
struct BatchRhs {
  const T* b;
  auto operator()(std::int64_t i) const noexcept {
    if constexpr (std::is_same_v<T, float>) {
      return _mm512_loadu_ps(b + i);
    } else if constexpr (std::is_same_v<T, double>) {
      return _mm512_loadu_pd(b + i);
    } else {
      return _mm512_loadu_si512(reinterpret_cast<const void*>(b + i));
    }
  }
  T tail(std::int64_t i) const noexcept { return b[i]; }
};

template <class T>
struct ScalarRhs {
  T b;
  auto operator()(std::int64_t) const noexcept {
    if constexpr (std::is_same_v<T, float>) {
      return _mm512_set1_ps(b);
    } else if constexpr (std::is_same_v<T, double>) {
      return _mm512_set1_pd(b);
    } else if constexpr (sizeof(T) == 1) {
      return _mm512_set1_epi8(static_cast<char>(b));
    } else if constexpr (sizeof(T) == 2) {
      return _mm512_set1_epi16(static_cast<short>(b));
    } else if constexpr (sizeof(T) == 4) {
      return _mm512_set1_epi32(static_cast<int>(b));
    } else {
      return _mm512_set1_epi64(static_cast<long long>(b));
    }
  }
  T tail(std::int64_t) const noexcept { return b; }
};

}  // namespace

// NOLINTBEGIN(bugprone-macro-parentheses): T expands to type names inside declarators.
#define QUIVER_K9_DEFINE(T)                                                                        \
  void k9_arith(ArithOp op, const T* a, const T* b, std::int64_t n, T* out) noexcept {             \
    arith_impl<T>(op, a, BatchRhs<T>{b}, n, out);                                                  \
  }                                                                                                \
  void k9_arith_scalar_rhs(ArithOp op, const T* a, T b, std::int64_t n, T* out) noexcept {         \
    arith_impl<T>(op, a, ScalarRhs<T>{b}, n, out);                                                 \
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

}  // namespace detail::avx512
QUIVER_TARGET_AVX512_END
QUIVER_END_NAMESPACE

#endif  // x86-64
