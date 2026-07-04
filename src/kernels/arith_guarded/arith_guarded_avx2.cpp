// K10 arith_guarded — AVX2 backend (PRD 08 §5 K10; ADR-014). Checked add/sub vectorize the
// sign trick (((a^r) & (b^r)) < 0 signed; carry compare unsigned) with overflow lanes
// packed to the position bitmap exactly like the K1 emit core. Saturating add/sub are
// native for 8/16-bit (paddsb/paddusb...) and checked+directed-clamp for 32/64-bit. ALL
// multiplies (checked and saturating) delegate to the scalar core: 64-bit is the documented
// REQ-K10-003 concession, and vectorizing the narrow widening-multiply forms is
// ledger-gated follow-up recorded on the family page. Bit-identical to
// arith_guarded_scalar_impl.h everywhere (REQ-KERNEL-002).
// Module: MOD-K10-ARITH-GUARDED | REQs: REQ-K10-001..003, REQ-SIMD-001..003 | ADR-014
#include "src/kernels/arith_guarded/arith_guarded_scalar_impl.h"
#include "src/kernels/common/target_regions.h"

#if defined(__x86_64__) || defined(_M_X64)

#include <cstring>
#include <immintrin.h>
#include <type_traits>

QUIVER_BEGIN_NAMESPACE
QUIVER_TARGET_AVX2_BEGIN
namespace detail::avx2 {

namespace {

// Lane-mask -> LSB-first bits for one 256-bit block at T's width (as in the K1 backend).
template <class T>
QUIVER_FORCE_INLINE std::uint32_t movemask_bits(__m256i lane_mask) noexcept {
  if constexpr (sizeof(T) == 1) {
    return static_cast<std::uint32_t>(_mm256_movemask_epi8(lane_mask));
  } else if constexpr (sizeof(T) == 2) {
    return _pext_u32(static_cast<std::uint32_t>(_mm256_movemask_epi8(lane_mask)), 0xAAAAAAAAu);
  } else if constexpr (sizeof(T) == 4) {
    return static_cast<std::uint32_t>(_mm256_movemask_ps(_mm256_castsi256_ps(lane_mask)));
  } else {
    return static_cast<std::uint32_t>(_mm256_movemask_pd(_mm256_castsi256_pd(lane_mask)));
  }
}

template <class T>
QUIVER_FORCE_INLINE __m256i add_wrap(__m256i a, __m256i b) noexcept {
  if constexpr (sizeof(T) == 1) {
    return _mm256_add_epi8(a, b);
  } else if constexpr (sizeof(T) == 2) {
    return _mm256_add_epi16(a, b);
  } else if constexpr (sizeof(T) == 4) {
    return _mm256_add_epi32(a, b);
  } else {
    return _mm256_add_epi64(a, b);
  }
}

template <class T>
QUIVER_FORCE_INLINE __m256i sub_wrap(__m256i a, __m256i b) noexcept {
  if constexpr (sizeof(T) == 1) {
    return _mm256_sub_epi8(a, b);
  } else if constexpr (sizeof(T) == 2) {
    return _mm256_sub_epi16(a, b);
  } else if constexpr (sizeof(T) == 4) {
    return _mm256_sub_epi32(a, b);
  } else {
    return _mm256_sub_epi64(a, b);
  }
}

// Sign bit of each lane as an all-ones/all-zero lane mask (arithmetic shift emulation for
// widths without vpsra*: 8-bit via cmpgt against zero, 64-bit via cmpgt).
template <class T>
QUIVER_FORCE_INLINE __m256i sign_mask(__m256i v) noexcept {
  const __m256i zero = _mm256_setzero_si256();
  if constexpr (sizeof(T) == 1) {
    return _mm256_cmpgt_epi8(zero, v);
  } else if constexpr (sizeof(T) == 2) {
    return _mm256_cmpgt_epi16(zero, v);
  } else if constexpr (sizeof(T) == 4) {
    return _mm256_cmpgt_epi32(zero, v);
  } else {
    return _mm256_cmpgt_epi64(zero, v);
  }
}

// Unsigned lane compare a < b via sign-bias (as in the K1 backend).
template <class T>
QUIVER_FORCE_INLINE __m256i ult_mask(__m256i a, __m256i b) noexcept {
  __m256i bias;
  if constexpr (sizeof(T) == 1) {
    bias = _mm256_set1_epi8(static_cast<char>(0x80));
  } else if constexpr (sizeof(T) == 2) {
    bias = _mm256_set1_epi16(static_cast<short>(0x8000));
  } else if constexpr (sizeof(T) == 4) {
    bias = _mm256_set1_epi32(static_cast<int>(0x80000000u));
  } else {
    bias = _mm256_set1_epi64x(static_cast<long long>(0x8000000000000000ull));
  }
  const __m256i xa = _mm256_xor_si256(a, bias);
  const __m256i xb = _mm256_xor_si256(b, bias);
  if constexpr (sizeof(T) == 1) {
    return _mm256_cmpgt_epi8(xb, xa);
  } else if constexpr (sizeof(T) == 2) {
    return _mm256_cmpgt_epi16(xb, xa);
  } else if constexpr (sizeof(T) == 4) {
    return _mm256_cmpgt_epi32(xb, xa);
  } else {
    return _mm256_cmpgt_epi64(xb, xa);
  }
}

// Checked add/sub for one block: {wrapped result, overflow lane mask}.
template <class T>
QUIVER_FORCE_INLINE __m256i checked_block(ArithOp op, __m256i a, __m256i b,
                                          __m256i* overflow) noexcept {
  if constexpr (std::is_signed_v<T>) {
    if (op == ArithOp::kAdd) {
      const __m256i r = add_wrap<T>(a, b);
      // ((a^r) & (b^r)) sign bit set => overflow
      *overflow = sign_mask<T>(_mm256_and_si256(_mm256_xor_si256(a, r), _mm256_xor_si256(b, r)));
      return r;
    }
    const __m256i r = sub_wrap<T>(a, b);
    *overflow = sign_mask<T>(_mm256_and_si256(_mm256_xor_si256(a, b), _mm256_xor_si256(a, r)));
    return r;
  } else {
    if (op == ArithOp::kAdd) {
      const __m256i r = add_wrap<T>(a, b);
      *overflow = ult_mask<T>(r, a);  // carry: r < a
      return r;
    }
    const __m256i r = sub_wrap<T>(a, b);
    *overflow = ult_mask<T>(a, b);  // borrow: a < b
    return r;
  }
}

// Vectorized checked add/sub over 8-element-aligned groups (bitmap byte granularity), with
// count and optional position bits; multiplies fall through to the scalar core.
template <class T, class LoadB>
std::int64_t checked_addsub_impl(ArithOp op, const T* a, LoadB load_b, std::int64_t n, T* out,
                                 std::uint8_t* overflow_bits) noexcept {
  constexpr std::int64_t kW = static_cast<std::int64_t>(32 / sizeof(T));
  constexpr std::int64_t kGroup = (kW < 8) ? 8 : kW;  // >= one bitmap byte per group
  std::int64_t count = 0;
  std::int64_t i = 0;
  for (; i + kGroup <= n; i += kGroup) {
    std::uint32_t bits = 0;
    for (std::int64_t v = 0; v < kGroup; v += kW) {
      const __m256i va = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(a + i + v));
      __m256i ov;
      const __m256i r = checked_block<T>(op, va, load_b(i + v), &ov);
      _mm256_storeu_si256(reinterpret_cast<__m256i*>(out + i + v), r);
      bits |= movemask_bits<T>(ov) << v;
    }
    count += std::popcount(bits);
    if (overflow_bits != nullptr) {
      std::memcpy(overflow_bits + (i >> 3), &bits, kGroup / 8);
    }
  }
  if (i < n) {  // scalar tail with byte assembly (ADR-015/ADR-016)
    std::int64_t byte_idx = i >> 3;
    std::uint8_t byte = 0;
    int k = 0;
    for (; i < n; ++i) {
      T r;
      const bool ov = scalar_impl::checked_one(op, a[i], load_b.tail(i), &r);
      out[i] = r;
      count += ov ? 1 : 0;
      byte = static_cast<std::uint8_t>(byte | (static_cast<std::uint8_t>(ov) << k));
      if (++k == 8) {
        if (overflow_bits != nullptr) {
          overflow_bits[byte_idx] = byte;
        }
        ++byte_idx;
        byte = 0;
        k = 0;
      }
    }
    if (k != 0 && overflow_bits != nullptr) {
      overflow_bits[byte_idx] = byte;  // tail bits zero by construction (ADR-016)
    }
  }
  return count;
}

// Saturating add/sub for one block: native instructions for 8/16-bit; checked result +
// directed clamp (blend on the overflow mask) for 32/64-bit (PRD 08 §5 K10).
template <class T>
QUIVER_FORCE_INLINE __m256i saturating_block(ArithOp op, __m256i a, __m256i b) noexcept {
  constexpr bool kSigned = std::is_signed_v<T>;
  if constexpr (sizeof(T) == 1) {
    if (op == ArithOp::kAdd) {
      return kSigned ? _mm256_adds_epi8(a, b) : _mm256_adds_epu8(a, b);
    }
    return kSigned ? _mm256_subs_epi8(a, b) : _mm256_subs_epu8(a, b);
  } else if constexpr (sizeof(T) == 2) {
    if (op == ArithOp::kAdd) {
      return kSigned ? _mm256_adds_epi16(a, b) : _mm256_adds_epu16(a, b);
    }
    return kSigned ? _mm256_subs_epi16(a, b) : _mm256_subs_epu16(a, b);
  } else {
    __m256i ov;
    const __m256i r = checked_block<T>(op, a, b, &ov);
    __m256i limit;
    if constexpr (kSigned) {
      const __m256i vmax = (sizeof(T) == 4) ? _mm256_set1_epi32(0x7FFFFFFF)
                                            : _mm256_set1_epi64x(0x7FFFFFFFFFFFFFFFll);
      const __m256i vmin = (sizeof(T) == 4)
                               ? _mm256_set1_epi32(static_cast<int>(0x80000000u))
                               : _mm256_set1_epi64x(static_cast<long long>(0x8000000000000000ull));
      // Overflow direction: add -> sign(a) picks MIN; sub -> (a < b) picks MIN.
      const __m256i toward_min =
          (op == ArithOp::kAdd)
              ? sign_mask<T>(a)
              : ((sizeof(T) == 4) ? _mm256_cmpgt_epi32(b, a) : _mm256_cmpgt_epi64(b, a));
      limit = _mm256_blendv_epi8(vmax, vmin, toward_min);
    } else {
      // Unsigned: add overflows toward MAX, sub toward 0.
      limit = (op == ArithOp::kAdd) ? _mm256_set1_epi8(static_cast<char>(0xFF))
                                    : _mm256_setzero_si256();
    }
    return _mm256_blendv_epi8(r, limit, ov);
  }
}

template <class T, class LoadB>
void saturating_addsub_impl(ArithOp op, const T* a, LoadB load_b, std::int64_t n, T* out) noexcept {
  constexpr std::int64_t kW = static_cast<std::int64_t>(32 / sizeof(T));
  std::int64_t i = 0;
  for (; i + kW <= n; i += kW) {
    const __m256i va = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(a + i));
    _mm256_storeu_si256(reinterpret_cast<__m256i*>(out + i),
                        saturating_block<T>(op, va, load_b(i)));
  }
  for (; i < n; ++i) {
    out[i] = scalar_impl::saturate_one(op, a[i], load_b.tail(i));
  }
}

template <class T>
struct BatchRhs {
  const T* b;
  __m256i operator()(std::int64_t i) const noexcept {
    return _mm256_loadu_si256(reinterpret_cast<const __m256i*>(b + i));
  }
  T tail(std::int64_t i) const noexcept { return b[i]; }
};

template <class T>
struct ScalarRhs {
  T b;
  __m256i operator()(std::int64_t) const noexcept {
    if constexpr (sizeof(T) == 1) {
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
#define QUIVER_K10_DEFINE(T)                                                                       \
  std::int64_t k10_arith_checked(ArithOp op, const T* a, const T* b, std::int64_t n, T* out,       \
                                 std::uint8_t* overflow_bits) noexcept {                           \
    if (op == ArithOp::kMul) {                                                                     \
      return scalar_impl::arith_checked<T>(op, a, b, n, out, overflow_bits);                       \
    }                                                                                              \
    return checked_addsub_impl<T>(op, a, BatchRhs<T>{b}, n, out, overflow_bits);                   \
  }                                                                                                \
  std::int64_t k10_arith_checked_scalar_rhs(ArithOp op, const T* a, T b, std::int64_t n, T* out,   \
                                            std::uint8_t* overflow_bits) noexcept {                \
    if (op == ArithOp::kMul) {                                                                     \
      return scalar_impl::arith_checked_scalar_rhs<T>(op, a, b, n, out, overflow_bits);            \
    }                                                                                              \
    return checked_addsub_impl<T>(op, a, ScalarRhs<T>{b}, n, out, overflow_bits);                  \
  }                                                                                                \
  void k10_arith_saturating(ArithOp op, const T* a, const T* b, std::int64_t n, T* out) noexcept { \
    if (op == ArithOp::kMul) {                                                                     \
      scalar_impl::arith_saturating<T>(op, a, b, n, out); /* REQ-K10-003 concession */             \
      return;                                                                                      \
    }                                                                                              \
    saturating_addsub_impl<T>(op, a, BatchRhs<T>{b}, n, out);                                      \
  }                                                                                                \
  void k10_arith_saturating_scalar_rhs(ArithOp op, const T* a, T b, std::int64_t n,                \
                                       T* out) noexcept {                                          \
    if (op == ArithOp::kMul) {                                                                     \
      scalar_impl::arith_saturating_scalar_rhs<T>(op, a, b, n, out);                               \
      return;                                                                                      \
    }                                                                                              \
    saturating_addsub_impl<T>(op, a, ScalarRhs<T>{b}, n, out);                                     \
  }

QUIVER_K10_DEFINE(std::int8_t)
QUIVER_K10_DEFINE(std::int16_t)
QUIVER_K10_DEFINE(std::int32_t)
QUIVER_K10_DEFINE(std::int64_t)
QUIVER_K10_DEFINE(std::uint8_t)
QUIVER_K10_DEFINE(std::uint16_t)
QUIVER_K10_DEFINE(std::uint32_t)
QUIVER_K10_DEFINE(std::uint64_t)
#undef QUIVER_K10_DEFINE
// NOLINTEND(bugprone-macro-parentheses)

}  // namespace detail::avx2
QUIVER_TARGET_AVX2_END
QUIVER_END_NAMESPACE

#endif  // x86-64
