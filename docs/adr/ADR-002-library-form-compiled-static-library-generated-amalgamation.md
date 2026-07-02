# ADR-002 — Library form: compiled static library + generated amalgamation

- **Identifier:** ADR-002
- **Title:** Library form: compiled static library + generated amalgamation
- **Status:** Accepted
- **Source:** extracted verbatim from the Engineering PRD, [docs/prd/03-build-system.md](../prd/03-build-system.md) (REQ-DOC-004). From M0 onward this file is the canonical, living ADR record; status changes happen here and in the [index](README.md), with the PRD text remaining the historical record.

---

- **Status:** Accepted.
- **Context:** Charter T4 demands three consumption modes including a two-file drop-in; Charter §7.3 demands `-fno-exceptions` compatibility; ADR-003 requires per-ISA compiled code.
- **Problem:** header-only vs compiled vs compiled+amalgamation.
- **Alternatives:**
  1. *Header-only.* + zero build integration. − every consumer TU recompiles all ISA paths; target-region pragmas inside consumer-included headers leak diagnostics and break under MSVC; dispatch table initialization becomes ODR-fragile; compile-time cost contradicts vendorability in practice.
  2. *Compiled static library only.* + clean. − no two-file vendoring path; fails Charter §6.5.
  3. *Compiled library + generated amalgamation* (selected). + normal builds stay clean; vendoring path is the proven simdjson/fsst pattern (OA §3, §8). − amalgamation generator must be maintained and verified (mitigated by REQ-BUILD-013's identical-output gate).
- **Decision:** Alternative 3.
- **Consequences:** MOD-AMALG exists (M8); release assets carry the pair (REQ-REPO-011).
- **Reconsideration:** if C++ modules become the ecosystem vendoring norm.
- **Related:** REQ-BUILD-002/010/013, ADR-003, ADR-018, Charter T4/§6.5.
