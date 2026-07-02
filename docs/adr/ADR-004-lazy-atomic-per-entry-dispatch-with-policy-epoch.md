# ADR-004 — Lazy atomic per-entry dispatch with policy epoch

- **Identifier:** ADR-004
- **Title:** Lazy atomic per-entry dispatch with policy epoch
- **Status:** Accepted
- **Source:** extracted verbatim from the Engineering PRD, [docs/prd/07-runtime-dispatch.md](../prd/07-runtime-dispatch.md) (REQ-DOC-004). From M0 onward this file is the canonical, living ADR record; status changes happen here and in the [index](README.md), with the PRD text remaining the historical record.

---

**ADR-004 — Lazy atomic per-entry dispatch with policy epoch.** *Status:* Accepted. *Context:* Charter §6.3 requires runtime dispatch + benchmarking requires forcing specific backends ([10](../prd/10-benchmark-architecture.md)); Charter forbids allocation/locks in the library. *Problem:* dispatch that is near-zero-overhead, override-capable mid-process, and initialization-order-safe. *Alternatives:* (1) GCC/Clang `ifunc` — rejected: no override capability, ELF-only, complicates macOS/MSVC and amalgamation; (2) eager init via static constructor — rejected: dynamic initialization prohibited (REQ-CORE-004), order fiasco risk in static-lib consumers; (3) per-call `if (feature)` branching — rejected: per-call overhead scales with tiers, pollutes branch predictor; (4) resolve-once atomic pointer *without* epoch — rejected: overrides could not retract already-resolved entries, making forced-variant benchmarking unsound; (5) **lazy atomic entries + epoch** (selected). *Tradeoffs:* one extra epoch load per call (measured, `bench_dispatch`) buys sound overrides and testability. *Consequences:* the epoch protocol must be TSan-proven; documented as above. *Reconsideration:* if `bench_dispatch` shows the epoch check costing > 1% on any 4K-element kernel invocation on a reference machine, evaluate collapsing to alternative 4 with process-start-only overrides (PRD amendment). *Related:* REQ-DISP-003/006/007/008/009, API-DISP-003.
