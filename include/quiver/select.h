// Surface A — K3 sel_convert: lossless bitmap <-> selection-vector conversion
// (API-K3-001/002); round-trips are identities. Primary instrument of the M9
// representation study (Charter §6.2; Survey §11.3 #3).
// Module: MOD-K3-SELECT | REQs: REQ-K3-001..002 | ADR-006, ADR-016, ADR-025
#pragma once

#include "quiver/core.h"
#include "quiver/detail/config.h"
#include "quiver/detail/extern_decls.h"

QUIVER_BEGIN_NAMESPACE

// Defined output = first count = popcount strictly increasing indices; capacity region n
// (REQ-MEM-008). Returns count (== mask_popcount(selection, n), REQ-K3-002).
inline std::int64_t bitmap_to_selvec(BitmapView selection, std::int64_t n,
                                     std::uint32_t* out_idx) noexcept {
  QUIVER_ASSERT(selection.bits != nullptr,
                "bitmap_to_selvec: selection must be non-null [REQ-API-008]");
  QUIVER_ASSERT(n >= 0 && n <= kMaxBatchLen,
                "bitmap_to_selvec: 0 <= n <= kMaxBatchLen [REQ-API-005]");
  return detail::k3_bitmap_to_selvec(selection.bits, n, out_idx);
}

// Writes ceil(n/8) bytes: set bits are exactly sel; all others (incl. tail) zero (ADR-016).
inline void selvec_to_bitmap(SelVec sel, std::int64_t n, std::uint8_t* out_bits) noexcept {
  QUIVER_ASSERT(sel.idx != nullptr || sel.len == 0,
                "selvec_to_bitmap: sel.idx must be non-null when sel.len > 0 [REQ-API-008]");
  QUIVER_ASSERT(n >= 0 && n <= kMaxBatchLen,
                "selvec_to_bitmap: 0 <= n <= kMaxBatchLen [REQ-API-005]");
  detail::k3_selvec_to_bitmap(sel.idx, sel.len, n, out_bits);
}

QUIVER_END_NAMESPACE
