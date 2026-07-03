# Changelog

All notable changes to Quiver are documented here. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and Quiver adheres to [Semantic Versioning](https://semver.org/) (0.x rules per Charter §7.5: breaking changes permitted with a minor bump and an entry here).

## [Unreleased]

## [0.3.0] — 2026-07-03

### Added

- M5 — Tier A NEON backends (Charter T5 launch condition: scalar+AVX2+NEON co-equal):
  K1 weight-AND/`vaddv` packed compares with native unsigned orderings; K2 nibble `TBL`
  compaction over new consteval control tables; K3 LUT index-store conversion; K4
  4×-unrolled 128-bit mask algebra; K5 scalar-MLP delegation (gather N/A on NEON,
  evidence gate recorded); K6 vector min/max with exactness rescues, pairwise-widening
  wrap-exact integer sums, and ADR-013 blocked float sums at NEON widths.
- The performance ledger as a product (PRD 11; ADR-020/021): QLS-1 entry/manifest JSON
  schemas; the stdlib-only runner (`ledger/runner/quiver_ledger.py`) with environment
  checklist, shuffled fresh-process repetitions (recorded seeds), seeded
  percentile-bootstrap statistics (B=10,000) with golden tests, CV noise policy,
  append-only results and structural validation wired into ctest; the machine registry
  (one registered machine: Apple M2, secondary platform — the three-µarch coverage gate
  is an open, recorded deferral); the first committed ledger run and
  explicit-vs-autovec verdict blocks on all six family pages (losses included,
  REQ-LEDGER-011); `docs/guides/disputes.md`; a lint check that docs reference ledger
  numbers only by committed `entry_id` (REQ-LEDGER-015).
- Runtime re-derivation tests for all compaction LUTs (closing an untested
  REQ-SIMD-005 clause).

### Changed

- On ARM64 the dispatcher now selects NEON by default; bit-identical to scalar except
  dense float sums (documented ADR-013 blocked policy, f32 {w=4,a=4} / f64 {w=2,a=4}).
- Bench variant naming is platform-dependent per REQ-BENCH-010: on ARM the scalar build
  registers as `autovec` (no `scalar` variant exists there).
- C++20 module scanning disabled in the build (no modules; `clang-scan-deps` is absent
  from minimal environments).

## [0.2.0] — 2026-07-03

### Added

- M4 — Tier A AVX2 backends: explicit AVX2 implementations for all six Tier A families
  (K1 movemask-packed vector compares with sign-bias unsigned orderings; K2/K3
  LUT-driven emulated compress incl. BMI2 `PDEP`/`PEXT` byte/word compaction; K4 256-bit
  mask algebra; K5 with the evidence-gated gather path compiled but defaulting to scalar
  MLP loads; K6 vector min/max with canonical-qNaN and first-zero exactness rescues,
  widen-to-64 wrapping sums, and the ADR-013 A=4 blocked float-sum policy), wired into
  the dispatch AVX2 row on x86-64.
- Differential libFuzzer targets for all six families (contract-valid decoding,
  cross-backend equality under ASan+UBSan) with committed corpora, a `fuzz` CMake
  preset, an enforced CI fuzz-smoke job (>= 30 s/family) and a 4-hour nightly fuzz leg.
- Equal-ISA autovec baselines (`bench/baselines/baseline_avx2.cpp`): the family scalar
  references recompiled under the AVX2 target region into a private namespace, exported
  as `autovec-avx2` benchmark variants for every family microbenchmark.
- Testkit blocked float-sum policy oracle (per-backend `{w, a}` parameterization) and
  ISA-aware float-sum expectations in the differential suite.
- Regression suite (first activation per REQ-TEST-011): `reg_empty_selvec.cpp`.
- Docs: per-ISA notes for all six family pages, `internals/kernel-common.md`,
  `testing/fuzzing.md`, `investigations/k5-gather-avx2.md` (gather decision held by
  prior — no hardware evidence yet, honestly recorded).

### Fixed

- Empty selection vectors (`SelVec{nullptr, 0}`, e.g. from an empty `std::vector`) were
  forwarded as a null pointer into the K5/K6 concrete symbols, whose convention reads
  null as "no selection" — so an empty selection silently processed **all** elements
  (heap overflow in fused `dict_decode`, wrong values from selected reductions). Found
  by the first differential-fuzzing session; façades now disambiguate via a non-null
  empty-selection sentinel.

### Changed

- The `avx2` dispatch tier now requires BMI2 alongside AVX2 (the AVX2 kernels emit
  `PDEP`/`PEXT`); every mainstream AVX2 CPU has BMI2, so this is a correctness guard,
  not a practical exclusion.

## [0.1.0] — 2026-07-03

### Added

- M3 — Tier A scalar kernels: the six Tier A families (K1 compare, K2 filter, K3
  sel_convert, K4 mask_algebra, K5 take/dict_decode, K6 reduce/SMA) as readable scalar
  reference implementations (the semantic specifications, Charter T3) behind 176
  dispatched concrete symbols with typed constinit backend rows; six public facade
  headers with O(1) debug-assert contracts; MOD-KCOMMON (bitmap word helpers,
  consteval compaction LUTs, target-region macros); the dual-oracle test stack
  (unit + property + differential-vs-naive + invariant + guard-page suites — 71
  tests incl. no-allocation and tail-zeroing invariants and the strict-fold float
  oracle); six family benchmarks + dispatch-overhead benchmarks, all
  validate-before-timing; the nightly workflow (full-axis sweep, MSan with
  instrumented libc++, LSan, coverage and vectorization-remark artifacts); six
  family API reference pages and the getting-started guide.

- M2 — Test kit and benchmark harness: seeded portable input generators implementing the
  QLM-1 axes (SplitMix64 core, no libm; golden-hash cross-platform determinism tests),
  naive-oracle and first-divergence-diagnostic testkit headers, the deliberately duplicated
  bench-local distributions with a drift-alarm conformance runner, the Google Benchmark
  harness (validation-before-timing with hard abort, REQ-BENCH-002 naming, forced-variant
  helper), the first-party perf_event_open PMU group wrapper with graceful degrade, run-context
  metadata, the flamegraph collection script, and CI bench-smoke (positive + negative
  validation demo) and fuzz-smoke scaffolding jobs.
- M1 — Core, CPU detection, dispatch framework: Surface B vocabulary types
  (`quiver/core.h`: `Element` concepts, enums, `BatchView`/`BitmapView`/`SelVec`/`Sma`,
  `SumType`), Surface C dispatch/introspection (`quiver/dispatch.h`: `active_isa`,
  `cpu_supports`, ISA override, `warmup`, `version`), first-party CPUID/XGETBV +
  getauxval/sysctl feature detection, the lazy-atomic policy-epoch dispatch framework
  (ADR-004) with an empty kernel registry until M3, the `quiver::quiver` static library
  target, unit/TSan test suites, the CI build/test matrix (2×GCC + 2×Clang on x86-64,
  GCC+Clang on ARM64, AppleClang on macOS; ASan/UBSan/TSan; pinned clang-tidy), and the
  include-graph repository lint.
- M0 — Repository bootstrap: governance files (LICENSE, CONTRIBUTING, SECURITY), documentation skeleton with per-directory ownership, materialized ADR-001…ADR-026 under `docs/adr/`, configure-only CMake skeleton with the full option surface (REQ-BUILD-006) and presets (REQ-BUILD-011), CI skeleton (format / repo-lint / docs-build / configure gates), MkDocs documentation site, and the M0 gate record (`docs/releases/gates/M0.md`).

No kernels exist yet; the first kernel families ship with v0.1 (milestone M3).
