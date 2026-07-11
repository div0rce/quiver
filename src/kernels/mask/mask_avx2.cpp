// K4 mask_algebra — AVX2 backend: 256-bit bitwise ops over the byte stream; popcount and
// queries reuse the scalar word cores (bit-identical by construction; the honest-verdict
// hypothesis for this family is that autovec already ties explicit SIMD — Survey §4.4).
// Bit-identical to mask_scalar_impl.h (REQ-KERNEL-002); tails per ADR-015/ADR-016.
// Module: MOD-K4-MASK | REQs: REQ-K4-001..003, REQ-SIMD-001..003 | ADR-003
#include "src/kernels/common/target_regions.h"
#include "src/kernels/mask/mask_scalar_impl.h"

#if defined(__x86_64__) || defined(_M_X64)

#include <immintrin.h>

QUIVER_BEGIN_NAMESPACE
QUIVER_TARGET_AVX2_BEGIN
namespace detail::avx2 {

namespace {

QUIVER_FORCE_INLINE __m256i apply_op256(MaskOp op, __m256i a, __m256i b) noexcept {
  switch (op) {
  case MaskOp::kAnd:
    return _mm256_and_si256(a, b);
  case MaskOp::kOr:
    return _mm256_or_si256(a, b);
  case MaskOp::kAndNot:
    return _mm256_andnot_si256(b, a);  // note operand order: (~b) & a
  case MaskOp::kXor:
    return _mm256_xor_si256(a, b);
  }
  return _mm256_setzero_si256();
}

}  // namespace

void k4_mask_combine(MaskOp op, const std::uint8_t* a, const std::uint8_t* b, std::int64_t n,
                     std::uint8_t* out) noexcept {
  const std::int64_t bytes = bitmap_byte_count(n);
  std::int64_t i = 0;
  for (; i + 32 <= bytes; i += 32) {
    const __m256i va = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(a + i));
    const __m256i vb = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(b + i));
    _mm256_storeu_si256(reinterpret_cast<__m256i*>(out + i), apply_op256(op, va, vb));
  }
  for (; i < bytes; ++i) {  // scalar byte tail (ADR-015: no over-read)
    out[i] = static_cast<std::uint8_t>(scalar_impl::apply_mask_op(op, a[i], b[i]));
  }
  zero_tail_bits(out, n);
}

void k4_mask_not(const std::uint8_t* a, std::int64_t n, std::uint8_t* out) noexcept {
  const std::int64_t bytes = bitmap_byte_count(n);
  const __m256i ones = _mm256_set1_epi8(static_cast<char>(0xFF));
  std::int64_t i = 0;
  for (; i + 32 <= bytes; i += 32) {
    const __m256i va = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(a + i));
    _mm256_storeu_si256(reinterpret_cast<__m256i*>(out + i), _mm256_xor_si256(va, ones));
  }
  for (; i < bytes; ++i) {
    out[i] = static_cast<std::uint8_t>(~a[i]);
  }
  zero_tail_bits(out, n);
}

// Word-loop cores are already the specified technique for counting/queries (PRD 08 K4);
// the backend delegates so results are identical by construction.
std::int64_t k4_mask_popcount(const std::uint8_t* a, std::int64_t n) noexcept {
  return scalar_impl::mask_popcount(a, n);
}
bool k4_mask_all(const std::uint8_t* a, std::int64_t n) noexcept {
  return scalar_impl::mask_all(a, n);
}
bool k4_mask_any(const std::uint8_t* a, std::int64_t n) noexcept {
  return scalar_impl::mask_any(a, n);
}
bool k4_mask_none(const std::uint8_t* a, std::int64_t n) noexcept {
  return scalar_impl::mask_none(a, n);
}

}  // namespace detail::avx2
QUIVER_TARGET_AVX2_END
QUIVER_END_NAMESPACE

#endif  // x86-64
