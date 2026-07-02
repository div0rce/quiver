// The single X-macro inventory of concrete kernel symbols. It drives BOTH the extern
// declarations consumed by the public template facades AND the dispatch entry table —
// table/declaration consistency by construction (PRD 07 §4, REQ-KERNEL-005).
// Module: MOD-CORE (declarations) / MOD-DISPATCH (table emission) | ADR-006
#pragma once

#include "quiver/core.h"

// Entry format (fixed now, populated from M3):
//   X(entry_index, family, api_name, element_type, representation)
//
// M1 state: the inventory is EMPTY — Tier A families populate it at M3, Tier B at M6
// (docs/prd/18-milestones.md). The dispatch framework below it is fully operational and
// tested against synthetic entries (tests/unit/test_dispatch.cpp).
#define QUIVER_KERNEL_ENTRY_LIST(X) /* empty until M3 */

QUIVER_BEGIN_NAMESPACE
namespace detail {

// Number of concrete kernel symbols in the inventory.
inline constexpr int kKernelEntryCount = 0
// NOLINTNEXTLINE(bugprone-macro-parentheses): X-macro summation idiom — `+1` terms
// concatenate onto the leading 0; parenthesizing would break the expression.
#define QUIVER_COUNT_ENTRY(idx, family, api, type, repr) +1
    QUIVER_KERNEL_ENTRY_LIST(QUIVER_COUNT_ENTRY)
#undef QUIVER_COUNT_ENTRY
    ;

}  // namespace detail
QUIVER_END_NAMESPACE
