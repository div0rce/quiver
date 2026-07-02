# 20 — Risk Register

## 1. Purpose

Engineering risks with likelihood (L), impact (I) on a low/med/high scale, mitigations, and monitoring triggers. Product-level risks live in Charter §10; this register covers implementation-phase risks. Owner for all rows: maintainer (solo project; OA §10). Review cadence: every milestone gate (gate file records changes).

| ID | Risk | L | I | Mitigation (built into this PRD) | Trigger / monitor |
|---|---|---|---|---|---|
| R-01 | Target-pragma miscompilation or quality regression on a tier-1 compiler (ADR-003) | med | high | two GCC + two Clang versions in CI (REQ-CI-003); differential matrix catches wrong-code; `QUIVER_DISABLE_AVX512` escape (REQ-BUILD-006) | new compiler-version job red; ledger delta per compiler |
| R-02 | Epoch-based dispatch overhead measurable on small batches (ADR-004) | low | med | `bench_dispatch` from M3; ADR-004 reconsideration threshold (>1% at 4K elements) | bench_dispatch ledger rows |
| R-03 | AVX2/NEON 64-bit-multiply emulation makes K7 explicit paths lose to autovec/scalar | med | low | that outcome is publishable by design (T7, REQ-LEDGER-011); K7 NEON evidence gate (REQ-KERNEL-007) | M6 ledger verdicts |
| R-04 | Intel SDE availability/licensing change breaks AVX-512 CI (ADR-010) | low | high | SDE cached (REQ-CI-011); fallback: QEMU-x86 AVX-512 emulation job or self-hosted runner (documented contingency) | SDE download job failures |
| R-05 | GitHub ARM64 runner availability/regression | low | high | macOS ARM runners as NEON backstop; QEMU user-mode correctness fallback ([13 §5](13-ci-architecture.md)) | ARM job queue times |
| R-06 | No AVX-512 registered machine for ledger rows at M7 | med | med | documented contingency: AVX-512 ledger rows may land in M9 (gate records slip; [18 M7](18-milestones.md)) | machine registry state at M6 |
| R-07 | Guard-page suite flakiness across OSes (mmap/mprotect semantics) | low | med | testkit isolates platform code; Windows leg is tier-2 | CI flake tracking (REQ-CI-010 policy) |
| R-08 | LUT footprint (≤16 KiB) evicts hot data in composed pipelines | low | low | budget capped (REQ-SIMD-005); pipeline bench observes composition effects (REQ-BENCH-012) | bench_pipeline vs micro deltas |
| R-09 | Bootstrap-CI statistics questioned by paper referees (ADR-020) | med | low | methodology versioned (QLM, REQ-LEDGER-014); Kalibera-Jones upgrade path named in ADR-020 | M9 review feedback |
| R-10 | Google Benchmark API churn breaks pinned integration | low | low | exact pin + hash (REQ-BUILD-007); upgrade is a deliberate PR | dependabot/pin review at milestones |
| R-11 | Amalgamation generator drifts from source conventions | med | med | REQ-STD-006 lint from M0 conventions + `--check` in CI (M8); verify target runs full unit suite (REQ-BUILD-013) | amalgamation-verify job |
| R-12 | Solo-maintainer bus factor stalls milestones | med | high | strict-order milestones each leave a releasable state (REQ-MS-001); shrink point pre-authorized at M5 (Charter §9.3) | schedule vs charter horizons |
| R-13 | MSVC tier-2 drift accumulates unfixable debt | med | low | tier-2 failures file issues (REQ [03 §7](03-build-system.md)); no tier-1 promises made | msvc job history |
| R-14 | Avalanche suite gives false confidence vs full SMHasher | low | med | documented as SMHasher-subset (ADR-012); one full SMHasher run recorded in family doc at M6 | M6 gate |
| R-15 | Ledger repo growth (raw JSON) beyond comfortable clone size | low | low | monitored; git-lfs contingency (ADR-021) | repo size at gates |
| R-16 | Charter/PRD conflict discovered mid-implementation | low | high | stop-and-report rule (master prompt Part 12; [00 §1](00-executive-summary.md)); amendment process defined (Charter §0, PRD [README](README.md)) | any stop-report |

## 2. Traceability

Charter §10 (product risks) → this register (engineering risks) → mitigations are existing REQs/ADRs (cited per row) → reviewed at every gate (REQ-MS-002).
