# ADR-017 — Contract-based error model

- **Identifier:** ADR-017
- **Title:** Contract-based error model
- **Status:** Accepted
- **Source:** extracted verbatim from the Engineering PRD, [docs/prd/16-error-handling.md](../prd/16-error-handling.md) (REQ-DOC-004). From M0 onward this file is the canonical, living ADR record; status changes happen here and in the [index](README.md), with the PRD text remaining the historical record.

---

- **Status:** Accepted.
- **Context:** Charter §7.3 fixes `noexcept` + `-fno-exceptions` usability and debug-assert posture; HFT persona (P2) requires no hidden control flow; kernels are total functions on their contract domain.
- **Alternatives:** (1) status-code returns (`quiver::Status`) — rejected: pollutes every hot signature for conditions that are programming errors, not runtime events; semantic returns (counts) would need out-params; (2) exceptions — charter-prohibited; (3) `std::expected` — same objection as (1) plus ABI/codegen weight; (4) **contracts + debug asserts + semantic returns** (selected) — matches every surveyed engine's internal-kernel practice and the simdjson-class library posture (OA §3).
- **Consequences:** documentation must be unambiguous about preconditions (REQ-DOC-002 template §2 carries them); fuzzing must generate in-contract inputs except where robustness is the goal (K8, ADR-025 consequence).
- **Reconsideration:** a C-ABI shim (future) may add error codes at *that* boundary without touching this model.
- **Related:** REQ-ERR-001..007, REQ-API-002, Charter §7.3.
