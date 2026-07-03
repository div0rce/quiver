// Surface C — ISA query/override and version introspection (API-DISP-001..005).
// Module: MOD-DISPATCH | REQs: REQ-DISP-001..012, REQ-API-002 | ADRs: ADR-004, ADR-005
#pragma once

#include "quiver/core.h"

QUIVER_BEGIN_NAMESPACE

// The ISA dispatch would select now: highest CPU-supported tier, capped by the QUIVER_ISA
// environment override and any programmatic override (API-DISP-001). Thread-safe.
[[nodiscard]] Isa active_isa() noexcept;

// True iff the current CPU **and OS** can execute the tier (x86 includes XGETBV state checks;
// on ARM kNeon is always true and kAvx* always false) (API-DISP-002). Pure query.
[[nodiscard]] bool cpu_supports(Isa isa) noexcept;

// Caps dispatch at `isa` for subsequent calls via the policy epoch. Returns false (no state
// change) if the tier is unsupported; kScalar is always accepted. Intended for benchmarking
// and diagnostics, not steady-state concurrent mutation (API-DISP-003, REQ-DISP-006/-009).
bool set_isa_override(Isa isa) noexcept;

// Restores the hardware+environment policy and bumps the policy epoch (API-DISP-003).
void clear_isa_override() noexcept;

// Eagerly resolves every dispatch entry under the current policy; idempotent, thread-safe
// (API-DISP-004, REQ-DISP-010).
void warmup() noexcept;

// Compile-embedded version; the string has static storage duration (API-DISP-005).
struct Version {
  int major;
  int minor;
  int patch;
};
[[nodiscard]] Version version() noexcept;
[[nodiscard]] const char* version_string() noexcept;

QUIVER_END_NAMESPACE
