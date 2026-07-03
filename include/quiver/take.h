// Surface A — K5 take / dict_decode: gather by index; dictionary decode as its special case;
// the fused form touches only surviving positions and packs its output (API-K5-001..003).
// take permits arbitrary order and duplicates; every index must be in-bounds — full-scan
// debug-asserted, UB in release (ADR-025, REQ-ERR-004).
// Module: MOD-K5-TAKE | REQs: REQ-K5-001..003 | ADR-006, ADR-025
#pragma once

#include "quiver/core.h"
#include "quiver/detail/config.h"
#include "quiver/detail/extern_decls.h"

QUIVER_BEGIN_NAMESPACE

// out[i] = values.data[indices.idx[i]]; writes exactly indices.len elements.
template <Element T>
void take(BatchView<T> values, SelVec indices, T* out) noexcept {
  QUIVER_ASSERT(indices.idx != nullptr || indices.len == 0,
                "take: indices must be non-null when len > 0 [REQ-API-008]");
  QUIVER_ASSERT(indices.len >= 0 && indices.len <= kMaxBatchLen,
                "take: 0 <= indices.len <= kMaxBatchLen [REQ-API-005]");
  detail::k5_take(values.data, values.len, indices.idx, indices.len, out);
}

// out[i] = dict.data[codes[i]]; writes exactly n elements.
template <Element T, CodeType C>
void dict_decode(BatchView<T> dict, const C* codes, std::int64_t n, T* out) noexcept {
  QUIVER_ASSERT(n >= 0 && n <= kMaxBatchLen, "dict_decode: 0 <= n <= kMaxBatchLen [REQ-API-005]");
  detail::k5_dict_decode(dict.data, dict.len, codes, n, nullptr, 0, out);
}

// Fused: out[j] = dict.data[codes[sel.idx[j]]], packed; only selected code positions are read
// (REQ-K5-003). Writes exactly sel.len elements.
template <Element T, CodeType C>
void dict_decode(BatchView<T> dict, const C* codes, std::int64_t n, SelVec sel, T* out) noexcept {
  QUIVER_ASSERT(sel.idx != nullptr || sel.len == 0,
                "dict_decode: sel.idx must be non-null when sel.len > 0 [REQ-API-008]");
  QUIVER_ASSERT(n >= 0 && n <= kMaxBatchLen, "dict_decode: 0 <= n <= kMaxBatchLen [REQ-API-005]");
  detail::k5_dict_decode(dict.data, dict.len, codes, n, sel.idx, sel.len, out);
}

QUIVER_END_NAMESPACE
