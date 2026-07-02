# 01 — Traceability

## 1. Purpose

The traceability backbone: charter-decision → PRD mapping, the charter handoff ledger, the complete ADR index, and the master requirement matrix. Where this chapter and a body chapter disagree on a requirement's *allocation* (milestone), this chapter's matrix is authoritative; where they disagree on *content*, the body chapter is authoritative and the disagreement is a defect (REQ-META-004).

## 2. Process requirements

| ID | Requirement |
|---|---|
| REQ-META-001 | Requirement IDs are globally unique, stable, and never reused; new REQs append to their area's sequence via PRD amendment. |
| REQ-META-002 | Every requirement shall map to at least one validation artifact (test, lint, CI job, benchmark, audit, or gate review) in §6. |
| REQ-META-003 | Every requirement belongs to exactly one milestone (the first-enforced convention of [18 §1](18-milestones.md)); later milestones maintain it. |
| REQ-META-004 | On discovering a conflict between PRD chapters, or between the PRD and an upstream document, implementation stops and files a structured report ([00 §1](00-executive-summary.md)); resolution is a versioned amendment, never silent divergence. |

## 3. Charter-bound decisions → PRD implementation map

Every item the Charter §13 lists as **bound** and where this PRD implements it:

| Charter-bound item | PRD implementation |
|---|---|
| Name, namespace (`quiver`) | REQ-API-001, ADR-007; naming-diligence gate at M5 launch ([18 M5](18-milestones.md)) |
| Two-product form (library + ledger) | Chapters [02](02-repository-architecture.md)–[09](09-simd-architecture.md) (library) + [10](10-benchmark-architecture.md)/[11](11-performance-ledger.md) (ledger); MOD-LEDGER co-equal module |
| Personas / anti-personas | No PRD surface addresses an anti-persona need (checked in [22 §1](22-final-review-checklist.md)); P1–P4 needs traced in [04](04-public-api.md)/[14](14-documentation.md) |
| Eight tenets T1–T8 | T1→Engine Test enforcement (REQ-BENCH-012 demo-layer bounds, [21](21-future-work.md)); T2→REQ-LEDGER-004/009/015; T3→REQ-KERNEL-001, REQ-TEST-002, docs template §3; T4→REQ-BUILD-003/010, ADR-002/018; T5→REQ-CI-003 ARM legs, M5 ordering; T6→REQ-API-003, REQ-MEM-*; T7→REQ-LEDGER-011, ADR-011; T8→ADR-008/019/024 selections, REQ-STD-* |
| Ten-family closed catalog, tiers | [08](08-kernel-design.md); catalog closure restated [00 §5](00-executive-summary.md); REQ-STD-010 (no new API without amendment) |
| Type coverage, Arrow bitmaps, u32 selvecs, dual representation | REQ-API-004, REQ-MEM-006/007, ADR-025; K3 + M9 study |
| ISA matrix, SVE2 non-blocking | REQ-DISP-*, REQ-SIMD-010, [09](09-simd-architecture.md); ADR-010 (AVX-512 CI) |
| Ledger content/axes/statistics/publish-losses | REQ-LEDGER-001..015, ADR-020/021; axes table [11 §4](11-performance-ledger.md) |
| Four public surfaces + memory/execution/determinism contracts | [04](04-public-api.md)/[06](06-memory-model.md)/[16](16-error-handling.md); REQ-API-*, REQ-MEM-*, REQ-ERR-* |
| Apache-2.0, DCO | M0 files; REQ-DOC-009, REQ-STD-009 |
| C++23 floor | REQ-STD-001, REQ-BUILD-001 |
| Deferred/permanent non-goals | [21](21-future-work.md) (deferred only); permanent-never table respected — verified [22 §14](22-final-review-checklist.md) |
| Success scorecard, pivot gates | [19 §4](19-release-plan.md), [18](18-milestones.md) horizons; shrink point at M5 |

## 4. Charter §13 handoff items → owning chapters

| # | Handoff item | Chapter(s) | Key ADRs |
|---|---|---|---|
| 1 | Physical architecture, file/namespace layout, per-ISA TU organization | [02](02-repository-architecture.md), [09](09-simd-architecture.md) | ADR-001/002/003 |
| 2 | Dispatch mechanism + compile-time pinning | [07](07-runtime-dispatch.md), [03](03-build-system.md) | ADR-004/005 |
| 3 | Signatures, view types, tail conventions | [04](04-public-api.md), [06](06-memory-model.md), [09 §4](09-simd-architecture.md) | ADR-006/007/015/016/023 |
| 4 | Hash algorithm | [08 K7](08-kernel-design.md) | ADR-012 |
| 5 | Testing architecture (golden, fuzzing, sanitizers, CI topology) | [12](12-testing-architecture.md), [13](13-ci-architecture.md) | ADR-009/010 |
| 6 | Benchmark harness, statistics, PMU, manifests, ledger schema | [10](10-benchmark-architecture.md), [11](11-performance-ledger.md) | ADR-008/011/020/021/022 |
| 7 | Toolchain matrix + flag policy | [03 §7](03-build-system.md), [17](17-coding-standards.md) | ADR-002 |
| 8 | Milestone plan (OA M1–M15 mapping, shrink point) | [18](18-milestones.md) | ADR-024 |
| 9 | Documentation tooling and site | [14](14-documentation.md) | ADR-019 |
| 10 | Repository bootstrap (layout, CONTRIBUTING, templates, release process) | [02](02-repository-architecture.md), [14](14-documentation.md), [19](19-release-plan.md) | ADR-001/024 |

Also resolved from the charter's explicit deferrals: K10 overflow-report granularity (ADR-014, charter Appendix A); `QUIVER_ISA` env behavior (REQ-DISP-005).

## 5. ADR index

| ADR | Title | Chapter | Status |
|---|---|---|---|
| ADR-001 | Repository layout and module boundaries | [02 §10](02-repository-architecture.md) | Accepted |
| ADR-002 | Compiled static library + generated amalgamation | [03 §5](03-build-system.md) | Accepted |
| ADR-003 | Per-ISA TUs with target-region macros | [09 §3](09-simd-architecture.md) | Accepted |
| ADR-004 | Lazy atomic dispatch with policy epoch | [07 §9](07-runtime-dispatch.md) | Accepted |
| ADR-005 | First-party CPU feature detection | [07 §9](07-runtime-dispatch.md) | Accepted |
| ADR-006 | Public API style (views + façade over concrete symbols) | [04 §8](04-public-api.md) | Accepted |
| ADR-007 | Inline-namespace ABI versioning | [04 §9](04-public-api.md) | Accepted |
| ADR-008 | Google Benchmark + first-party ledger runner | [10 §4](10-benchmark-architecture.md) | Accepted |
| ADR-009 | Testing stack (GoogleTest + first-party generators + libFuzzer) | [12 §8](12-testing-architecture.md) | Accepted |
| ADR-010 | CI topology; SDE-based AVX-512 coverage | [13 §4](13-ci-architecture.md) | Accepted |
| ADR-011 | Equal-ISA auto-vectorized baselines | [10 §5](10-benchmark-architecture.md) | Accepted |
| ADR-012 | qhash64 v1 algorithm | [08 K7](08-kernel-design.md) | Accepted |
| ADR-013 | Float reduction reassociation policy | [08 §3](08-kernel-design.md) | Accepted |
| ADR-014 | K10 overflow reporting (count + optional bitmap) | [08 K10](08-kernel-design.md) | Accepted |
| ADR-015 | Tail handling policy (scalar tails / AVX-512 masking) | [09 §4](09-simd-architecture.md) | Accepted |
| ADR-016 | Bitmap tail zeroing and output determinism | [06 §5](06-memory-model.md) | Accepted |
| ADR-017 | Contract-based error model | [16 §4](16-error-handling.md) | Accepted |
| ADR-018 | Amalgamation generation strategy | [03 §6](03-build-system.md) | Accepted |
| ADR-019 | Documentation toolchain (MkDocs + hand-written reference) | [14 §4](14-documentation.md) | Accepted |
| ADR-020 | Statistics implementation (percentile bootstrap) | [11 §5](11-performance-ledger.md) | Accepted |
| ADR-021 | Ledger data model and storage | [11 §6](11-performance-ledger.md) | Accepted |
| ADR-022 | First-party PMU wrapper (`perf_event_open`) | [10 §6](10-benchmark-architecture.md) | Accepted |
| ADR-023 | Aliasing contract (exact-alias allowlist) | [06 §7](06-memory-model.md) | Accepted |
| ADR-024 | Release and branching strategy | [19 §3](19-release-plan.md) | Accepted |
| ADR-025 | Selection-vector semantics and enforcement | [08 §6](08-kernel-design.md) | Accepted |
| ADR-026 | Bit-packing layout (LSB-first, Parquet-compatible) | [08 K8](08-kernel-design.md) | Accepted |

Materialized as standalone files at M0 (REQ-DOC-004).

**Coverage waiver (master prompt Part 10 minimum ADR list):** three mandated ADR areas carry no dedicated ADR by explicit waiver — *memory ownership* (fixed by Charter T6/§7.2; realized as REQ-API-003/REQ-MEM-009 with no engineering alternative available to the PRD), *allocator strategy* (allocators are a charter §8.2 **permanent non-goal**; REQ-MEM-003 is the whole decision), and *threading model* (fixed by Charter T1/§7.3 — no threads, pure functions; the only synchronization design is dispatch, which has ADR-004). Recording these as ADRs would restate charter law without alternatives; the waiver itself is the documented decision.

## 6. Master requirement traceability matrix

Conventions: ranged rows group requirements whose columns are identical — each ID appears in exactly one row. **Release** is derived from milestone per the fixed mapping ([19 §4](19-release-plan.md)): M0–M3→v0.1, M4→v0.2, M5→v0.3, M6→v0.4, M7→v0.5, M8→v0.6, M9/M10→v1.0. Validation entries name the artifact class; full definitions in [12](12-testing-architecture.md)/[13](13-ci-architecture.md).

| Requirement(s) | Chapter | Module(s) | ADR(s) | Validation | Documentation | Milestone |
|---|---|---|---|---|---|---|
| REQ-META-001..004 | 01 | process | — | gate reviews (REQ-MS-002) | this chapter | M0 |
| REQ-REPO-001 | 02 | MOD-CI | ADR-001 | tree-manifest job | module-map.md | M0 |
| REQ-REPO-008, -012 | 02 | all / docs | ADR-001 | M0 gate review | module-map, docs READMEs | M0 |
| REQ-REPO-005, -006, -009 | 02 | MOD-CI | ADR-001 | include-lint job | module-map.md | M1 |
| REQ-REPO-003, -004 | 02 | MOD-K1..K10 | ADR-001 | file-inventory + `_impl.h` purity lints | module-map.md | M3 |
| REQ-REPO-010 | 02 | process | — | milestone gate order | gates/ | M3 |
| REQ-REPO-002, -007, -011 | 02 | build / MOD-CI | ADR-002 | install-tree + amalg-content + release-workflow checks | vendoring.md | M8 |
| REQ-BUILD-011 | 03 | build | — | presets exist; CI uses presets only | building.md | M0 |
| REQ-BUILD-001..006, -008, -012 | 03 | build | ADR-002 | CI preset build matrix + sanitizer jobs | building.md | M1 |
| REQ-BUILD-007 | 03 | build | ADR-008/009 | hash-pinned fetch verification | building.md | M1 |
| REQ-BUILD-009, -010, -013 | 03 | build / MOD-AMALG | ADR-002/018 | consumption tests ×3 + amalgamation-verify | vendoring.md | M8 |
| REQ-BUILD-014, -015 | 03 | build | — | manifest LTO field; packaging skeleton presence | methodology, vendoring | M8 |
| REQ-API-001..005 | 04 | MOD-CORE/DISPATCH | ADR-006/007 | test_core, test_dispatch (concept accept/reject, noexcept static asserts) | api/core.md, api/dispatch.md | M1 |
| REQ-API-007 | 04 | MOD-CORE | ADR-006 | header/docs audit + review | api pages | M1 |
| REQ-API-006 | 04 | MOD-K1..K10 | ADR-013/016 | inv_determinism + differential matrix | family pages | M3 |
| REQ-API-008, -010..012 | 04 | MOD-K1..K10 | ADR-023/025 | death tests, capacity/aliasing unit rows, API-coverage audit | family pages | M3 |
| REQ-API-009 | 04 | process | ADR-024 | 0.x CHANGELOG discipline; M10 freeze audit | releases/ | M3 |
| REQ-CORE-001..004 | 05 | MOD-CORE | — | test_core; tidy `constinit` check | architecture/core.md | M1 |
| REQ-INT-001 | 05 | MOD-CPU | ADR-005 | test_dispatch detection rows | internals/cpu-detection.md | M1 |
| REQ-INT-002 | 05 | MOD-TESTKIT | — | testkit determinism self-tests (golden hashes) | testing/testkit.md | M2 |
| REQ-INT-003 | 05 | MOD-BENCH | ADR-008 | harness validation-abort self-test | benchmarks/methodology.md | M2 |
| REQ-INT-004 | 05 | MOD-LEDGER | ADR-020/021 | runner unit tests (stdlib-only import check) | benchmarks/ledger.md | M5 |
| REQ-INT-005 | 05 | MOD-AMALG | ADR-018 | byte-determinism test | vendoring.md | M8 |
| REQ-INT-006 | 05 | MOD-EXAMPLES | — | examples CI job (exceptions-on build) | getting-started.md | M8 |
| REQ-MEM-001 | 06 | MOD-K1..K10 | ADR-015 | guard-page suite (both ends, all residues) | family pages | M3 |
| REQ-MEM-002 | 06 | process | ADR-015 | naming lint (no `_padded` symbols) | — | M3 |
| REQ-MEM-003 | 06 | MOD-K1..K10 | — | inv_noalloc + symbol scan | — | M3 |
| REQ-MEM-004 | 06 | MOD-K1..K10 | — | alignment-offset differential column | family pages | M3 |
| REQ-MEM-005 | 06 | MOD-K1..K10 | ADR-023 | aliasing equivalence tests + death tests | family pages | M3 |
| REQ-MEM-006 | 06 | MOD-K1..K10 | ADR-016 | inv_bitmap_tail | api/core.md | M3 |
| REQ-MEM-007 | 06 | MOD-K1..K10 | ADR-025 | inv_selvec_sorted + death tests | api/core.md | M3 |
| REQ-MEM-008..010 | 06 | MOD-K1..K10 / CORE | — | MSan nightly; review; death test (n cap) | api/core.md | M3 |
| REQ-DISP-001..010, -012 | 07 | MOD-DISPATCH | ADR-004/005 | test_dispatch + TSan + bench_dispatch | architecture/dispatch.md | M1 |
| REQ-DISP-011 | 07 | MOD-DISPATCH | ADR-004 | SDE `-spr` vs `-skx` differential | internals/dispatch-state-machine.md | M7 |
| REQ-DISP-013 | 07 | MOD-DISPATCH / build | ADR-004 | pinned-build consumption leg (REQ-CI-008) | architecture/dispatch.md | M8 |
| REQ-KERNEL-001, -002, -004..006, -008 | 08 | MOD-K1..K10 | ADR-006 | unit + differential suites; dead-backend build check | family pages | M3 |
| REQ-KERNEL-003 | 08 | MOD-K1/K2/K3 | — | selectivity-flatness bench evidence | family pages | M3 |
| REQ-KERNEL-007 | 08 | MOD-K5/K7 | — | gate-recorded ledger decisions | family pages | M4 |
| REQ-K1-001..003 | 08 | MOD-K1 | ADR-016/025 | K1 unit/property/differential | api/compare.md | M3 |
| REQ-K2-001..003 | 08 | MOD-K2 | ADR-023 | K2 suites incl. in-place equivalence | api/filter.md | M3 |
| REQ-K3-001..002 | 08 | MOD-K3 | — | round-trip property; popcount agreement | api/select.md | M3 |
| REQ-K4-001..003 | 08 | MOD-K4 | — | truth-table + De Morgan properties | api/mask.md | M3 |
| REQ-K5-001..003 | 08 | MOD-K5 | ADR-025 | duplicate/reverse tests; fused-decode guard-page test | api/take.md | M3 |
| REQ-K5-004 | 08 | MOD-K5 | — | REQ-KERNEL-007 record (AVX2 leg) | api/take.md | M4 |
| REQ-K6-001..005 | 08 | MOD-K6 | ADR-013 | policy oracle; big-int checked-sum reference; SMA composition | api/reduce.md | M3 |
| REQ-K7-001..004 | 08 | MOD-K7 | ADR-012 | golden vectors ×platforms; avalanche suite; differential | api/hash.md | M6 |
| REQ-K8-001..004 | 08 | MOD-K8 | ADR-026 | width-exhaustive differential; guard pages; raw fuzz | api/unpack.md | M6 |
| REQ-K9-001..002 | 08 | MOD-K9 | — | big-int mod-2ⁿ reference; composition property | api/arith.md | M6 |
| REQ-K10-001..003 | 08 | MOD-K10 | ADR-014 | boundary matrix; count≡popcount property | api/arith.md | M6 |
| REQ-SIMD-006 | 09 | MOD-K1..K10 | ADR-011 | purity lint + baseline TU compile | internals/kernel-common.md | M3 |
| REQ-SIMD-009 | 09 | MOD-CI | — | nightly `-Rpass-missed` artifact | — | M3 |
| REQ-SIMD-010 | 09 | process | — | repo scan (no SVE2 sources) | future-work | M0 |
| REQ-SIMD-001..003, -005, -007 | 09 | MOD-K*/KCOMMON | ADR-003/015 | intrinsic-location lint; guard pages per ISA; LUT re-derivation | internals/kernel-common.md | M4 |
| REQ-SIMD-008 | 09 | NEON TUs | — | unroll-evidence bench rows | family pages | M5 |
| REQ-SIMD-004 | 09 | AVX-512 TUs | — | SDE feature-subset runs | internals | M7 |
| REQ-BENCH-001, -002, -004, -005, -007, -008, -013, -015 | 10 | MOD-BENCH | ADR-008/022 | harness self-tests; name lint; PMU sanity | benchmarks/methodology.md, running.md | M2 |
| REQ-BENCH-003, -009 | 10 | MOD-BENCH | — | hypothesis-line extraction check | per-family bench docs | M3 |
| REQ-BENCH-006, -010 | 10 | MOD-BENCH | ADR-011 | forced-variant verification; baseline variants present | methodology | M4 |
| REQ-BENCH-011, -014 | 10 | MOD-BENCH/LEDGER | — | release regression run; flamegraph script smoke | methodology | M5 |
| REQ-BENCH-012 | 10 | MOD-BENCH | — | pipeline bench validation run | — | M8 |
| REQ-LEDGER-001..011, -013..015 | 11 | MOD-LEDGER | ADR-020/021 | schema fixtures; stats golden tests; 3-µarch run; reproduction dry run; verdict-block presence | benchmarks/ledger.md, disputes.md | M5 |
| REQ-LEDGER-012 | 11 | MOD-LEDGER | — | coverage audit (≥3 at M5 progress; ≥5 final) | ledger docs | M10 |
| REQ-TEST-001, -002, -012, -018 | 12 | MOD-TESTKIT | ADR-009 | testkit self-tests; structure lint | testing/ | M2 |
| REQ-TEST-010 | 12 | MOD-CI | — | clang-tidy job | — | M1 |
| REQ-TEST-003, -005, -006, -008, -011, -013, -014 | 12 | tests | — | the suites themselves + nightly full sweep | testing/ | M3 |
| REQ-TEST-004, -007, -009 | 12 | tests | ADR-009 | policy-oracle diff; fuzz targets; full sanitizer matrix | testing/ | M4 |
| REQ-TEST-015, -016 | 12 | K7 tests | ADR-012 | vectors ×platforms; avalanche nightly | api/hash.md | M6 |
| REQ-TEST-017 | 12 | MOD-CI | ADR-010 | SDE suite execution | — | M7 |
| REQ-CI-001 | 13 | MOD-CI | ADR-010 | workflow presence; SHA-pinned actions audit | — | M0 |
| REQ-CI-002, -003, -010..012 | 13 | MOD-CI | ADR-010 | gate-set enforcement demo; matrix presence | — | M1 |
| REQ-CI-007 | 13 | MOD-CI | — | bench-smoke job | — | M2 |
| REQ-CI-006 | 13 | MOD-CI | — | nightly workflow runs | — | M3 |
| REQ-CI-005 | 13 | MOD-CI | — | fuzz-smoke job enforced | — | M4 |
| REQ-CI-004 | 13 | MOD-CI | ADR-010 | SDE job green both profiles | — | M7 |
| REQ-CI-008, -009 | 13 | MOD-CI | — | amalg-verify + consumption jobs; release dry run | — | M8 |
| REQ-DOC-001, -004, -005, -008, -009 | 14 | docs | ADR-019 | strict build; ADR index check; guide presence | — | M0 |
| REQ-DOC-002, -003, -006, -010..012 | 14 | docs | ADR-019 | gate doc reviews; snippet + lexicon lints; release-notes template | — | M3 |
| REQ-DOC-007 | 14 | docs | — | entry-id existence check | — | M5 |
| REQ-SEC-006..008 | 15 | MOD-CI / process | — | pin audits; SECURITY.md presence; permissions blocks | — | M0 |
| REQ-SEC-001..003, -005 | 15 | MOD-K1..K10 | — | sanitizer matrix + guard pages + fuzz corpus | internals/ub-catalog.md | M3 |
| REQ-SEC-004 | 15 | MOD-K8 | ADR-026 | raw-byte fuzzing budget; exact-bound guard tests | api/unpack.md | M6 |
| REQ-ERR-002, -005 | 16 | MOD-CORE | ADR-017 | assert-format death tests; fixed-handler review | — | M1 |
| REQ-ERR-008 | 16 | MOD-TESTKIT/BENCH | — | diagnostic-format self-tests | testing/ | M2 |
| REQ-ERR-001, -003, -004, -006, -007 | 16 | MOD-K1..K10 | ADR-017/014 | death-test rows per precondition; API review | family pages | M3 |
| REQ-STD-002, -007, -009, -010 | 17 | process / MOD-CI | — | format/tidy gates; DCO check; PR template | — | M0 |
| REQ-STD-001, -003..006, -008 | 17 | all code | ADR-018 | tidy checks; header lint; review | internals/coding-standards.md | M1 |
| REQ-MS-001, -002 | 18 | process | ADR-024 | gate records `docs/releases/gates/` | — | M0 |
| REQ-REL-002 | 19 | process | ADR-024 | branch protection config | — | M0 |
| REQ-REL-001, -003, -005, -006 | 19 | process / MOD-CI | ADR-024 | first tagged release executes checklist | releases/ | M3 |
| REQ-REL-004 | 19 | MOD-CI | — | artifact set verification | — | M8 |
| REQ-REL-007, -008 | 19 | process | ADR-024 | v1.0 freeze audit; deprecation policy in docs | releases/ | M10 |

**Count check:** 235 requirement IDs; every ID appears exactly once above (verified by the [22 §9](22-final-review-checklist.md) audit and the review sweep in [REVIEW_REPORT.md](REVIEW_REPORT.md)).

## 7. Research traceability (upstream anchors for major decisions)

| Decision | Survey / OA anchor |
|---|---|
| Branch-free selection kernels; selectivity axes | Survey §3.4 (Ross; Zhou & Ross) |
| Batch/cache-residency framing; batch-size sweep | Survey §1.4, §11.3 #1 |
| Compress-based compaction; Zen 4 store hazard; NEON movemask idioms | Survey §4.1 |
| Gather skepticism → evidence-gated K5 | Survey §4.2 |
| MLP framing for K5/reductions; multi-accumulator policy | Survey §3.9 |
| Equal-ISA autovec baselines; ~10-kernel explicit-SIMD economics | Survey §4.4, §4.5 (ADMS 2023) |
| Benchmark methodology (repetitions, min/median, environment, pitfalls) | Survey §7.1–§7.5 (DBTest 2018; Berger school; LLVM checklist) |
| Runtime cpuid dispatch with conservative baseline | Survey §2.3, §9 #11 |
| Arrow-compatible validity bitmaps | Survey §2.6; Charter §6.2 |
| Vendoring path (amalgamation, Apache-2.0, simdjson precedent) | OA §3, §8, §13 |
| Solo-feasibility constraints (pins, boring choices, graceful shrink) | OA §10; Charter §9.3 |

## 8. Acceptance criteria

§6 covers all 235 IDs exactly once with non-empty validation and milestone columns; §5 indexes all 26 ADRs with valid chapter anchors; §3 covers every charter §13 bound item; [18](18-milestones.md) per-milestone lists reconcile with §6 (audited at every gate, REQ-MS-002).
