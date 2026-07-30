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

// Integer arithmetic is sign-agnostic (wrapping add/sub and the low half of the product), so
// one specialization per lane width covers all eight integer element types.
template <int Bytes>
struct ArIntOps;

template <>
struct ArIntOps<1> {
  static QUIVER_FORCE_INLINE __m512i add(__m512i a, __m512i b) noexcept {
    return _mm512_add_epi8(a, b);
  }
  static QUIVER_FORCE_INLINE __m512i sub(__m512i a, __m512i b) noexcept {
    return _mm512_sub_epi8(a, b);
  }
  static QUIVER_FORCE_INLINE __m512i mul(__m512i a, __m512i b) noexcept { return mul8_lo(a, b); }
};

template <>
struct ArIntOps<2> {
  static QUIVER_FORCE_INLINE __m512i add(__m512i a, __m512i b) noexcept {
    return _mm512_add_epi16(a, b);
  }
  static QUIVER_FORCE_INLINE __m512i sub(__m512i a, __m512i b) noexcept {
    return _mm512_sub_epi16(a, b);
  }
  static QUIVER_FORCE_INLINE __m512i mul(__m512i a, __m512i b) noexcept {
    return _mm512_mullo_epi16(a, b);
  }
};

template <>
struct ArIntOps<4> {
  static QUIVER_FORCE_INLINE __m512i add(__m512i a, __m512i b) noexcept {
    return _mm512_add_epi32(a, b);
  }
  static QUIVER_FORCE_INLINE __m512i sub(__m512i a, __m512i b) noexcept {
    return _mm512_sub_epi32(a, b);
  }
  static QUIVER_FORCE_INLINE __m512i mul(__m512i a, __m512i b) noexcept {
    return _mm512_mullo_epi32(a, b);
  }
};

template <>
struct ArIntOps<8> {
  static QUIVER_FORCE_INLINE __m512i add(__m512i a, __m512i b) noexcept {
    return _mm512_add_epi64(a, b);
  }
  static QUIVER_FORCE_INLINE __m512i sub(__m512i a, __m512i b) noexcept {
    return _mm512_sub_epi64(a, b);
  }
  static QUIVER_FORCE_INLINE __m512i mul(__m512i a, __m512i b) noexcept {
    return _mm512_mullo_epi64(a, b);  // native vpmullq (AVX-512DQ)
  }
};

// One element type's vector view: the register type, its width, unaligned load/store, and the
// three ops. Naming these once lets the kernel below be a single loop for every type.
template <class T>
struct ArOps {
  using V = __m512i;
  static constexpr std::int64_t kW = static_cast<std::int64_t>(64 / sizeof(T));
  using I = ArIntOps<static_cast<int>(sizeof(T))>;
  static QUIVER_FORCE_INLINE V load(const T* p) noexcept {
    return _mm512_loadu_si512(reinterpret_cast<const void*>(p));
  }
  static QUIVER_FORCE_INLINE void store(T* p, V v) noexcept {
    _mm512_storeu_si512(reinterpret_cast<void*>(p), v);
  }
  static QUIVER_FORCE_INLINE V add(V a, V b) noexcept { return I::add(a, b); }
  static QUIVER_FORCE_INLINE V sub(V a, V b) noexcept { return I::sub(a, b); }
  static QUIVER_FORCE_INLINE V mul(V a, V b) noexcept { return I::mul(a, b); }
};

template <>
struct ArOps<float> {
  using V = __m512;
  static constexpr std::int64_t kW = 16;
  static QUIVER_FORCE_INLINE V load(const float* p) noexcept { return _mm512_loadu_ps(p); }
  static QUIVER_FORCE_INLINE void store(float* p, V v) noexcept { _mm512_storeu_ps(p, v); }
  static QUIVER_FORCE_INLINE V add(V a, V b) noexcept { return _mm512_add_ps(a, b); }
  static QUIVER_FORCE_INLINE V sub(V a, V b) noexcept { return _mm512_sub_ps(a, b); }
  static QUIVER_FORCE_INLINE V mul(V a, V b) noexcept { return _mm512_mul_ps(a, b); }
};

template <>
struct ArOps<double> {
  using V = __m512d;
  static constexpr std::int64_t kW = 8;
  static QUIVER_FORCE_INLINE V load(const double* p) noexcept { return _mm512_loadu_pd(p); }
  static QUIVER_FORCE_INLINE void store(double* p, V v) noexcept { _mm512_storeu_pd(p, v); }
  static QUIVER_FORCE_INLINE V add(V a, V b) noexcept { return _mm512_add_pd(a, b); }
  static QUIVER_FORCE_INLINE V sub(V a, V b) noexcept { return _mm512_sub_pd(a, b); }
  static QUIVER_FORCE_INLINE V mul(V a, V b) noexcept { return _mm512_mul_pd(a, b); }
};

template <class T>
QUIVER_FORCE_INLINE typename ArOps<T>::V apply_op(ArithOp op, typename ArOps<T>::V a,
                                                  typename ArOps<T>::V b) noexcept {
  using O = ArOps<T>;
  switch (op) {
  case ArithOp::kAdd:
    return O::add(a, b);
  case ArithOp::kSub:
    return O::sub(a, b);
  case ArithOp::kMul:
    return O::mul(a, b);
  }
  return a;  // unreachable for in-contract op values
}

// The left-hand batch an arith op reads.
template <class T>
struct ArBatch {
  const T* a;
  std::int64_t n;
};

template <class T, class LoadB>
void arith_impl(ArithOp op, ArBatch<T> in, LoadB load_b, T* out) noexcept {
  using O = ArOps<T>;
  std::int64_t i = 0;
  for (; i + O::kW <= in.n; i += O::kW) {
    O::store(out + i, apply_op<T>(op, O::load(in.a + i), load_b(i)));
  }
  for (; i < in.n; ++i) {  // scalar tail, identical arithmetic (floats: same IEEE op)
    out[i] = scalar_impl::arith_one(op, in.a[i], load_b.tail(i));
  }
}

template <class T>
struct ar_BatchRhs {
  const T* b;
  typename ArOps<T>::V operator()(std::int64_t i) const noexcept { return ArOps<T>::load(b + i); }
  T tail(std::int64_t i) const noexcept { return b[i]; }
};

template <class T>
struct ar_ScalarRhs {
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
    arith_impl<T>(op, {a, n}, ar_BatchRhs<T>{b}, out);                                             \
  }                                                                                                \
  void k9_arith_scalar_rhs(ArithOp op, const T* a, T b, std::int64_t n, T* out) noexcept {         \
    arith_impl<T>(op, {a, n}, ar_ScalarRhs<T>{b}, out);                                            \
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
