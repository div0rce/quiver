# ADR-011 — Equal-ISA auto-vectorized baselines

- **Identifier:** ADR-011
- **Title:** Equal-ISA auto-vectorized baselines
- **Status:** Accepted
- **Source:** extracted verbatim from the Engineering PRD, [docs/prd/10-benchmark-architecture.md](../prd/10-benchmark-architecture.md) (REQ-DOC-004). From M0 onward this file is the canonical, living ADR record; status changes happen here and in the [index](README.md), with the PRD text remaining the historical record.

---

- **Status:** Accepted.
- **Context:** Charter T7 requires every kernel page to lead with the explicit-vs-autovec comparison; a *fair* autovec baseline must be allowed to use the same ISA as the explicit path — comparing AVX-512 intrinsics against SSE2-constrained scalar codegen would be the strawman-baseline pitfall (Survey §7.5; ADMS 2023 method, Survey §4.5).
- **Problem:** produce compiler-vectorized code at each ISA level without shipping it or polluting the library.
- **Alternatives:** (1) compile whole library at `-march=<level>` variants — rejected: N library builds, dispatch confusion; (2) benchmark against external engines' kernels — rejected: apples-vs-oranges (Survey §7.5) and license entanglement; (3) **bench-tree TUs re-including `_scalar_impl.h` inside target regions** (selected) — exactly the same C++ the scalar backend runs, freely auto-vectorized at each ISA level, existing only in the bench tree.
- **Consequences:** REQ-SIMD-006 purity of `_impl.h` is load-bearing; the ledger's `variant` axis carries the comparison; verdicts (including losses) are publication requirements (REQ-LEDGER-011).
- **Reconsideration:** none — this is the methodological core of the product.
- **Related:** REQ-BENCH-010, REQ-SIMD-006, Charter T7, OA §13 (ledger identity).
