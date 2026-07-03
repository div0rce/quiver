// Surface A — K1 compare: branch-free predicate evaluation into either selection
// representation (API-K1-001..006; semantics in docs/api/compare.md and PRD 04/08).
// Module: MOD-K1-COMPARE | REQs: REQ-K1-001..003, REQ-API-* | ADR-006, ADR-016, ADR-025
#pragma once

#include "quiver/core.h"
#include "quiver/detail/config.h"
#include "quiver/detail/extern_decls.h"

QUIVER_BEGIN_NAMESPACE

// out_bits: capacity ceil(n/8) bytes, fully written, tail bits zero; returns popcount.
template <Element T>
std::int64_t compare_bitmap(CompareOp op, BatchView<T> in, T comparand, BitmapView validity,
                            std::uint8_t* out_bits) noexcept {
  QUIVER_ASSERT(in.len >= 0 && in.len <= kMaxBatchLen,
                "compare_bitmap: 0 <= n <= kMaxBatchLen [REQ-API-005]");
  return detail::k1_compare_bitmap(op, in.data, in.len, comparand, validity.bits, out_bits);
}

// Two-batch form: element i is valid iff a_valid(i) AND b_valid(i) (PRD 04 K1).
template <Element T>
std::int64_t compare_bitmap(CompareOp op, BatchView<T> a, BatchView<T> b, BitmapView a_validity,
                            BitmapView b_validity, std::uint8_t* out_bits) noexcept {
  QUIVER_ASSERT(a.len == b.len, "compare_bitmap: a.len == b.len [API-K1-002]");
  QUIVER_ASSERT(a.len >= 0 && a.len <= kMaxBatchLen,
                "compare_bitmap: 0 <= n <= kMaxBatchLen [REQ-API-005]");
  return detail::k1_compare_bitmap2(op, a.data, b.data, a.len, a_validity.bits, b_validity.bits,
                                    out_bits);
}

// Inclusive range: selects lo <= x && x <= hi (API-K1-003).
template <Element T>
std::int64_t compare_between_bitmap(BatchView<T> in, T lo, T hi, BitmapView validity,
                                    std::uint8_t* out_bits) noexcept {
  QUIVER_ASSERT(in.len >= 0 && in.len <= kMaxBatchLen,
                "compare_between_bitmap: 0 <= n <= kMaxBatchLen [REQ-API-005]");
  return detail::k1_compare_between_bitmap(in.data, in.len, lo, hi, validity.bits, out_bits);
}

// Selvec forms: defined output = first count strictly increasing indices; capacity region n
// (REQ-MEM-008); returns count.
template <Element T>
std::int64_t compare_selvec(CompareOp op, BatchView<T> in, T comparand, BitmapView validity,
                            std::uint32_t* out_idx) noexcept {
  QUIVER_ASSERT(in.len >= 0 && in.len <= kMaxBatchLen,
                "compare_selvec: 0 <= n <= kMaxBatchLen [REQ-API-005]");
  return detail::k1_compare_selvec(op, in.data, in.len, comparand, validity.bits, out_idx);
}

template <Element T>
std::int64_t compare_selvec(CompareOp op, BatchView<T> a, BatchView<T> b, BitmapView a_validity,
                            BitmapView b_validity, std::uint32_t* out_idx) noexcept {
  QUIVER_ASSERT(a.len == b.len, "compare_selvec: a.len == b.len [API-K1-005]");
  QUIVER_ASSERT(a.len >= 0 && a.len <= kMaxBatchLen,
                "compare_selvec: 0 <= n <= kMaxBatchLen [REQ-API-005]");
  return detail::k1_compare_selvec2(op, a.data, b.data, a.len, a_validity.bits, b_validity.bits,
                                    out_idx);
}

template <Element T>
std::int64_t compare_between_selvec(BatchView<T> in, T lo, T hi, BitmapView validity,
                                    std::uint32_t* out_idx) noexcept {
  QUIVER_ASSERT(in.len >= 0 && in.len <= kMaxBatchLen,
                "compare_between_selvec: 0 <= n <= kMaxBatchLen [REQ-API-005]");
  return detail::k1_compare_between_selvec(in.data, in.len, lo, hi, validity.bits, out_idx);
}

QUIVER_END_NAMESPACE
