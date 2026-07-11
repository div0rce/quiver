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

// --- Convenience layer (PRD 04 §3.6, ADR-027): spelling only — every form below forwards to
// --- the primitives above; nothing allocates and no kernel behavior changes.

// No-validity forms ("all rows valid" without writing BitmapView{nullptr}).
template <Element T>
std::int64_t compare_bitmap(CompareOp op, BatchView<T> in, T comparand,
                            std::uint8_t* out_bits) noexcept {
  return compare_bitmap(op, in, comparand, all_valid, out_bits);
}

template <Element T>
std::int64_t compare_selvec(CompareOp op, BatchView<T> in, T comparand,
                            std::uint32_t* out_idx) noexcept {
  return compare_selvec(op, in, comparand, all_valid, out_idx);
}

template <Element T>
std::int64_t compare_between_bitmap(BatchView<T> in, T lo, T hi, std::uint8_t* out_bits) noexcept {
  return compare_between_bitmap(in, lo, hi, all_valid, out_bits);
}

template <Element T>
std::int64_t compare_between_selvec(BatchView<T> in, T lo, T hi, std::uint32_t* out_idx) noexcept {
  return compare_between_selvec(in, lo, hi, all_valid, out_idx);
}

// Range-in / span-out forms: the input is any contiguous range of Element values, the output
// span's capacity is checked (assertion builds, REQ-MEM-008), and the WRITTEN subspan is
// returned — the result and its count travel together.
template <BatchRange R, class T = std::remove_cv_t<std::ranges::range_value_t<R>>>
std::span<std::uint32_t>
compare_selvec(CompareOp op, const R& in, std::type_identity_t<T> comparand,
               std::span<std::uint32_t> out_idx, BitmapView validity = all_valid) noexcept {
  QUIVER_ASSERT(out_idx.size() >= std::ranges::size(in),
                "compare_selvec: out_idx capacity must be >= n [REQ-MEM-008]");
  const std::int64_t count =
      compare_selvec(op, batch_view(in), comparand, validity, out_idx.data());
  return out_idx.first(static_cast<std::size_t>(count));
}

template <BatchRange R, class T = std::remove_cv_t<std::ranges::range_value_t<R>>>
std::span<std::uint8_t> compare_bitmap(CompareOp op, const R& in, std::type_identity_t<T> comparand,
                                       std::span<std::uint8_t> out_bits,
                                       BitmapView validity = all_valid) noexcept {
  const auto need = bitmap_bytes(static_cast<std::int64_t>(std::ranges::size(in)));
  QUIVER_ASSERT(out_bits.size() >= need,
                "compare_bitmap: out_bits capacity must be >= bitmap_bytes(n) [REQ-MEM-008]");
  (void)compare_bitmap(op, batch_view(in), comparand, validity, out_bits.data());
  return out_bits.first(need);
}

QUIVER_END_NAMESPACE
