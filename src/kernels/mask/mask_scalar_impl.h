// K4 mask_algebra — scalar reference implementation (the family specification, Charter T3).
// Semantics (PRD 04 §5 K4): bitmap boolean algebra over 64-bit words (memcpy access, never a
// misaligned dereference); outputs fully written with tail bits zeroed (ADR-016); vacuous
// truths for n == 0: all=true, any=false, none=true; all/any/none may exit early at word
// granularity (queries, not selection-dependent transforms — REQ-KERNEL-003 does not apply,
// PRD 08 §5 K4); exact aliasing out == a or out == b permitted (ADR-023).
// Module: MOD-K4-MASK | REQs: REQ-K4-001..003 | ADR-016, ADR-023
#pragma once

#include <bit>
#include <cstdint>

#include "quiver/core.h"
#include "src/kernels/common/kernel_common.h"

QUIVER_BEGIN_NAMESPACE
namespace detail::scalar_impl {

QUIVER_FORCE_INLINE std::uint64_t apply_mask_op(MaskOp op, std::uint64_t a,
                                                std::uint64_t b) noexcept {
  switch (op) {
  case MaskOp::kAnd:
    return a & b;
  case MaskOp::kOr:
    return a | b;
  case MaskOp::kAndNot:
    return a & ~b;
  case MaskOp::kXor:
    return a ^ b;
  }
  return 0;  // unreachable for in-contract op values
}

// The two operand bitmaps a combine reads.
struct MaskPair {
  const std::uint8_t* a;
  const std::uint8_t* b;
};

inline void mask_combine(MaskOp op, MaskPair in, std::int64_t n, std::uint8_t* out) noexcept {
  const std::int64_t bytes = bitmap_byte_count(n);
  const std::int64_t words = bytes >> 3;
  for (std::int64_t w = 0; w < words; ++w) {
    store_word(out + w * 8, apply_mask_op(op, load_word(in.a + w * 8), load_word(in.b + w * 8)));
  }
  for (std::int64_t i = words * 8; i < bytes; ++i) {
    out[i] = static_cast<std::uint8_t>(apply_mask_op(op, in.a[i], in.b[i]));
  }
  zero_tail_bits(out, n);  // kAndNot/kXor can set tail bits; producers zero them (ADR-016)
}

inline void mask_not(const std::uint8_t* a, std::int64_t n, std::uint8_t* out) noexcept {
  const std::int64_t bytes = bitmap_byte_count(n);
  const std::int64_t words = bytes >> 3;
  for (std::int64_t w = 0; w < words; ++w) {
    store_word(out + w * 8, ~load_word(a + w * 8));
  }
  for (std::int64_t i = words * 8; i < bytes; ++i) {
    out[i] = static_cast<std::uint8_t>(~a[i]);
  }
  zero_tail_bits(out, n);
}

inline std::int64_t mask_popcount(const std::uint8_t* a, std::int64_t n) noexcept {
  std::int64_t count = 0;
  const std::int64_t full_bytes = n >> 3;
  const std::int64_t words = full_bytes >> 3;
  for (std::int64_t w = 0; w < words; ++w) {
    count += std::popcount(load_word(a + w * 8));
  }
  for (std::int64_t i = words * 8; i < full_bytes; ++i) {
    count += std::popcount(static_cast<unsigned>(a[i]));
  }
  const int tail = static_cast<int>(n & 7);
  if (tail != 0) {
    count += std::popcount(static_cast<unsigned>(a[full_bytes] & tail_mask(n)));
  }
  return count;
}

// all() and any() are one scan with an inverted sense: all() stops at the first byte that is
// not full, any() at the first that is not empty. Early exit is permitted — these are queries,
// not transforms (PRD 08 Sec 5 K4).
template <bool All>
inline bool mask_query(const std::uint8_t* a, std::int64_t n) noexcept {
  const std::int64_t full_bytes = n >> 3;
  for (std::int64_t i = 0; i < full_bytes; ++i) {
    if (a[i] != (All ? 0xFF : 0)) {
      return !All;
    }
  }
  if ((n & 7) == 0) {
    return All;  // no tail: the full-byte scan decided it
  }
  const std::uint8_t masked = static_cast<std::uint8_t>(a[full_bytes] & tail_mask(n));
  return All ? masked == tail_mask(n) : masked != 0;
}

inline bool mask_all(const std::uint8_t* a, std::int64_t n) noexcept {
  return mask_query<true>(a, n);
}

inline bool mask_any(const std::uint8_t* a, std::int64_t n) noexcept {
  return mask_query<false>(a, n);
}

inline bool mask_none(const std::uint8_t* a, std::int64_t n) noexcept {
  return !mask_any(a, n);
}

}  // namespace detail::scalar_impl
QUIVER_END_NAMESPACE
