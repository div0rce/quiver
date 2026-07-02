# ADR-006 — Public API style

- **Identifier:** ADR-006
- **Title:** Public API style
- **Status:** Accepted
- **Source:** extracted verbatim from the Engineering PRD, [docs/prd/04-public-api.md](../prd/04-public-api.md) (REQ-DOC-004). From M0 onward this file is the canonical, living ADR record; status changes happen here and in the [index](README.md), with the PRD text remaining the historical record.

---

- **Status:** Accepted.
- **Context:** Charter Surface B promises non-owning view vocabulary types; Charter §7.6 anticipates a future C ABI; kernels are compiled per-ISA in TUs (ADR-003), so public templates cannot carry implementations.
- **Problem:** how callers name types and how templates meet compiled code.
- **Alternatives:** (1) `std::span`-based signatures — rejected: `size_t` vs `int64` friction, no place to hang bitmap/selvec semantics, harder future C shim; (2) rich owning/RAII types — rejected: violates T6/REQ-API-003; (3) pure C-style pointer+length everywhere — rejected: charter names view structs; (4) **first-party trivially-copyable view structs + thin constrained-template façade over concrete per-type `detail::` symbols** (selected).
- **Decision:** Alternative 4. The façade is `inline` `if constexpr` type switches (or tag-dispatched calls) to `quiver::detail::<kernel>_<type>` functions declared in `detail/extern_decls.h` and defined in kernel TUs. One concrete symbol per admissible template-parameter combination (10 element types; ×3 code types for K5 `dict_decode`; fewer where a concept narrows the set).
- **Consequences:** + per-ISA compilation works; public headers stay light; C shim later is mechanical. − a fixed extern-symbol inventory must be maintained (bounded: closed type list × closed API list).
- **Reconsideration:** C ABI work (future) may promote `detail` symbols to a stable `qv_` C surface.
- **Related:** REQ-API-004/007, ADR-003, Charter §7.1/§7.6.
