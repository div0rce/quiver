# ADR-010 — CI topology and SDE-based AVX-512 coverage

- **Identifier:** ADR-010
- **Title:** CI topology and SDE-based AVX-512 coverage
- **Status:** Accepted
- **Source:** extracted verbatim from the Engineering PRD, [docs/prd/13-ci-architecture.md](../prd/13-ci-architecture.md) (REQ-DOC-004). From M0 onward this file is the canonical, living ADR record; status changes happen here and in the [index](README.md), with the PRD text remaining the historical record.

---

- **Status:** Accepted.
- **Context:** GitHub-hosted x86 runners do not guarantee AVX-512 silicon; Charter §6.3 makes AVX-512 a v1.0 tier-1 target; correctness coverage cannot wait for hardware availability.
- **Alternatives:** (1) self-hosted AVX-512 runner — rejected for v1: maintenance + security surface for a solo project (OA §10); (2) skip AVX-512 in CI, test on registered machines only — rejected: violates "every backend tested per PR" discipline; (3) **Intel SDE emulation for correctness** (selected): full instruction coverage incl. mask/compress semantics, deterministic, cacheable; performance testing explicitly out of SDE scope (emulation ≠ timing; ledger machines own performance). GH Actions ARM64 runners cover NEON natively; macOS runners cover AppleClang+NEON.
- **Consequences:** SDE licensing permits internal CI use (verified against Intel's ISDLA at adoption time; risk R-04 tracks changes); SDE runtime ~5–10× — differential matrix under SDE uses the PR-sampled tier, full sweep nightly.
- **Reconsideration:** when AVX-512 GH runners or a trusted self-hosted box become available.
- **Related:** REQ-CI-004, REQ-TEST-017, REQ-DISP-011.
