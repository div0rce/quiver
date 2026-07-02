# ADR-009 — Testing stack

- **Identifier:** ADR-009
- **Title:** Testing stack
- **Status:** Accepted
- **Source:** extracted verbatim from the Engineering PRD, [docs/prd/12-testing-architecture.md](../prd/12-testing-architecture.md) (REQ-DOC-004). From M0 onward this file is the canonical, living ADR record; status changes happen here and in the [index](README.md), with the PRD text remaining the historical record.

---

- **Status:** Accepted.
- **Context:** dev-only dependencies are permitted but must stay pinned and minimal (Charter T4 scoping, REQ-BUILD-007); the oracle scheme (dual oracle + differential fuzzing) is first-party by necessity — no framework provides it.
- **Problem:** select the assertion framework, property-testing approach, and fuzzing engine.
- **Alternatives:** (1) Catch2/doctest — viable; rejected on T8 grounds: GoogleTest is the convention the target contributor pool knows, and death tests (REQ-TEST-014) are first-class there; (2) rapidcheck/fuzztest for properties — rejected: unpinned maturity risk and overlapping roles; seeded first-party generators (MOD-TESTKIT) already provide deterministic property enumeration with better failure diagnostics (REQ-TEST-012); (3) AFL++ — rejected: libFuzzer integrates with the sanitizer toolchain in-process and is OSS-Fuzz-shaped (F-14).
- **Decision:** GoogleTest (pinned) + first-party seeded generators/oracles + libFuzzer with structured differential harnesses.
- **Consequences:** property "shrinking" is manual (seed + axis printing substitutes); acceptable at this API scale.
- **Reconsideration:** if property-test volume outgrows the first-party runner (v2 kernels).
- **Related:** REQ-TEST-001/002/007/008/012, REQ-BUILD-007.
