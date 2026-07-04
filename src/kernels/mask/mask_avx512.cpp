// K4 mask_algebra — AVX-512 backend: 512-bit bitwise ops over the byte stream; popcount and
// queries reuse the scalar word cores (bit-identical by construction). Uses only the base
// required set F+BW+DQ+VL (REQ-SIMD-004) — the and/or/xor/andnot here are AVX-512F, so this
// backend is correct on the SDE -skx profile too. First AVX-512 backend (M7 validation slice:
// proves the public API → dispatch → slot [3] path under SDE / the detection seam).
// Bit-identical to mask_scalar_impl.h (REQ-KERNEL-002); tails per ADR-015/ADR-016.
// Module: MOD-K4-MASK | REQs: REQ-K4-001..003, REQ-SIMD-001..004 | ADR-003
#include "src/kernels/common/target_regions.h"
#include "src/kernels/mask/mask_scalar_impl.h"

#if defined(__x86_64__) || defined(_M_X64)

#include <immintrin.h>

QUIVER_BEGIN_NAMESPACE
QUIVER_TARGET_AVX512_BEGIN
namespace detail::avx512 {

namespace {

QUIVER_FORCE_INLINE __m512i apply_op512(MaskOp op, __m512i a, __m512i b) noexcept {
  switch (op) {
  case MaskOp::kAnd:
    return _mm512_and_si512(a, b);
  case MaskOp::kOr:
    return _mm512_or_si512(a, b);
  case MaskOp::kAndNot:
    return _mm512_andnot_si512(b, a);  // note operand order: (~b) & a
  case MaskOp::kXor:
    return _mm512_xor_si512(a, b);
  }
  return _mm512_setzero_si512();
}

}  // namespace

void k4_mask_combine(MaskOp op, const std::uint8_t* a, const std::uint8_t* b, std::int64_t n,
                     std::uint8_t* out) noexcept {
  const std::int64_t bytes = bitmap_bytes(n);
  std::int64_t i = 0;
  for (; i + 64 <= bytes; i += 64) {
    const __m512i va = _mm512_loadu_si512(reinterpret_cast<const void*>(a + i));
    const __m512i vb = _mm512_loadu_si512(reinterpret_cast<const void*>(b + i));
    _mm512_storeu_si512(reinterpret_cast<void*>(out + i), apply_op512(op, va, vb));
  }
  for (; i < bytes; ++i) {  // scalar byte tail (ADR-015: no over-read)
    out[i] = static_cast<std::uint8_t>(scalar_impl::apply_mask_op(op, a[i], b[i]));
  }
  zero_tail_bits(out, n);
}

void k4_mask_not(const std::uint8_t* a, std::int64_t n, std::uint8_t* out) noexcept {
  const std::int64_t bytes = bitmap_bytes(n);
  const __m512i ones = _mm512_set1_epi8(static_cast<char>(0xFF));
  std::int64_t i = 0;
  for (; i + 64 <= bytes; i += 64) {
    const __m512i va = _mm512_loadu_si512(reinterpret_cast<const void*>(a + i));
    _mm512_storeu_si512(reinterpret_cast<void*>(out + i), _mm512_xor_si512(va, ones));
  }
  for (; i < bytes; ++i) {
    out[i] = static_cast<std::uint8_t>(~a[i]);
  }
  zero_tail_bits(out, n);
}

// Word-loop cores are the specified technique for counting/queries (PRD 08 K4); the backend
// delegates so results are identical by construction (VPOPCNTDQ variant is a later option).
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

}  // namespace detail::avx512
QUIVER_TARGET_AVX512_END
QUIVER_END_NAMESPACE

#endif  // x86-64
