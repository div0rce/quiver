# 16 — Error Handling

## 1. Purpose

The complete error model. Quiver is contract-based: there are no runtime error channels on hot paths — there are contracts, debug assertions, and semantic return values. Upstream authority: Charter §7.3 (no exceptions, `-fno-exceptions` usability), §7.4 (no silent overflow), Appendix A.

## 2. The model

```text
Input class                         → Behavior
──────────────────────────────────────────────────────────────────────
Contract-satisfying                 → defined result, memory-safe, deterministic (REQ-SEC-001)
Contract-violating, O(1)-checkable  → QUIVER_ASSERT (debug) / UB (release)
Contract-violating, O(n)-checkable  → QUIVER_ASSERT in debug only where listed (§3) / UB (release)
Semantic conditions (not errors)    → return values: counts, bool overflow, bool override-accepted
Environmental conditions            → none exist in the library (REQ-SEC-005)
```

## 3. Requirements

| ID | Requirement |
|---|---|
| REQ-ERR-001 | No public function shall throw, return an error code object, set `errno`, or log. Error-like information flows only through documented return values (K1/K2/K3 counts; K6-003 and K10-001 overflow reports; API-DISP-003 acceptance bool). |
| REQ-ERR-002 | `QUIVER_ASSERT` behavior per REQ-CORE-003 (stderr one-liner + `std::abort`). The assert message shall name the violated contract in the form `<api>: <condition> [REQ-or-API-id]`. |
| REQ-ERR-003 | O(1)-checkable preconditions shall be asserted in every debug build: null-pointer rules (REQ-API-008), `n` range (REQ-API-005), `bit_width` range (K8), enum validity, pointer-equality aliasing violations (REQ-MEM-005), length equality (K1-002/K9/K10). |
| REQ-ERR-004 | O(n)-checkable preconditions asserted in debug builds (documented as debug-only cost): selection-vector sortedness/range (ADR-025), `take`/`dict_decode` index bounds (REQ-K5-002). Release builds perform no such scans. |
| REQ-ERR-005 | The assert handler is fixed in v1 (no user hook — [21-future-work.md](21-future-work.md)); the handler shall be async-signal-unsafe-free (single `write(2)`-style emission + abort) so it behaves in constrained environments. |
| REQ-ERR-006 | Overflow is never silent (Charter §7.4): K6/K10 checked variants report exactly per ADR-014 ([08](08-kernel-design.md)); wrapping and saturating variants state their semantics in their names; no other kernel has an overflow concept (K9 wraps by name). |
| REQ-ERR-007 | Every asserted precondition has a death test (REQ-TEST-014); assert-off builds are validated by the same suites minus death tests. |
| REQ-ERR-008 | Diagnostics for validation failures in tests/benches follow REQ-TEST-012 / REQ-BENCH-004 formats — the library itself never produces diagnostics beyond REQ-ERR-002. |

## 4. ADR-017 — Contract-based error model

- **Status:** Accepted.
- **Context:** Charter §7.3 fixes `noexcept` + `-fno-exceptions` usability and debug-assert posture; HFT persona (P2) requires no hidden control flow; kernels are total functions on their contract domain.
- **Alternatives:** (1) status-code returns (`quiver::Status`) — rejected: pollutes every hot signature for conditions that are programming errors, not runtime events; semantic returns (counts) would need out-params; (2) exceptions — charter-prohibited; (3) `std::expected` — same objection as (1) plus ABI/codegen weight; (4) **contracts + debug asserts + semantic returns** (selected) — matches every surveyed engine's internal-kernel practice and the simdjson-class library posture (OA §3).
- **Consequences:** documentation must be unambiguous about preconditions (REQ-DOC-002 template §2 carries them); fuzzing must generate in-contract inputs except where robustness is the goal (K8, ADR-025 consequence).
- **Reconsideration:** a C-ABI shim (future) may add error codes at *that* boundary without touching this model.
- **Related:** REQ-ERR-001..007, REQ-API-002, Charter §7.3.

## 5. Failure modes / acceptance / traceability

The library's only observable failure behavior is the debug assert path; everything else is defined results or contract UB bounded by [15](15-security-and-ub.md). **Acceptance:** death-test rows exist for every §3 item; assert messages conform to REQ-ERR-002 format (unit-tested via death-test regex); `-fno-exceptions` consumer example compiles (M8 consumption test). **Traceability:** Charter §7.3/§7.4/Appendix A → REQ-ERR-001..008 → ADR-017/ADR-014 → [04](04-public-api.md) invalid-input rows → tests ([12](12-testing-architecture.md)) → all kernel milestones.
