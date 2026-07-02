# ADR-022 — First-party PMU wrapper

- **Identifier:** ADR-022
- **Title:** First-party PMU wrapper
- **Status:** Accepted
- **Source:** extracted verbatim from the Engineering PRD, [docs/prd/10-benchmark-architecture.md](../prd/10-benchmark-architecture.md) (REQ-DOC-004). From M0 onward this file is the canonical, living ADR record; status changes happen here and in the [index](README.md), with the PRD text remaining the historical record.

---

- **Status:** Accepted.
- **Context:** cycles/value and miss rates are ledger fields (Charter §6.4); GB's perf-counter path requires libpfm (a dependency); Apple has no public PMU API (Survey §7.3).
- **Alternatives:** (1) GB+libpfm — rejected: dev-dependency creep, less control of grouping/multiplexing; (2) `perf stat` subprocess parsing — rejected: per-iteration attribution impossible; (3) **first-party `perf_event_open` wrapper (~200 lines)** (selected): explicit group creation, no multiplexing by construction (fail-and-drop rather than silently multiplex, priority order documented in REQ-BENCH-005), counters read around GB's timed loop via GB custom counters.
- **Consequences:** Linux-only; macOS entries ship without PMU columns, labeled secondary (Charter §6.4); documented in the methodology page.
- **Reconsideration:** macOS kperf integration is future work ([21](../prd/21-future-work.md)) — private-API risk stays out of v1 (OA §5 C12 red team).
- **Related:** REQ-BENCH-005, REQ-LEDGER-008.
