// Surface A — K6 reduce / SMA: single-pass reductions with optional validity and selection;
// participation = selected AND valid; identity results for empty participation
// (API-K6-001..005; float policy per ADR-013 — the scalar backend is the strict-order
// recourse; NaN-propagating min/max with canonical qNaN).
// Module: MOD-K6-REDUCE | REQs: REQ-K6-001..005 | ADR-006, ADR-013
#pragma once

#include "quiver/core.h"
#include "quiver/detail/config.h"
#include "quiver/detail/extern_decls.h"
#include "quiver/mask.h"

QUIVER_BEGIN_NAMESPACE

template <Element T>
T reduce_min(BatchView<T> in, BitmapView validity) noexcept {
  QUIVER_ASSERT(in.len >= 0 && in.len <= kMaxBatchLen,
                "reduce_min: 0 <= n <= kMaxBatchLen [REQ-API-005]");
  return detail::k6_reduce_min(in.data, in.len, validity.bits, nullptr, 0);
}

template <Element T>
T reduce_min(BatchView<T> in, BitmapView validity, SelVec sel) noexcept {
  QUIVER_ASSERT(sel.idx != nullptr || sel.len == 0,
                "reduce_min: sel.idx must be non-null when sel.len > 0 [REQ-API-008]");
  return detail::k6_reduce_min(in.data, in.len, validity.bits, sel.idx, sel.len);
}

template <Element T>
T reduce_max(BatchView<T> in, BitmapView validity) noexcept {
  QUIVER_ASSERT(in.len >= 0 && in.len <= kMaxBatchLen,
                "reduce_max: 0 <= n <= kMaxBatchLen [REQ-API-005]");
  return detail::k6_reduce_max(in.data, in.len, validity.bits, nullptr, 0);
}

template <Element T>
T reduce_max(BatchView<T> in, BitmapView validity, SelVec sel) noexcept {
  QUIVER_ASSERT(sel.idx != nullptr || sel.len == 0,
                "reduce_max: sel.idx must be non-null when sel.len > 0 [REQ-API-008]");
  return detail::k6_reduce_max(in.data, in.len, validity.bits, sel.idx, sel.len);
}

template <Element T>
SumType<T> reduce_sum_wrap(BatchView<T> in, BitmapView validity) noexcept {
  QUIVER_ASSERT(in.len >= 0 && in.len <= kMaxBatchLen,
                "reduce_sum_wrap: 0 <= n <= kMaxBatchLen [REQ-API-005]");
  return detail::k6_reduce_sum_wrap(in.data, in.len, validity.bits, nullptr, 0);
}

template <Element T>
SumType<T> reduce_sum_wrap(BatchView<T> in, BitmapView validity, SelVec sel) noexcept {
  QUIVER_ASSERT(sel.idx != nullptr || sel.len == 0,
                "reduce_sum_wrap: sel.idx must be non-null when sel.len > 0 [REQ-API-008]");
  return detail::k6_reduce_sum_wrap(in.data, in.len, validity.bits, sel.idx, sel.len);
}

// Returns true iff the mathematical sum is unrepresentable in SumType<T>; *out_sum then holds
// the wrapped value (API-K6-003). Integer-only; exact by 128-bit accumulation.
template <IntElement T>
bool reduce_sum_checked(BatchView<T> in, BitmapView validity, SumType<T>* out_sum) noexcept {
  QUIVER_ASSERT(in.len >= 0 && in.len <= kMaxBatchLen,
                "reduce_sum_checked: 0 <= n <= kMaxBatchLen [REQ-API-005]");
  return detail::k6_reduce_sum_checked(in.data, in.len, validity.bits, nullptr, 0, out_sum);
}

template <IntElement T>
bool reduce_sum_checked(BatchView<T> in, BitmapView validity, SelVec sel,
                        SumType<T>* out_sum) noexcept {
  QUIVER_ASSERT(sel.idx != nullptr || sel.len == 0,
                "reduce_sum_checked: sel.idx must be non-null when sel.len > 0 [REQ-API-008]");
  return detail::k6_reduce_sum_checked(in.data, in.len, validity.bits, sel.idx, sel.len, out_sum);
}

// One pass: min + max + null_count (selected-but-invalid positions) (API-K6-004).
template <Element T>
Sma<T> compute_sma(BatchView<T> in, BitmapView validity) noexcept {
  QUIVER_ASSERT(in.len >= 0 && in.len <= kMaxBatchLen,
                "compute_sma: 0 <= n <= kMaxBatchLen [REQ-API-005]");
  return detail::k6_compute_sma(in.data, in.len, validity.bits, nullptr, 0);
}

template <Element T>
Sma<T> compute_sma(BatchView<T> in, BitmapView validity, SelVec sel) noexcept {
  QUIVER_ASSERT(sel.idx != nullptr || sel.len == 0,
                "compute_sma: sel.idx must be non-null when sel.len > 0 [REQ-API-008]");
  return detail::k6_compute_sma(in.data, in.len, validity.bits, sel.idx, sel.len);
}

// Public-API delegation to K4 (API-K6-005; the documented cross-family exception, PRD 02 §6):
// count of valid positions, or of selected-and-valid positions with the SelVec overload.
inline std::int64_t reduce_count_valid(BitmapView validity, std::int64_t n) noexcept {
  QUIVER_ASSERT(n >= 0 && n <= kMaxBatchLen,
                "reduce_count_valid: 0 <= n <= kMaxBatchLen [REQ-API-005]");
  return validity.bits == nullptr ? n : mask_popcount(validity, n);
}

inline std::int64_t reduce_count_valid(BitmapView validity, std::int64_t n, SelVec sel) noexcept {
  QUIVER_ASSERT(sel.idx != nullptr || sel.len == 0,
                "reduce_count_valid: sel.idx must be non-null when sel.len > 0 [REQ-API-008]");
  if (validity.bits == nullptr) {
    return sel.len;
  }
  std::int64_t count = 0;
  for (std::int64_t j = 0; j < sel.len; ++j) {
    count += ((validity.bits[sel.idx[j] >> 3] >> (sel.idx[j] & 7)) & 1u) != 0 ? 1 : 0;
  }
  (void)n;
  return count;
}

QUIVER_END_NAMESPACE
