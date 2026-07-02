# 13 — CI Architecture

## 1. Purpose

Continuous validation topology: workflows, runner matrix, PR gates, nightly jobs, and release automation (MOD-CI). CI verifies; it never measures for the ledger (REQ-LEDGER-007). Upstream authority: Charter §6.3/§9.2; master prompt Part 11 CI gates.

## 2. Requirements

| ID | Requirement |
|---|---|
| REQ-CI-001 | CI shall run on GitHub Actions with three workflow files: `ci.yml` (PR + push to main), `nightly.yml` (scheduled), `release.yml` (tag-triggered). All third-party actions pinned by commit SHA (REQ-SEC-006). |
| REQ-CI-002 | The PR gate set (all blocking unless marked): format check; clang-tidy; build+test matrix (§3); ASan+UBSan job; TSan (dispatch) job; SDE AVX-512 job; examples job; bench-smoke job; docs-build job; repo-lint job (tree manifest REQ-REPO-001, include lint REQ-REPO-005/006/009, `_impl.h` purity REQ-SIMD-006); fuzz-smoke job; MSVC job (**non-blocking**, tier-2). |
| REQ-CI-003 | Build+test matrix: `{ubuntu-24.04: gcc-13, gcc-14, clang-17, clang-19} × {dev, release presets}`; `{ubuntu-24.04-arm: gcc-14, clang-19} × {dev, release}`; `{macos-15: AppleClang} × {dev, release}`. Presets only (REQ-BUILD-011). |
| REQ-CI-004 | The SDE job shall download a cached Intel SDE and run two legs: (a) release preset — unit + differential + invariant suites under `sde64 -spr --` (Sapphire-Rapids profile: F+BW+DQ+VL+VBMI2 exposed) plus `-skx` (VBMI2 absent — exercises the REQ-DISP-011 fallback); (b) Clang ASan+UBSan asserts-on preset under `-spr` — unit + differential (PR: sampled axis tier; nightly: full sweep) — the leg that discharges REQ-SIMD-003's sanitized masked-tail validation. Death tests self-skip under leg (a) per REQ-TEST-014. SDE is a CI-only external tool (ADR-010; risk R-04). |
| REQ-CI-005 | Fuzz smoke: each family target runs ≥ 30 s on PR (batched to fit budget); nightly runs ≥ 4 h total with corpus minimization and artifact upload on crash. |
| REQ-CI-006 | Nightly additionally runs: full differential cross product (REQ-TEST-003), MSan, LSan, coverage artifact (REQ-TEST-013), `-Rpass-missed` vectorization report (REQ-SIMD-009), K7 avalanche suite (REQ-TEST-016). |
| REQ-CI-007 | Bench-smoke: build the bench preset; run every bench binary with a 1-iteration filter; success = validation passes (REQ-BENCH-004) and process exits 0. **No timing thresholds in CI, ever** (Survey §7.3 noise; REQ-BENCH-011). |
| REQ-CI-008 | From M8: amalgamation-verify job (`quiver_amalgamate_verify`, REQ-BUILD-013), three-mode consumption test (REQ-BUILD-010), and a pinned-build consumption leg (`QUIVER_PIN_ISA=avx2` on an x86 runner: build + unit suite + `active_isa()==kAvx2` check — REQ-DISP-013). |
| REQ-CI-009 | `release.yml`: on tag `vX.Y.Z` — full `ci.yml` gate set + nightly suite + amalgamation build → generate SHA-256SUMS + GitHub artifact attestation → draft release with release-notes template ([19 §6](19-release-plan.md)). Publication stays manual (maintainer reviews the draft). |
| REQ-CI-010 | Any red blocking job blocks merge (branch protection); flaky-test policy: a test may be marked `DISABLED_` only with an open issue + regression plan; disabled tests are release blockers ([19 §5](19-release-plan.md)). |
| REQ-CI-011 | CI shall cache FetchContent dependencies and SDE keyed by pinned versions; cache poisoning is mitigated by hash-pinned sources (REQ-BUILD-007). |
| REQ-CI-012 | Total PR wall time target ≤ 25 min (parallel jobs); if exceeded, matrix trimming follows the documented priority (drop gcc-13 first, never drop ASan/SDE/ARM) — a PRD-recorded policy, not an ad-hoc choice. |

## 3. Workflow topology

```text
Title: ci.yml job graph
Purpose: REQ-CI-002 gate visualization

  lint-tier (format, tidy, repo-lint) ──┐
  build-test matrix (x86 / arm / mac) ──┤
  sanitizers (asan-ubsan, tsan) ────────┼──► merge gate (all blocking green)
  sde-avx512 ───────────────────────────┤
  examples / bench-smoke / docs ────────┤
  fuzz-smoke ───────────────────────────┘
  msvc (tier-2) ────────────────────────► informational
```

## 4. ADR-010 — CI topology and SDE-based AVX-512 coverage

- **Status:** Accepted.
- **Context:** GitHub-hosted x86 runners do not guarantee AVX-512 silicon; Charter §6.3 makes AVX-512 a v1.0 tier-1 target; correctness coverage cannot wait for hardware availability.
- **Alternatives:** (1) self-hosted AVX-512 runner — rejected for v1: maintenance + security surface for a solo project (OA §10); (2) skip AVX-512 in CI, test on registered machines only — rejected: violates "every backend tested per PR" discipline; (3) **Intel SDE emulation for correctness** (selected): full instruction coverage incl. mask/compress semantics, deterministic, cacheable; performance testing explicitly out of SDE scope (emulation ≠ timing; ledger machines own performance). GH Actions ARM64 runners cover NEON natively; macOS runners cover AppleClang+NEON.
- **Consequences:** SDE licensing permits internal CI use (verified against Intel's ISDLA at adoption time; risk R-04 tracks changes); SDE runtime ~5–10× — differential matrix under SDE uses the PR-sampled tier, full sweep nightly.
- **Reconsideration:** when AVX-512 GH runners or a trusted self-hosted box become available.
- **Related:** REQ-CI-004, REQ-TEST-017, REQ-DISP-011.

## 5. Failure modes

Runner-image drift (compiler minor bumps): pinned tool versions installed explicitly, not taken from image defaults where gates depend on them (clang-format/clang-tidy exact versions — REQ-STD-002). SDE download failure: cached copy; job retries; persistent failure blocks (by design — coverage is mandatory). ARM runner unavailability: risk R-05 (fallback: QEMU user-mode emulation job, correctness-only, documented as temporary).

## 6. Acceptance criteria

All REQ-CI jobs exist and enforce their gates from the milestone that introduces them ([18](18-milestones.md)); a deliberately broken PR (each gate class violated once) is demonstrated blocked during M2 gate review; PR wall time within REQ-CI-012 target.

## 7. Traceability

Charter §9.2, T5 (no ISA gap in shipped tiers → SDE necessity) → REQ-CI-001..012 → ADR-010 → workflows (MOD-CI) → all milestones (gates). Survey authority: §7.3 (CI-noise prohibition on timing gates).
