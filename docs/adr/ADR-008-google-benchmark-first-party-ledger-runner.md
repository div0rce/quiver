# ADR-008 — Google Benchmark + first-party ledger runner

- **Identifier:** ADR-008
- **Title:** Google Benchmark + first-party ledger runner
- **Status:** Accepted
- **Source:** extracted verbatim from the Engineering PRD, [docs/prd/10-benchmark-architecture.md](../prd/10-benchmark-architecture.md) (REQ-DOC-004). From M0 onward this file is the canonical, living ADR record; status changes happen here and in the [index](README.md), with the PRD text remaining the historical record.

---

- **Status:** Accepted.
- **Context:** the shipped library is zero-dependency; the bench tree is dev-only (Charter T4 scoping); the ledger needs process-level repetitions, manifests, and custom statistics no in-process harness provides (Survey §7.4).
- **Alternatives:** (1) fully first-party harness — rejected: re-implements solved timing/iteration estimation (GB's loop sizing, `DoNotOptimize`), burns solo budget (OA §10) against T8; (2) GB alone — rejected: repetitions within one process share warmup/layout state; no environment manifests; statistics policy (bootstrap CIs) unsupported; (3) **GB for in-process measurement + Python runner for orchestration/statistics** (selected) — clean split: GB owns "measure this loop well," the runner owns "measure it *credibly*." (4) nanobench — rejected: dormant upstream (OA §2).
- **Consequences:** GB pin must track upstream (risk R-13); the runner's GB-JSON parser is schema-tolerant by contract (ignores unknown fields).
- **Reconsideration:** GB breaking-change or abandonment → re-evaluate alternative 1 with the then-existing harness code.
- **Related:** REQ-BENCH-001, REQ-INT-004, REQ-LEDGER-*.
