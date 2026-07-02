# ADR-019 — Documentation toolchain

- **Identifier:** ADR-019
- **Title:** Documentation toolchain
- **Status:** Accepted
- **Source:** extracted verbatim from the Engineering PRD, [docs/prd/14-documentation.md](../prd/14-documentation.md) (REQ-DOC-004). From M0 onward this file is the canonical, living ADR record; status changes happen here and in the [index](README.md), with the PRD text remaining the historical record.

---

- **Status:** Accepted.
- **Context:** small stable API surface (≈30 entry points); docs are a primary adoption artifact; solo maintenance (T8: boring).
- **Alternatives:** (1) Doxygen(+Breathe) — rejected: generated reference adds toolchain weight and reads worse than curated pages for a surface this small; header comments would duplicate the [04](../prd/04-public-api.md) contracts; (2) plain README tree without a site — rejected: navigation and strict link checking matter for the ledger's credibility; (3) **MkDocs Material with hand-written reference pages + compiled snippet extraction** (selected).
- **Consequences:** API pages are hand-maintained — REQ-DOC-002/012 synchronization discipline is the price; the M10 freeze review includes a signature-by-signature docs-vs-headers audit.
- **Reconsideration:** if the API surface triples (v2), revisit generated reference.
- **Related:** REQ-DOC-002/005/006.
