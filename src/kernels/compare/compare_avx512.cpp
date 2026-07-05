// K1 compare — AVX-512 backend. The bitmap forms use native opmask compares
// (`_mm512_cmp_ep{i,u}{8,16,32,64}_mask` / `_mm512_cmp_p{s,d}_mask`): one instruction yields
// the N predicate bits directly — no movemask/PEXT packing the AVX2 path needs — which is the
// k-mask infrastructure the rest of the AVX-512 families reuse. Validity ANDs the loaded
// bitmap bits into the mask; the tail is scalar-byte-assembled (ADR-015).
//
// The SELVEC forms delegate to the scalar reference: turning a mask into indices is compaction
// (`vpcompressd` on an iota) — K2/K3's domain — so it lands with the compress work rather than
// being reinvented here (still bit-identical by construction, REQ-KERNEL-002).
// ponytail: bitmap = native k-masks (simpler than AVX2); selvec = delegate until K2/K3 compress.
// Base required set F+BW+DQ+VL only (REQ-SIMD-004): all compares here are F/BW/DQ, correct on
// SDE -skx. Uses only the shift-free op→immediate mapping.
// Module: MOD-K1-COMPARE | REQs: REQ-K1-001..003, REQ-SIMD-001..004 | ADR-003, ADR-016
#include "src/kernels/common/target_regions.h"
#include "src/kernels/compare/compare_scalar_impl.h"

#if defined(__x86_64__) || defined(_M_X64)

#include <bit>
#include <cstring>
#include <immintrin.h>

QUIVER_BEGIN_NAMESPACE
QUIVER_TARGET_AVX512_BEGIN
namespace detail::avx512 {

namespace {

template <class T>
constexpr int kLanes = 64 / static_cast<int>(sizeof(T));  // lanes per 512-bit group

// The opmask compare intrinsics take a COMPILE-TIME immediate, so each op is a literal case.
// Integer immediates (signed and unsigned share the mnemonic set): kGt=NLE, kGe=NLT.
#define QUIVER_CMP_INT_SWITCH(INTR, v, c)                                                          \
  switch (op) {                                                                                    \
  case CompareOp::kEq:                                                                             \
    return INTR((v), (c), _MM_CMPINT_EQ);                                                          \
  case CompareOp::kNe:                                                                             \
    return INTR((v), (c), _MM_CMPINT_NE);                                                          \
  case CompareOp::kLt:                                                                             \
    return INTR((v), (c), _MM_CMPINT_LT);                                                          \
  case CompareOp::kLe:                                                                             \
    return INTR((v), (c), _MM_CMPINT_LE);                                                          \
  case CompareOp::kGt:                                                                             \
    return INTR((v), (c), _MM_CMPINT_NLE);                                                         \
  case CompareOp::kGe:                                                                             \
    return INTR((v), (c), _MM_CMPINT_NLT);                                                         \
  }                                                                                                \
  return 0
// Float: ordered-quiet compares (NaN → false), except kNe = unordered (NaN → true) — matches
// the scalar reference's `!(a==b)`.
#define QUIVER_CMP_FLT_SWITCH(INTR, v, c)                                                          \
  switch (op) {                                                                                    \
  case CompareOp::kEq:                                                                             \
    return INTR((v), (c), _CMP_EQ_OQ);                                                             \
  case CompareOp::kNe:                                                                             \
    return INTR((v), (c), _CMP_NEQ_UQ);                                                            \
  case CompareOp::kLt:                                                                             \
    return INTR((v), (c), _CMP_LT_OQ);                                                             \
  case CompareOp::kLe:                                                                             \
    return INTR((v), (c), _CMP_LE_OQ);                                                             \
  case CompareOp::kGt:                                                                             \
    return INTR((v), (c), _CMP_GT_OQ);                                                             \
  case CompareOp::kGe:                                                                             \
    return INTR((v), (c), _CMP_GE_OQ);                                                             \
  }                                                                                                \
  return 0

// Compare a full group of lanes at `p` against a cmp_broadcast scalar; returns the N predicate
// bits.
template <class T>
QUIVER_FORCE_INLINE std::uint64_t cmp_scalar_group(CompareOp op, const T* p, T comparand) noexcept {
  if constexpr (std::is_same_v<T, float>) {
    const __m512 v = _mm512_loadu_ps(p);
    const __m512 c = _mm512_set1_ps(comparand);
    QUIVER_CMP_FLT_SWITCH(_mm512_cmp_ps_mask, v, c);
  } else if constexpr (std::is_same_v<T, double>) {
    const __m512d v = _mm512_loadu_pd(p);
    const __m512d c = _mm512_set1_pd(comparand);
    QUIVER_CMP_FLT_SWITCH(_mm512_cmp_pd_mask, v, c);
  } else {
    const __m512i v = _mm512_loadu_si512(reinterpret_cast<const void*>(p));
    if constexpr (std::is_same_v<T, std::int8_t>) {
      const __m512i c = _mm512_set1_epi8(comparand);
      QUIVER_CMP_INT_SWITCH(_mm512_cmp_epi8_mask, v, c);
    } else if constexpr (std::is_same_v<T, std::uint8_t>) {
      const __m512i c = _mm512_set1_epi8(static_cast<char>(comparand));
      QUIVER_CMP_INT_SWITCH(_mm512_cmp_epu8_mask, v, c);
    } else if constexpr (std::is_same_v<T, std::int16_t>) {
      const __m512i c = _mm512_set1_epi16(comparand);
      QUIVER_CMP_INT_SWITCH(_mm512_cmp_epi16_mask, v, c);
    } else if constexpr (std::is_same_v<T, std::uint16_t>) {
      const __m512i c = _mm512_set1_epi16(static_cast<short>(comparand));
      QUIVER_CMP_INT_SWITCH(_mm512_cmp_epu16_mask, v, c);
    } else if constexpr (std::is_same_v<T, std::int32_t>) {
      const __m512i c = _mm512_set1_epi32(comparand);
      QUIVER_CMP_INT_SWITCH(_mm512_cmp_epi32_mask, v, c);
    } else if constexpr (std::is_same_v<T, std::uint32_t>) {
      const __m512i c = _mm512_set1_epi32(static_cast<int>(comparand));
      QUIVER_CMP_INT_SWITCH(_mm512_cmp_epu32_mask, v, c);
    } else if constexpr (std::is_same_v<T, std::int64_t>) {
      const __m512i c = _mm512_set1_epi64(comparand);
      QUIVER_CMP_INT_SWITCH(_mm512_cmp_epi64_mask, v, c);
    } else {  // std::uint64_t
      const __m512i c = _mm512_set1_epi64(static_cast<long long>(comparand));
      QUIVER_CMP_INT_SWITCH(_mm512_cmp_epu64_mask, v, c);
    }
  }
}

// Two-batch compare a[i..] vs b[i..]; returns N predicate bits.
template <class T>
QUIVER_FORCE_INLINE std::uint64_t cmp_batch_group(CompareOp op, const T* a, const T* b) noexcept {
  if constexpr (std::is_same_v<T, float>) {
    QUIVER_CMP_FLT_SWITCH(_mm512_cmp_ps_mask, _mm512_loadu_ps(a), _mm512_loadu_ps(b));
  } else if constexpr (std::is_same_v<T, double>) {
    QUIVER_CMP_FLT_SWITCH(_mm512_cmp_pd_mask, _mm512_loadu_pd(a), _mm512_loadu_pd(b));
  } else {
    const __m512i va = _mm512_loadu_si512(reinterpret_cast<const void*>(a));
    const __m512i vb = _mm512_loadu_si512(reinterpret_cast<const void*>(b));
    if constexpr (std::is_same_v<T, std::int8_t>) {
      QUIVER_CMP_INT_SWITCH(_mm512_cmp_epi8_mask, va, vb);
    } else if constexpr (std::is_same_v<T, std::uint8_t>) {
      QUIVER_CMP_INT_SWITCH(_mm512_cmp_epu8_mask, va, vb);
    } else if constexpr (std::is_same_v<T, std::int16_t>) {
      QUIVER_CMP_INT_SWITCH(_mm512_cmp_epi16_mask, va, vb);
    } else if constexpr (std::is_same_v<T, std::uint16_t>) {
      QUIVER_CMP_INT_SWITCH(_mm512_cmp_epu16_mask, va, vb);
    } else if constexpr (std::is_same_v<T, std::int32_t>) {
      QUIVER_CMP_INT_SWITCH(_mm512_cmp_epi32_mask, va, vb);
    } else if constexpr (std::is_same_v<T, std::uint32_t>) {
      QUIVER_CMP_INT_SWITCH(_mm512_cmp_epu32_mask, va, vb);
    } else if constexpr (std::is_same_v<T, std::int64_t>) {
      QUIVER_CMP_INT_SWITCH(_mm512_cmp_epi64_mask, va, vb);
    } else {
      QUIVER_CMP_INT_SWITCH(_mm512_cmp_epu64_mask, va, vb);
    }
  }
}

#undef QUIVER_CMP_INT_SWITCH
#undef QUIVER_CMP_FLT_SWITCH

// N validity bits for the group starting at element i (all-ones when validity is absent).
template <class T>
QUIVER_FORCE_INLINE std::uint64_t vbits_group(const std::uint8_t* validity,
                                              std::int64_t i) noexcept {
  if (validity == nullptr) {
    return ~std::uint64_t{0};
  }
  std::uint64_t v = 0;
  std::memcpy(&v, validity + (i >> 3), static_cast<std::size_t>(kLanes<T> / 8));
  return v;
}

// Bitmap emit: vector groups (k-mask AND validity), scalar-byte tail (ADR-015/-016).
template <class T, class GroupMask, class TailPred>
QUIVER_FORCE_INLINE std::int64_t emit_bm(std::int64_t n, std::uint8_t* out, GroupMask group_mask,
                                         TailPred tail_pred) noexcept {
  constexpr int kN = kLanes<T>;
  constexpr std::size_t kNB = static_cast<std::size_t>(kN / 8);
  std::int64_t count = 0;
  std::int64_t i = 0;
  for (; i + kN <= n; i += kN) {
    const std::uint64_t m = group_mask(i);
    std::memcpy(out + (i >> 3), &m, kNB);
    count += std::popcount(m);
  }
  if (i < n) {  // scalar tail, byte-assembled exactly like the reference
    std::int64_t byte_idx = i >> 3;
    std::uint8_t byte = 0;
    int k = 0;
    for (; i < n; ++i) {
      byte = static_cast<std::uint8_t>(byte | (static_cast<std::uint8_t>(tail_pred(i)) << k));
      if (++k == 8) {
        out[byte_idx++] = byte;
        count += std::popcount(byte);
        byte = 0;
        k = 0;
      }
    }
    if (k != 0) {
      out[byte_idx] = byte;  // bits >= tail are zero by construction (ADR-016)
      count += std::popcount(byte);
    }
  }
  return count;
}

}  // namespace

// NOLINTBEGIN(bugprone-macro-parentheses): T expands to type names inside declarators.
#define QUIVER_K1_DEFINE(T)                                                                        \
  std::int64_t k1_compare_bitmap(CompareOp op, const T* in, std::int64_t n, T comparand,           \
                                 const std::uint8_t* validity, std::uint8_t* out) noexcept {       \
    return emit_bm<T>(                                                                             \
        n, out,                                                                                    \
        [&](std::int64_t i) {                                                                      \
          return cmp_scalar_group<T>(op, in + i, comparand) & vbits_group<T>(validity, i);         \
        },                                                                                         \
        [&](std::int64_t i) {                                                                      \
          return is_valid(validity, i) && scalar_impl::compare_one(op, in[i], comparand);          \
        });                                                                                        \
  }                                                                                                \
  std::int64_t k1_compare_bitmap2(CompareOp op, const T* a, const T* b, std::int64_t n,            \
                                  const std::uint8_t* a_validity, const std::uint8_t* b_validity,  \
                                  std::uint8_t* out) noexcept {                                    \
    return emit_bm<T>(                                                                             \
        n, out,                                                                                    \
        [&](std::int64_t i) {                                                                      \
          return cmp_batch_group<T>(op, a + i, b + i) & vbits_group<T>(a_validity, i) &            \
                 vbits_group<T>(b_validity, i);                                                    \
        },                                                                                         \
        [&](std::int64_t i) {                                                                      \
          return is_valid(a_validity, i) && is_valid(b_validity, i) &&                             \
                 scalar_impl::compare_one(op, a[i], b[i]);                                         \
        });                                                                                        \
  }                                                                                                \
  std::int64_t k1_compare_between_bitmap(const T* in, std::int64_t n, T lo, T hi,                  \
                                         const std::uint8_t* validity,                             \
                                         std::uint8_t* out) noexcept {                             \
    return emit_bm<T>(                                                                             \
        n, out,                                                                                    \
        [&](std::int64_t i) {                                                                      \
          return cmp_scalar_group<T>(CompareOp::kGe, in + i, lo) &                                 \
                 cmp_scalar_group<T>(CompareOp::kLe, in + i, hi) & vbits_group<T>(validity, i);    \
        },                                                                                         \
        [&](std::int64_t i) { return is_valid(validity, i) && (lo <= in[i]) && (in[i] <= hi); });  \
  }                                                                                                \
  std::int64_t k1_compare_selvec(CompareOp op, const T* in, std::int64_t n, T comparand,           \
                                 const std::uint8_t* validity, std::uint32_t* out) noexcept {      \
    return scalar_impl::compare_selvec<T>(op, in, n, comparand, validity, out);                    \
  }                                                                                                \
  std::int64_t k1_compare_selvec2(CompareOp op, const T* a, const T* b, std::int64_t n,            \
                                  const std::uint8_t* a_validity, const std::uint8_t* b_validity,  \
                                  std::uint32_t* out) noexcept {                                   \
    return scalar_impl::compare_selvec2<T>(op, a, b, n, a_validity, b_validity, out);              \
  }                                                                                                \
  std::int64_t k1_compare_between_selvec(const T* in, std::int64_t n, T lo, T hi,                  \
                                         const std::uint8_t* validity,                             \
                                         std::uint32_t* out) noexcept {                            \
    return scalar_impl::compare_between_selvec<T>(in, n, lo, hi, validity, out);                   \
  }

QUIVER_K1_DEFINE(std::int8_t)
QUIVER_K1_DEFINE(std::int16_t)
QUIVER_K1_DEFINE(std::int32_t)
QUIVER_K1_DEFINE(std::int64_t)
QUIVER_K1_DEFINE(std::uint8_t)
QUIVER_K1_DEFINE(std::uint16_t)
QUIVER_K1_DEFINE(std::uint32_t)
QUIVER_K1_DEFINE(std::uint64_t)
QUIVER_K1_DEFINE(float)
QUIVER_K1_DEFINE(double)
#undef QUIVER_K1_DEFINE
// NOLINTEND(bugprone-macro-parentheses)

}  // namespace detail::avx512
QUIVER_TARGET_AVX512_END
QUIVER_END_NAMESPACE

#endif  // x86-64
