# 19 — Release Plan

## 1. Purpose

Versioning, branching, the release process, and the version-by-version maturity map. Upstream authority: Charter §7.5 (SemVer, 0.x rules, 1.0 freeze), §9.1 (scorecard), §12 (governance).

## 2. Requirements

| ID | Requirement |
|---|---|
| REQ-REL-001 | Versioning is SemVer, tags `vX.Y.Z`. During 0.x, breaking changes are permitted with a minor bump + CHANGELOG entry (Charter §7.5). From v1.0, surfaces A–C are frozen (REQ-API-009); qhash64 changes are major-version events (Charter §7.4). |
| REQ-REL-002 | Branching is trunk-based: protected `main`, short-lived PR branches, release tags on `main`. No long-lived release branches before v1.0; post-1.0 patch releases may branch from tags. |
| REQ-REL-003 | The release process is the fixed checklist of §5, executed via `release.yml` (REQ-CI-009) with manual publication. Every step's evidence lands in `docs/releases/gates/` (REQ-MS-002). |
| REQ-REL-004 | Release artifacts: GitHub source archive, amalgamation pair (`quiver.h` + `quiver.cpp`, from M8), `SHA256SUMS`, artifact attestation (REQ-SEC-006). Amalgamation artifacts are release-only, never committed (REQ-REPO-011). |
| REQ-REL-005 | Release notes per REQ-DOC-010 template for every tag. |
| REQ-REL-006 | Version ↔ milestone mapping is fixed per §4; a version shall not ship before its milestone gate passes (REQ-MS-001). |
| REQ-REL-007 | v1.0 ships only when: M10 gate passes; the charter §9.1 v1.0 items hold (Tiers A+B frozen, AVX-512 + dispatch complete, ledger ≥5 µarchs); the freeze audit ([18 M10](18-milestones.md)) is clean; zero disabled tests; regression policy clean vs v0.6. |
| REQ-REL-008 | Post-1.0 deprecation: `[[deprecated]]` + one full minor version coexistence + migration notes ([04 §7](04-public-api.md)); removal only at a major version. |

## 3. ADR-024 — Release and branching strategy

- **Status:** Accepted. **Context:** solo maintainer (OA §10), boring-on-purpose (T8), charter SemVer commitment. **Problem:** choose branching and release mechanics that keep every mainline state releasable with one maintainer. **Constraints:** milestone-gated releases (REQ-MS-001); charter §7.5 SemVer rules; no long-lived branch divergence is tolerable at a bus factor of one. **Alternatives:** GitFlow (rejected: ceremony without a team), release trains (rejected: milestone-gated releases fit a contract-driven plan better). **Decision:** trunk-based + milestone-tagged releases + manual publication of CI-built drafts. **Consequences:** every merge to `main` is releasable by construction (master prompt Part 11 repository-evolution rule). **Reconsideration:** multi-maintainer future. **Related:** REQ-REL-001..003.

## 4. Version progression

| Version | Milestone | API maturity | Benchmark/ledger maturity | Known limitations recorded |
|---|---|---|---|---|
| v0.1 | M3 | Tier A + Surfaces B/C, 0.x-fluid | family benches validate; no ledger | scalar only; no SIMD claims |
| v0.2 | M4 | unchanged | AVX2 + autovec-avx2 variants measured | x86-explicit only; no ledger publication |
| v0.3 | M5 | unchanged | **ledger v1, 3 µarchs; verdicts live** | Tier B absent; AVX-512 absent |
| v0.4 | M6 | + Tier B | Tier B grids; qhash64 frozen | AVX-512 absent |
| v0.5 | M7 | unchanged | all four tiers measured | packaging absent |
| v0.6 | M8 | unchanged | pipeline bench | pre-freeze API fluidity ends here in practice |
| v1.0 | M10 | **frozen A–C** | ≥5 µarchs; regression-clean | per release notes |

## 5. Release checklist (fixed)

1. Milestone gate record complete (REQ-MS-002). 2. `ci.yml` + nightly suites green on the release SHA. 3. Regression subset vs previous release on ≥2 registered machines; deltas explained ([11 §9](11-performance-ledger.md)). 4. CHANGELOG + release notes reviewed. 5. Docs site builds; verdict blocks current (REQ-LEDGER-011). 6. `release.yml` artifacts + checksums + attestation verified. 7. Manual publication. 8. Post-release: version bump PR, gate file archived.

## 6. Acceptance criteria / traceability

Each shipped version demonstrably followed §5 (gate files); v1.0 satisfies REQ-REL-007 in full. **Traceability:** Charter §7.4/§7.5/§9.1/§12 → REQ-REL-001..008 → ADR-024 → milestones ([18](18-milestones.md)) → CI (`release.yml`, [13](13-ci-architecture.md)).
