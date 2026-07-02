# ADR-021 — Ledger data model and storage

- **Identifier:** ADR-021
- **Title:** Ledger data model and storage
- **Status:** Accepted
- **Source:** extracted verbatim from the Engineering PRD, [docs/prd/11-performance-ledger.md](../prd/11-performance-ledger.md) (REQ-DOC-004). From M0 onward this file is the canonical, living ADR record; status changes happen here and in the [index](README.md), with the PRD text remaining the historical record.

---

- **Status:** Accepted.
- **Context:** Charter Surface D promises machine-readable, independently versioned results; disputes must be answerable from artifacts alone (Charter §12).
- **Alternatives:** (1) database/service — rejected: infrastructure burden, repo-external trust; (2) one growing JSON/CSV file — rejected: merge conflicts, no per-run manifests; (3) **append-only per-run directories with schema-validated JSON committed via PR** (selected): reviewable diffs, git provenance, zero infrastructure.
- **Consequences:** repo size grows with runs (raw JSON retained) — acceptable at v1 scale (~tens of MB); revisit with git-lfs if >200 MB ([21](../prd/21-future-work.md)).
- **Reconsideration:** result volume or an external dashboard need.
- **Related:** REQ-LEDGER-001/010.
