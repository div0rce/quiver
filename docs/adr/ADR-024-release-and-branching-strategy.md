# ADR-024 — Release and branching strategy

- **Identifier:** ADR-024
- **Title:** Release and branching strategy
- **Status:** Accepted
- **Source:** extracted verbatim from the Engineering PRD, [docs/prd/19-release-plan.md](../prd/19-release-plan.md) (REQ-DOC-004). From M0 onward this file is the canonical, living ADR record; status changes happen here and in the [index](README.md), with the PRD text remaining the historical record.

---

- **Status:** Accepted. **Context:** solo maintainer (OA §10), boring-on-purpose (T8), charter SemVer commitment. **Problem:** choose branching and release mechanics that keep every mainline state releasable with one maintainer. **Constraints:** milestone-gated releases (REQ-MS-001); charter §7.5 SemVer rules; no long-lived branch divergence is tolerable at a bus factor of one. **Alternatives:** GitFlow (rejected: ceremony without a team), release trains (rejected: milestone-gated releases fit a contract-driven plan better). **Decision:** trunk-based + milestone-tagged releases + manual publication of CI-built drafts. **Consequences:** every merge to `main` is releasable by construction (master prompt Part 11 repository-evolution rule). **Reconsideration:** multi-maintainer future. **Related:** REQ-REL-001..003.
