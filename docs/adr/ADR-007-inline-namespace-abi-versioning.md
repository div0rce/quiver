# ADR-007 — Inline-namespace ABI versioning

- **Identifier:** ADR-007
- **Title:** Inline-namespace ABI versioning
- **Status:** Accepted
- **Source:** extracted verbatim from the Engineering PRD, [docs/prd/04-public-api.md](../prd/04-public-api.md) (REQ-DOC-004). From M0 onward this file is the canonical, living ADR record; status changes happen here and in the [index](README.md), with the PRD text remaining the historical record.

---

- **Status:** Accepted. **Context/Problem:** static-library symbol collisions across future major versions; charter promises SemVer.
- **Alternatives:** no namespace versioning (rejected: forecloses v2 coexistence), preprocessor-renamed symbols (rejected: ugly, non-idiomatic).
- **Decision:** `namespace quiver { inline namespace v1 { … } }`; the inline namespace increments only on ABI-epoch changes (major versions with incompatible types).
- **Consequences:** user code spells `quiver::` unchanged; mangled names carry `v1`. **Reconsideration:** C ABI milestone. **Related:** REQ-API-001/009.
