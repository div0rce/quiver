# ADR-023 — Aliasing contract

- **Identifier:** ADR-023
- **Title:** Aliasing contract
- **Status:** Accepted
- **Source:** extracted verbatim from the Engineering PRD, [docs/prd/06-memory-model.md](../prd/06-memory-model.md) (REQ-DOC-004). From M0 onward this file is the canonical, living ADR record; status changes happen here and in the [index](README.md), with the PRD text remaining the historical record.

---

- **Status:** Accepted.
- **Context:** engines compact and transform in place; blanket no-aliasing wastes real use cases, blanket allowance forbids `restrict`-quality codegen.
- **Problem:** which overlap is legal, per kernel.
- **Alternatives:** (a) prohibit all aliasing — rejected: forces copies for in-place filter, a top real-world pattern; (b) allow arbitrary overlap — rejected: unimplementable efficiently, unverifiable; (c) **exact-alias allowlist** (selected): partial overlap always prohibited; `out == input` pointer-equality permitted only where a forward-pass safety invariant holds.
- **Decision — the allowlist:**

| Kernel | Permitted exact aliasing | Safety invariant |
|---|---|---|
| K2 `filter` | `out == in.data` | write index ≤ read index in a forward scan |
| K4 combine/not | `out_bits == a.bits` (or `b.bits`) | pure elementwise on words |
| K7 `hash64_combine` | `out == a` or `out == b` | elementwise |
| K9/K10 value outputs | `out == a.data` or `out == b.data`; `out_validity ==` either validity | elementwise |
| all others | none | gather/scatter/conversion patterns lack a safe order |

- **Consequences:** internal implementations may apply `QUIVER_RESTRICT` only where aliasing is prohibited; the allowlist rows are covered by dedicated unit tests (in-place result == out-of-place result).
- **Reconsideration:** adding a row requires demonstrating the invariant and a test, via PRD amendment.
- **Related:** REQ-MEM-005, REQ-API-012.
