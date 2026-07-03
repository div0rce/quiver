// Surface A — K2 filter: dense order-preserving compaction (API-K2-001/002).
// Exact aliasing out == in.data is permitted (forward-scan invariant, ADR-023).
// Module: MOD-K2-FILTER | REQs: REQ-K2-001..003 | ADR-006, ADR-023
#pragma once

#include "quiver/core.h"
#include "quiver/detail/config.h"
#include "quiver/detail/extern_decls.h"

QUIVER_BEGIN_NAMESPACE

// Bitmap-driven: defined output = first count = popcount elements; capacity region n
// (REQ-MEM-008). `selection` must be non-null (REQ-API-008: only `validity` params may be
// null). Returns count.
template <Element T>
std::int64_t filter(BatchView<T> in, BitmapView selection, T* out) noexcept {
  QUIVER_ASSERT(selection.bits != nullptr, "filter: selection must be non-null [REQ-API-008]");
  QUIVER_ASSERT(in.len >= 0 && in.len <= kMaxBatchLen,
                "filter: 0 <= n <= kMaxBatchLen [REQ-API-005]");
  return detail::k2_filter_bitmap(in.data, in.len, selection.bits, out);
}

// Selvec-driven: writes exactly sel.len elements in O(sel.len) (REQ-K2-003).
template <Element T>
std::int64_t filter(BatchView<T> in, SelVec sel, T* out) noexcept {
  QUIVER_ASSERT(sel.idx != nullptr || sel.len == 0,
                "filter: sel.idx must be non-null when sel.len > 0 [REQ-API-008]");
  QUIVER_ASSERT(sel.len >= 0 && sel.len <= kMaxBatchLen,
                "filter: 0 <= sel.len <= kMaxBatchLen [REQ-API-005]");
  return detail::k2_filter_selvec(in.data, sel.idx, sel.len, out);
}

QUIVER_END_NAMESPACE
