// Surface A — K4 mask_algebra: bitmap boolean algebra and cardinality; the null-propagation
// primitive (API-K4-001..004). All bitmap parameters here are non-null by contract — these
// APIs ARE the mask operations; the null-means-all-valid shorthand does not apply (PRD 04 K4).
// Exact aliasing out_bits == a.bits or b.bits permitted (ADR-023).
// Module: MOD-K4-MASK | REQs: REQ-K4-001..003 | ADR-006, ADR-016, ADR-023
#pragma once

#include "quiver/core.h"
#include "quiver/detail/config.h"
#include "quiver/detail/extern_decls.h"

QUIVER_BEGIN_NAMESPACE

inline void mask_combine(MaskOp op, BitmapView a, BitmapView b, std::int64_t n,
                         std::uint8_t* out_bits) noexcept {
  QUIVER_ASSERT(a.bits != nullptr && b.bits != nullptr,
                "mask_combine: bitmaps must be non-null [PRD 04 K4]");
  QUIVER_ASSERT(n >= 0 && n <= kMaxBatchLen, "mask_combine: 0 <= n <= kMaxBatchLen [REQ-API-005]");
  detail::k4_mask_combine(op, a.bits, b.bits, n, out_bits);
}

inline void mask_not(BitmapView a, std::int64_t n, std::uint8_t* out_bits) noexcept {
  QUIVER_ASSERT(a.bits != nullptr, "mask_not: bitmap must be non-null [PRD 04 K4]");
  QUIVER_ASSERT(n >= 0 && n <= kMaxBatchLen, "mask_not: 0 <= n <= kMaxBatchLen [REQ-API-005]");
  detail::k4_mask_not(a.bits, n, out_bits);
}

inline std::int64_t mask_popcount(BitmapView a, std::int64_t n) noexcept {
  QUIVER_ASSERT(a.bits != nullptr, "mask_popcount: bitmap must be non-null [PRD 04 K4]");
  QUIVER_ASSERT(n >= 0 && n <= kMaxBatchLen, "mask_popcount: 0 <= n <= kMaxBatchLen [REQ-API-005]");
  return detail::k4_mask_popcount(a.bits, n);
}

// Vacuous truths for n == 0: all = true, any = false, none = true (PRD 04 K4).
inline bool mask_all(BitmapView a, std::int64_t n) noexcept {
  QUIVER_ASSERT(a.bits != nullptr, "mask_all: bitmap must be non-null [PRD 04 K4]");
  QUIVER_ASSERT(n >= 0 && n <= kMaxBatchLen, "mask_all: 0 <= n <= kMaxBatchLen [REQ-API-005]");
  return detail::k4_mask_all(a.bits, n);
}

inline bool mask_any(BitmapView a, std::int64_t n) noexcept {
  QUIVER_ASSERT(a.bits != nullptr, "mask_any: bitmap must be non-null [PRD 04 K4]");
  QUIVER_ASSERT(n >= 0 && n <= kMaxBatchLen, "mask_any: 0 <= n <= kMaxBatchLen [REQ-API-005]");
  return detail::k4_mask_any(a.bits, n);
}

inline bool mask_none(BitmapView a, std::int64_t n) noexcept {
  QUIVER_ASSERT(a.bits != nullptr, "mask_none: bitmap must be non-null [PRD 04 K4]");
  QUIVER_ASSERT(n >= 0 && n <= kMaxBatchLen, "mask_none: 0 <= n <= kMaxBatchLen [REQ-API-005]");
  return detail::k4_mask_none(a.bits, n);
}

QUIVER_END_NAMESPACE
