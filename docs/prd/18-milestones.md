# 18 — Milestones

## 1. Purpose and conventions

The deterministic execution plan: eleven milestones, M0–M10, each leaving the repository releasable (master prompt Part 11). Conventions:

- **REQ allocation:** every requirement is assigned to exactly one milestone — the one where it first becomes *enforced/satisfied*; all later milestones maintain it (regressing an earlier REQ fails the current gate). The full REQ→milestone map is the [01 §6](01-traceability.md) matrix; this chapter lists each milestone's assignments.
- **Files:** "created" lists are exhaustive for production code (REQ-REPO-001/[02 §8](02-repository-architecture.md)); test/bench/doc files follow the named patterns. "Modified" lists name only architecturally meaningful edits.
- **Release gate (uniform base, per master prompt Part 11):** repository builds on all tier-1 targets via presets; all blocking CI jobs green; all tests of this and prior milestones pass; sanitizer matrix green; benchmarks build + validate; docs build strict with this milestone's pages present; traceability updated ([01](01-traceability.md) columns filled); CHANGELOG + release notes drafted where a version ships. Milestone-specific gate items are listed per milestone as **Gate+**.

| ID | Requirement |
|---|---|
| REQ-MS-001 | Milestones shall execute strictly in order M0→M10; no milestone begins before the predecessor's gate passes; no requirement is implemented outside its assigned milestone. |
| REQ-MS-002 | Every gate outcome (pass evidence per item) shall be recorded in `docs/releases/gates/M<n>.md` — the auditable trail of the master prompt's release-gate checklist. |

Correspondence to upstream plans: M0–M10 realize OA §10's M1–M15 skeleton and the Charter §9.1 horizons; **end of M5 is the charter's pre-authorized 9-month shrink point** — a releasable, publicly launchable product state (Charter §9.3).

---

## M0 — Repository bootstrap

- **Objective:** governed, lintable, documented empty repository; all process infrastructure live.
- **Scope:** governance files, docs skeleton + ADR materialization, CI skeleton (lint/docs/repo-lint gates), build skeleton (configure-only), presets. **Excluded:** any production C++.
- **Dependencies:** this PRD accepted.
- **Files created:** `LICENSE`, `README.md`, `CONTRIBUTING.md`, `CHANGELOG.md`, `SECURITY.md`, `.clang-format`, `.clang-tidy`, `.gitignore`, `.gitattributes`, `CMakeLists.txt` (skeleton), `CMakePresets.json`, `cmake/{deps,flags,sanitizers}.cmake`, `cmake/QuiverConfig.cmake.in`, `.github/workflows/ci.yml`, `.github/ISSUE_TEMPLATE/{bug.yml,benchmark-dispute.yml}`, `.github/PULL_REQUEST_TEMPLATE.md`, `docs/mkdocs.yml`, `docs/requirements.txt`, `docs/*/README.md` (every docs dir), `docs/adr/ADR-001…ADR-026 + README.md` (extracted from this PRD), `docs/architecture/module-map.md`, `docs/guides/building.md`, `docs/releases/gates/` (dir).
- **Files modified:** none (initial).
- **REQs implemented:** REQ-REPO-001, -008, -012; REQ-BUILD-011; REQ-DOC-001, -004, -005, -008, -009; REQ-SEC-006..008; REQ-SIMD-010; REQ-STD-002, -007, -009, -010; REQ-CI-001; REQ-REL-002; REQ-MS-001, -002; REQ-META-001..004 ([01 §2](01-traceability.md)).
- **ADRs realized:** ADR-001, ADR-019, ADR-024 (policies in force).
- **Tests required:** none (no code); CI self-test: a deliberately mis-formatted PR is blocked.
- **Benchmarks required:** none.
- **Docs required:** all listed above; mkdocs builds strict.
- **Acceptance criteria:** tree matches [02 §3](02-repository-architecture.md) manifest; ADR index complete (26 entries); docs site deploys locally; DCO + branch protection active.
- **Gate+:** deliberate-violation PR demonstrably blocked (format, docs-link, tree-manifest each once).
- **Risks:** none material.

## M1 — Core, CPU detection, dispatch framework

- **Objective:** MOD-CORE, MOD-CPU, MOD-DISPATCH complete with scalar-only tables; full build/test CI matrix live.
- **Scope:** Surfaces B/C implemented; dispatch entries exist for the (empty-until-M3) kernel inventory via the X-macro list. **Excluded:** kernels.
- **Dependencies:** M0.
- **Files created:** `include/quiver/{quiver.h, core.h, dispatch.h}`, `include/quiver/detail/{config.h, extern_decls.h}`, `src/cpu/{cpu_features.h, cpu_features.cpp}`, `src/dispatch/{dispatch_internal.h, dispatch_tables.cpp, version.cpp}`, `tests/CMakeLists.txt`, `tests/unit/{test_core.cpp, test_dispatch.cpp}`.
- **Files modified:** root `CMakeLists.txt` (library target), `ci.yml` (build/test matrix, sanitizers, TSan).
- **REQs implemented:** REQ-CORE-001..004; REQ-INT-001; REQ-DISP-001..010, -012; REQ-API-001..005, -007; REQ-BUILD-001..008, -012; REQ-CI-002, -003, -010..012; REQ-TEST-010; REQ-STD-001, -003..006, -008; REQ-ERR-002, -005; REQ-REPO-005, -006, -009 (lints enforced from here).
- **ADRs realized:** ADR-002, -004, -005, -006, -007, -017.
- **Tests required:** `test_core` (REQ-CORE rows), `test_dispatch` (env matrix, override round-trip, monotonicity, TSan concurrent-resolution).
- **Benchmarks required:** none yet.
- **Docs required:** `docs/architecture/{core,dispatch}.md`, `docs/internals/{cpu-detection,dispatch-state-machine}.md`, `docs/api/core.md`, `docs/api/dispatch.md`.
- **Acceptance:** matrix + sanitizers green on x86-64, ARM64, macOS; TSan clean; version introspection matches CMake project version.
- **Gate+:** `warmup()`/override behavior demonstrated in `test_dispatch` on all three platforms.
- **Risks:** epoch protocol subtlety → mitigated by TSan + documented memory-order argument ([07 §6](07-runtime-dispatch.md)).

## M2 — Test kit and benchmark harness

- **Objective:** measurement-before-optimization infrastructure (master prompt Part 3): MOD-TESTKIT and MOD-BENCH complete before any kernel exists.
- **Dependencies:** M1.
- **Files created:** `tests/testkit/{generators.h, generators.cpp, reference.h, assertions.h}`, testkit self-tests, `bench/CMakeLists.txt`, `bench/harness/{bench_common.h, distributions.h, distributions.cpp, pmu.h, pmu.cpp, meta.h, meta.cpp, flamegraph.sh}`.
- **Files modified:** `ci.yml` (bench-smoke, fuzz-smoke scaffolding), `cmake/deps.cmake` (GB pin active).
- **REQs implemented:** REQ-INT-002, -003; REQ-TEST-001, -002, -012, -018; REQ-BENCH-001, -002, -004, -005, -007, -008, -013, -015; REQ-CI-007; REQ-ERR-008.
- **ADRs realized:** ADR-008, ADR-022.
- **Tests required:** testkit determinism self-tests (golden byte hashes on all tier-1 platforms); harness self-test (validation-abort path; PMU-absent fallback).
- **Benchmarks required:** harness smoke target proving the GB + PMU + naming pipeline with a placeholder loop.
- **Docs required:** `docs/testing/testkit.md`, `docs/benchmarks/{methodology.md (QLM-1 §-stub), running.md}`.
- **Acceptance:** identical generator hashes across x86/ARM/macOS CI; PMU counters collected on a Linux runner (values sane: cycles > 0, IPC ∈ (0.1, 8)).
- **Gate+:** deliberately-miscompiled validation demo aborts the bench binary (REQ-BENCH-004 evidence).
- **Risks:** distribution-spec drift between testkit and bench → conformance test (REQ-BENCH-015) from day one.

## M3 — Tier A scalar kernels (K1–K6) → **v0.1**

- **Objective:** the six Tier A families, scalar backends, full contract enforcement, complete family test/bench/doc scaffolding.
- **Dependencies:** M2.
- **Files created:** `include/quiver/{compare.h, filter.h, select.h, mask.h, take.h, reduce.h}`; `src/kernels/common/{kernel_common.h, target_regions.h, luts.h, luts.cpp}`; for each family f ∈ {compare, filter, select, mask, take, reduce}: `src/kernels/<f>/{<f>_scalar_impl.h, <f>_scalar.cpp}` (12 files); `tests/unit/test_<f>.cpp` ×6; `tests/property/prop_<f>.cpp` ×6; `tests/differential/diff_isa_<f>.cpp` ×6 (scalar-vs-naive column live; ISA columns activate later); `tests/invariant/{inv_bitmap_tail.cpp, inv_determinism.cpp, inv_selvec_sorted.cpp, inv_noalloc.cpp}`; guard-page suite in testkit; `bench/micro/bench_<f>.cpp` ×6 + `bench_dispatch.cpp`; `tests/regression/README.md`.
- **Files modified:** `detail/extern_decls.h` + `dispatch_tables.cpp` (Tier A inventory), `ci.yml` (nightly.yml created: full-matrix, coverage, `-Rpass-missed`).
- **REQs implemented:** REQ-KERNEL-001..006, -008 (Tier A); REQ-K1-001..003; REQ-K2-001..003; REQ-K3-001..002; REQ-K4-001..003; REQ-K5-001..003; REQ-K6-001..005; REQ-MEM-001..010; REQ-API-006, -008..012; REQ-SIMD-006, -009; REQ-TEST-003 (scalar tier), -005, -006, -008, -011, -013, -014; REQ-ERR-001, -003, -004, -006, -007; REQ-SEC-001..003, -005; REQ-BENCH-003, -009; REQ-DOC-002, -003 (six families), -006, -010..012; REQ-CI-006; REQ-REPO-003, -004, -010; REQ-REL-001, -003, -005, -006 (first tagged release).
- **ADRs realized:** ADR-013, -015 (scalar tails), -016, -023, -025.
- **Tests required:** all listed suites green incl. guard-page + death tests; dual-oracle discipline demonstrated (a seeded scalar-vs-naive sweep per family).
- **Benchmarks required:** six family benches + dispatch bench run with validation; hypothesis lines present.
- **Docs required:** six family pages (template [14 §5](14-documentation.md), ledger section marked "pending v0.3"), `docs/guides/getting-started.md`.
- **Acceptance:** everything green on all tier-1 platforms; `inv_noalloc` proves zero allocation across all Tier A APIs.
- **Gate+:** tag **v0.1**; release notes; gate record `M3.md`.
- **Risks:** scalar/naive oracle disagreements late → mitigated by writing `reference.h` first per family (order mandated).

## M4 — Tier A AVX2 → **v0.2**

- **Objective:** first explicit-SIMD tier; differential + fuzz machinery proving cross-backend equality; equal-ISA baselines live.
- **Dependencies:** M3.
- **Files created:** `src/kernels/<f>/<f>_avx2.cpp` ×6; `tests/fuzz/fuzz_<f>.cpp` ×6 + `corpus/<f>/` seeds; `bench/baselines/baseline_avx2.cpp`.
- **Files modified:** dispatch tables (AVX2 rows), `ci.yml` (fuzz-smoke enforced, ASan/UBSan already on), family docs (per-ISA notes).
- **REQs implemented:** REQ-SIMD-001..003, -005, -007; REQ-TEST-004, -007, -009 (full sanitizer matrix incl. MSan nightly); REQ-BENCH-006, -010; REQ-KERNEL-007 and REQ-K5-004 (K5 gather decision recorded — AVX2 leg); REQ-CI-005.
- **ADRs realized:** ADR-003, ADR-011.
- **Tests required:** differential matrix scalar↔avx2 full L×alignment on PR, sampled axes; guard-page per kernel × AVX2; fuzz targets running differential comparison.
- **Benchmarks required:** avx2 + autovec-avx2 variants for all six families; selectivity-flatness evidence for K1/K2/K3 (REQ-KERNEL-003).
- **Docs required:** per-ISA notes sections ×6; `docs/internals/kernel-common.md`.
- **Acceptance:** bit-exact differential green; fuzz smoke clean; K5 gather-vs-scalar AVX2 evidence recorded on at least one registered x86 machine (pre-ledger format acceptable, archived under investigations/).
- **Gate+:** tag **v0.2**.
- **Risks:** AVX2 unsigned-compare and 64-bit-mul emulation bugs → boundary-value matrix (REQ-TEST-003 axes) is designed to catch exactly these.

## M5 — Tier A NEON + Ledger v1 → **v0.3** (public launch; 9-month shrink point)

- **Objective:** NEON parity (Charter T5 launch condition) and the ledger as a product on ≥3 µarchs.
- **Dependencies:** M4.
- **Files created:** `src/kernels/<f>/<f>_neon.cpp` ×6; `bench/baselines/baseline_neon.cpp` (naming per REQ-BENCH-010); `ledger/schema/{ledger-entry.schema.json, manifest.schema.json}`; `ledger/runner/quiver_ledger.py` (+ modules + unit tests); `ledger/machines/{<zen>.json, <goldencove>.json, <mseries>.json}`; `ledger/results/<3 µarch dirs>/...`.
- **Files modified:** dispatch tables (NEON rows), family docs (verdict blocks + ledger excerpts), `docs/benchmarks/methodology.md` (QLM-1 complete), `nightly.yml` (avalanche placeholder—activates M6).
- **REQs implemented:** REQ-SIMD-008; REQ-INT-004; REQ-LEDGER-001..011, -013..015 (Tier A scope; REQ-LEDGER-012 progresses here, owned by M10); REQ-BENCH-011, -014; REQ-DOC-007. Maintenance: REQ-KERNEL-007 (K5 NEON leg recorded as N/A — NEON has no gather, rationale documented); disputes guide ships as a docs deliverable under REQ-DOC-008 (owned by M0).
- **ADRs realized:** ADR-020, ADR-021.
- **Tests required:** differential scalar↔neon full tier on ARM CI; runner statistics golden tests; schema accept/reject fixtures.
- **Benchmarks required:** full Tier A grid on the three registered machines via the runner (not CI).
- **Docs required:** verdict blocks on all six family pages (wins **and** losses — REQ-LEDGER-011); `docs/guides/disputes.md`; ledger README.
- **Acceptance:** three-µarch ledger committed, schema-valid, CV-policy-conforming; reproduction dry run (fresh clone → matching entry within CI bounds) executed and recorded.
- **Gate+:** tag **v0.3**; public-launch checklist (charter naming diligence confirmed done — Charter §1 gate); **shrink-point note recorded**: repository state certified releasable-as-final per Charter §9.3.
- **Risks:** registered-machine access latency → machines identified during M4; Apple secondary labeling enforced by schema flags.

## M6 — Tier B families (K7–K10), scalar+AVX2+NEON → **v0.4**

- **Objective:** complete the catalog on the first three tiers; freeze qhash64 with golden vectors.
- **Dependencies:** M5.
- **Files created:** `include/quiver/{hash.h, unpack.h, arith.h}`; for f ∈ {hash, unpack, arith, arith_guarded}: `src/kernels/<f>/{<f>_scalar_impl.h, <f>_scalar.cpp, <f>_avx2.cpp, <f>_neon.cpp}` (16 files); `tests/{unit,property,differential,fuzz}` ×4 families; `tests/golden/qhash64_vectors.txt`; `bench/micro/bench_<f>.cpp` ×4; avalanche suite (`tests/property/prop_hash.cpp` section, nightly-wired).
- **Files modified:** extern_decls + dispatch tables (Tier B inventory), baselines (Tier B `_impl.h` inclusion), nightly (avalanche active), family docs ×4 created.
- **REQs implemented:** REQ-K7-001..004; REQ-K8-001..004; REQ-K9-001..002; REQ-K10-001..003; REQ-TEST-015, -016; REQ-SEC-004. Maintenance: REQ-KERNEL-007 (K7 NEON GPR-vs-vector decision recorded with ledger data).
- **ADRs realized:** ADR-012, -014, -026.
- **Tests required:** all four-family suites; K8 width-exhaustive differential; K10 boundary matrix; cross-platform hash vector equality on every CI platform.
- **Benchmarks required:** Tier B grids incl. `overflow_density` and `bit_width` axes; ledger update run for Tier B on ≥2 machines.
- **Docs required:** four family pages complete with verdicts; qhash64 algorithm page section (constants, vectors, quality-gate results, SMHasher-run note).
- **Acceptance:** golden vectors byte-identical on x86/ARM/macOS/SDE; avalanche thresholds met; evidence-gated K7 decision recorded.
- **Gate+:** tag **v0.4**.
- **Risks:** avalanche gate failure → constants/rounds are ADR-012-frozen; failure means an implementation bug, not tuning latitude.

## M7 — AVX-512 across all families → **v0.5**

- **Objective:** the fourth tier everywhere; SDE-based CI coverage; VBMI2 sub-dispatch.
- **Dependencies:** M6.
- **Files created:** `src/kernels/<f>/<f>_avx512.cpp` ×10; `bench/baselines/baseline_avx512.cpp`.
- **Files modified:** dispatch tables (AVX-512 rows + VBMI2 variants), `ci.yml` (SDE job), family docs (AVX-512 notes incl. Zen 4 compress note), ledger runs.
- **REQs implemented:** REQ-SIMD-004; REQ-DISP-011; REQ-CI-004; REQ-TEST-017. Maintenance: REQ-KERNEL-007 (K5 AVX-512 gather leg recorded).
- **ADRs realized:** (ADR-003/015 extended to the masked-tail path — validation, not new decisions).
- **Tests required:** full differential + invariant + guard-page under `sde -spr` and `-skx`; masked-tail guard-page rows.
- **Benchmarks required:** avx512 + autovec-avx512 variants; ledger update on an AVX-512-capable registered machine.
- **Docs required:** AVX-512 sections ×10; dispatch docs updated for sub-feature selection.
- **Acceptance:** SDE suites green both profiles; verdict blocks updated; compress-to-register implementation note verified in code review.
- **Gate+:** tag **v0.5**.
- **Risks:** no AVX-512 registered machine → risk R-06 (acquire/borrow before M7 ends; ledger AVX-512 rows may otherwise slip to M9 — documented contingency, gate records it).

## M8 — Amalgamation, packaging, examples, hardening → **v0.6**

- **Objective:** the vendoring product surface (Charter §6.5), consumption proofs, and dispatch hardening (compile-time pinning, REQ-DISP-013).
- **Dependencies:** M7.
- **Files created:** `tools/amalgamate/{amalgamate.py, test_amalgamate.py}`; `examples/{01_minimal_filter.cpp, 02_filter_take_reduce.cpp, 03_isa_override.cpp, 04_nullable_pipeline.cpp}`; `bench/pipeline/bench_pipeline.cpp`; `cmake/packaging/` (vcpkg port + Conan recipe skeletons); `docs/guides/vendoring.md`.
- **Files modified:** `ci.yml` (amalgamation-verify + three-mode consumption jobs + pinned-build leg), `release.yml` created; `CMakeLists.txt` + `cmake/flags.cmake` (install/export finalized; `QUIVER_PIN_ISA` wiring); `src/dispatch/dispatch_tables.cpp` + `dispatch_internal.h` (static-pin resolution path); `docs/architecture/dispatch.md` (pinning guidance).
- **REQs implemented:** REQ-BUILD-009, -010, -013..015; REQ-DISP-013; REQ-INT-005, -006; REQ-REPO-002, -007, -011; REQ-CI-008, -009; REQ-BENCH-012; REQ-REL-004. Maintenance: REQ-STD-006 (`--check` lint activates); vendoring guide ships under REQ-DOC-008 (owned by M0).
- **ADRs realized:** ADR-018.
- **Tests required:** amalgamate unit tests; amalgamation-verify (full unit suite, byte-identical outputs); consumption tests ×3 modes incl. a `-fno-exceptions` consumer (REQ-ERR acceptance) and the pinned-build leg (REQ-DISP-013).
- **Benchmarks required:** pipeline bench live with validation.
- **Docs required:** vendoring guide; examples wired into docs snippets.
- **Acceptance:** two-file drop-in demo builds with a documented single compiler command; install-tree inspection passes.
- **Gate+:** tag **v0.6**.
- **Risks:** MSVC amalgamation narrowing confusion → explicit vendoring-guide section + tier-2 CI leg.

## M9 — Representation study, ledger expansion, paper

- **Objective:** the M9 charter deliverables: bitmap-vs-selvec study (Charter §6.2/Survey §11.3 #3), ledger ≥5 µarchs, workshop paper draft.
- **Dependencies:** M8 (all tiers + full grids available).
- **Files created:** `docs/benchmarks/investigations/representation-study/` (question, method, entries, analysis, conclusion); ≥2 new `ledger/machines/*.json` + results (Graviton-class + one more); paper draft tracked outside the repo (link recorded in gate file).
- **Files modified:** family pages (study cross-links); possibly `bench/micro/bench_select.cpp` axes (within QLM — else QLM bump per REQ-LEDGER-014).
- **REQs implemented:** none newly owned — the study consumes existing machinery by design. Maintenance: REQ-LEDGER-012 progresses (≥5-µarch leg; owned by M10); any axis change bumps QLM per REQ-LEDGER-014.
- **Tests/benchmarks required:** no new suites; full ledger regeneration on new machines.
- **Docs required:** the study (publication-grade internal document); methodology page updated if QLM bumps.
- **Acceptance:** study answers the question with entry-referenced data across ≥3 ISAs and ≥5 selectivity points; paper draft exists (DaMoN/ADMS target per Charter §9.1).
- **Gate+:** no version tag required; gate file records study + coverage evidence.
- **Risks:** hardware access (R-06); paper timeline external to repo — gate only requires the draft.

## M10 — API freeze and v1.0

- **Objective:** freeze surfaces A–C; certify charter §9.1 v1.0 criteria.
- **Dependencies:** M9.
- **Files created:** `docs/releases/v1.0.0.md`; freeze-audit record in gate file.
- **Files modified:** version metadata; any audit-found doc/header mismatches (docs-vs-headers signature audit, [14 ADR-019](14-documentation.md)).
- **REQs implemented:** REQ-REL-007, -008; REQ-LEDGER-012 (final coverage gate). Maintenance verification: REQ-API-009 freeze in force; REQ-REL-001..006 re-verified in full ([19](19-release-plan.md)).
- **Tests required:** full nightly suite as a release candidate run; zero `DISABLED_` tests (REQ-CI-010).
- **Benchmarks required:** regression subset vs v0.6 on ≥2 machines ([11 §9](11-performance-ledger.md)); no unexplained >3% regressions.
- **Docs required:** freeze audit; complete release notes; charter §9.1 scorecard snapshot.
- **Acceptance:** signature-for-signature equality [04](04-public-api.md) ↔ headers ↔ docs; every ADR status current; traceability matrix complete and bidirectional.
- **Gate+:** tag **v1.0.0**.
- **Risks:** freeze-audit findings → fix-forward inside M10; nothing ships until clean.

## 2. Milestone dependency graph

M0 → M1 → M2 → M3 → M4 → M5 → M6 → M7 → M8 → M9 → M10 (strict chain, REQ-MS-001). Within milestones, family-level work parallelizes per [02 §9](02-repository-architecture.md).

## 3. Traceability

Charter §9 (success criteria, shrink point), OA §10 (M1–M15 skeleton, LOC/testing estimates) → REQ-MS-001/002 + per-milestone REQ lists (complete allocation verified in [01 §6](01-traceability.md)) → releases ([19](19-release-plan.md)).
