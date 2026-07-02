# ADR-015 — Tail handling policy

- **Identifier:** ADR-015
- **Title:** Tail handling policy
- **Status:** Accepted
- **Source:** extracted verbatim from the Engineering PRD, [docs/prd/09-simd-architecture.md](../prd/09-simd-architecture.md) (REQ-DOC-004). From M0 onward this file is the canonical, living ADR record; status changes happen here and in the [index](README.md), with the PRD text remaining the historical record.

---

- **Status:** Accepted.
- **Context:** Charter §7.2 no-over-read default; batch tails (`n mod W`) are the classic over-read temptation.
- **Alternatives:** (1) read-past-end with page-boundary checks — rejected outright: violates the charter's sanitizer-clean pledge, unvendorable; (2) require padded buffers — rejected: that is the `_padded` variant family, deliberately not shipped in v1 (REQ-MEM-002); (3) scalar tails everywhere — safe but leaves AVX-512 masking value unused; (4) **scalar tails on AVX2/NEON + native masking on AVX-512** (selected) — masking is architecturally access-suppressing, keeping the contract while showcasing the ISA (Survey §4.1: "the ISA matters more than the width").
- **Consequences:** tail code is a per-family shared helper (MOD-KCOMMON); guard-page tests place batch ends flush against protected pages for every kernel × ISA ([12 §2](../prd/12-testing-architecture.md), REQ-TEST-006).
- **Reconsideration:** ledger evidence that scalar tails dominate small-batch cost for a hot family may motivate `_padded` variants — via the REQ-MEM-002 amendment path, never silently.
- **Related:** REQ-SIMD-003, REQ-MEM-001/002.
