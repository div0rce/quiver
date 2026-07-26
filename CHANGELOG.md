# Changelog

All notable changes to Quiver are documented here. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and Quiver adheres to [Semantic Versioning](https://semver.org/) (0.x rules per Charter §7.5: breaking changes permitted with a minor bump and an entry here).

## [Unreleased]

### Fixed

- **`reduce_sum_checked` reported overflow for representable sums on toolchains without
  `__int128`** (MSVC; risk R-18). The fallback used a *sticky per-add* overflow flag, but
  API-K6-003 specifies "true iff mathematically unrepresentable (exact, 128-bit accumulation)"
  — a property of the **final** sum. `[INT64_MAX, 1, -1]` overflows transiently yet sums to
  `INT64_MAX`, so tier-1 returned `false` and MSVC returned `true`: a silent cross-platform
  result divergence in a library that promises bit-identical integer results. Replaced with
  exact 128-bit `(hi, lo)` limb accumulation, differentially verified bit-identical to
  `__int128` over 800k randomized adversarial sequences (int64 and uint64).
- **The Windows guard-page test harness never allocated anything.** `GuardedAlloc`'s `_WIN32`
  branch discarded its arguments and set `payload_ = nullptr` despite a comment claiming a
  `VirtualAlloc` guard, so every guard-page test took an access violation during setup.
  Implemented with `VirtualAlloc(MEM_RESERVE|MEM_COMMIT)` + `VirtualProtect(PAGE_NOACCESS)`,
  mirroring the POSIX `mmap`/`mprotect` leg (REQ-TEST-006, REQ-MEM-008).
- `tests/property/prop_reduce.cpp` used `__int128` unguarded, which MSVC rejects on every
  architecture (error C4235) — the whole property suite failed to compile on Windows.

### Changed

- **Windows CI now builds the full test suite and runs it with no `--gtest_filter` exclusions**
  (127/127 under MSVC 14.44 / VS 2022 17.14). Previously only the amalgamated unit target was
  built, with two cases filtered out. Risk R-18 is closed. MSVC remains labelled toolchain
  tier-2: that is now a Charter §8.1 governance gate ("promote on demonstrated demand"), not a
  technical gap.

### Added

- **Convenience surface** (ADR-027, PRD 04 §3.6) — zero-cost spellings over the unchanged
  primitives, from the external API-ergonomics review: `all_valid` (the named
  `BitmapView{nullptr}`), `bitmap_bytes(n)`, `batch_view(range)`/`selection_view(range)` view
  builders, `validity` defaults and no-validity overloads across compare/reduce, range-in /
  span-out overloads for `compare_bitmap`/`compare_selvec`/`take` that check output capacity
  (assertion builds) and return the **written subspan**, and `CheckedSum<T>` + `sum_checked`
  replacing the pointer-out-parameter pattern. Nothing allocates; no kernel behavior changes.

- Narrow-width compare coverage (i8/i16/i32) with a pre-registered hypothesis, confirmed: the
  handwritten NEON pack wins 2.8×/1.6×/1.1× respectively (monotone width gradient; i64 stays
  delegated at parity), so the narrow widths keep their handwritten paths on measured evidence
  (#35).
- Dense-min benchmark coverage (i64/i32/f64, nulls axis), pre-registered (#35).

### Changed

- **Breaking (0.x rules, REQ-API-009): `Sma<T>` → `MinMaxSummary<T>`, `compute_sma` →
  `compute_min_max`** (ADR-027). The old name read as "simple moving average", which the
  operation never was — it is the fused single-pass min/max/null-count summary. `[[deprecated]]`
  aliases keep old code compiling through 0.x and are removed at v1.0.
- On aarch64, **integer dense min/max/SMA delegates to the autovectorized scalar reference**: the
  measured handwritten single-accumulator chain lost 0.26×–0.27× (integer min is
  associative-exact, so the compiler builds a multi-accumulator loop), and the delegated path
  confirms parity — a ~3.8× user-visible gain on dense integer min. Floats keep the handwritten
  path (measured 4.0× win; the compiler cannot reassociate float min/max past NaN semantics)
  (#35).

### Fixed

- R-19 closed in the risk register and status docs after the v0.7.0 release run live-verified the
  publish path.

## [0.7.0] — 2026-07-11

### Added

- **Vectorized sub-byte NEON unpack** (bit widths 1–7): promoted to production dispatch on
  quiet-machine ledger evidence — 6.9×–11.0× over the scalar gather (u32, every CV under 0.9%),
  with the exact `⌈n·w/8⌉` read bound preserved (REQ-K8-002). Landed first as an experimental
  candidate off dispatch with exhaustive differential, boundary-length, randomized-sweep, and
  guard-page coverage; promoted per the evidence gate (REQ-KERNEL-007). Building with
  `-DQUIVER_K8_SUBBYTE_VECTOR=0` reverts sub-byte widths to the scalar reference (#29, #30).
- **Complete Apple M2 paired benchmark grid** for compare/filter/select/reduce: 53 shapes measured
  neon-vs-autovec on a quiet machine, zero unknowns remain. New findings recorded honestly:
  compare i64 bitmap is parity at all 15 shapes (the delegation verification), filter wins
  1.60×–1.81× at all 20, select wins 5.0×–7.4× at all 10, dense i64 sum is parity while the
  null-masked path wins up to 8.39× (#31); the unpack promotion measurement (#30).
- **Investigations** (Apple M2): NEON losses and dispatch routing (#27), the full performance
  sweep with per-family roofline/parity proofs (#28), and the sub-byte unpack record (#29, #30).
- **M9 (partial): pre-registered bitmap-vs-selvec representation study** — question, method, and
  the available-slice analysis with an open conclusion; completion is hardware-blocked (#21).
- **M10 (partial): API-freeze audit** (clean) and the final implementation report; v1.0
  certification stays deferred under R-06 (#22).

### Changed

- On aarch64, the 64-bit compare `bitmap` forms and the 8-byte elementwise `arith` paths delegate
  to the autovectorized scalar reference — the committed ledger showed the handwritten NEON losing
  (0.69× / 0.90×), the delegated codegen is byte-identical to the measured baseline, and the full
  grid now confirms parity at every registered shape (#27, #31). Results are bit-identical;
  narrower widths and `selvec` keep their handwritten NEON.
- Documentation overhaul: plain-language rewrite with diagrams across the docs tree (#25, #26);
  public repo status aligned with v0.6.0 and the deferred v1.0 (#24).

### Fixed

- **Nightly MSan leg (closes the R-19 release hold):** `-fsanitize=memory` now sits in the MSan
  job's global `CXXFLAGS`/`LDFLAGS`, so CMake's compiler probe links the MSan runtime that the
  instrumented `libc++abi.so` references; the leg had failed at `project()` since it was added.
  With the nightly green, `release.yml` can publish artifacts (R-19's exit criterion). The hold
  itself and the corrected artifact claims were documented in #23.

## [0.6.0] — 2026-07-05

### Added

- M8 — the vendoring and packaging surface (Charter §6.5):
  - Installable CMake package: `install()` exports the headers, the static archive, and a
    `find_package(Quiver CONFIG)` package (`quiver::quiver`), and nothing else (REQ-BUILD-009).
  - Single-file **amalgamation**: `tools/amalgamate/amalgamate.py` generates the drop-in pair
    `quiver.h` + `quiver.cpp` (byte-deterministic, ADR-018); `quiver_amalgamate_verify` compiles
    it and runs the unit suite against it with byte-identical kernel outputs (REQ-BUILD-013).
  - All three consumption modes CI-verified — find_package, FetchContent, amalgamation drop-in —
    including a `-fno-exceptions` consumer (REQ-BUILD-010).
  - Four runnable examples (REQ-INT-006); an end-to-end pipeline benchmark composing
    compare→select→take→reduce (REQ-BENCH-012); vcpkg/Conan packaging skeletons (REQ-BUILD-015).
  - Compile-time ISA pinning `QUIVER_PIN_ISA=scalar|neon|avx2|avx512` (REQ-DISP-013): a build
    that statically resolves every dispatch entry to the pinned tier.
  - A tag-triggered `release.yml` producing the amalgamation pair + source archive with
    `SHA256SUMS` and a GitHub build-provenance attestation (REQ-CI-009, REQ-REL-004).
  - LTO state recorded as a benchmark ledger manifest field (REQ-BUILD-014).

### Changed

- ISA backend TUs use family-unique anonymous-namespace helper names so the amalgamation's
  single translation unit has no collisions (behavior-preserving; REQ-STD-006 amendment).
- `ci.yml`/`nightly.yml` are reusable (`workflow_call`) so a release tag runs the full gate.

### Notes

- ADR-018 amended: the MSVC per-ISA narrowing is unnecessary for the default-`/arch` build
  (empirical finding); `/arch`-consumer narrowing is a tier-2 deferral (R-17). The version
  constant still trails the SemVer tags at 0.1.0 (pre-existing; see gate M8 §8).

## [0.5.0] — 2026-07-04

### Added

- M7 — the AVX-512 ISA tier, with Intel SDE correctness coverage (ADR-010; there is no
  AVX-512 hardware, so this is a correctness release, not a performance one):
  - Native AVX-512 backends for the seven families where a distinct technique wins over AVX2:
    K1 compare (native opmask predicates), K2 filter + K3 select (`vpcompress` to register,
    Zen-4-safe), K4 mask (512-bit), K7 hash (native `vpmullq`), K8 unpack (512-bit widening),
    K9 arith (vertical ops, native `vpmullq`). All use only the base required set F+BW+DQ+VL
    and are correct on the SDE `-skx` profile.
  - AVX-512 dispatch foundation: slot [3] wired incrementally via per-uid markers; the base
    F+BW+DQ+VL target region plus a VBMI2 region for future resolution-time sub-feature
    variants (REQ-DISP-011); a `DispatchAvx512` selectability test.
  - The `sde-avx512` CI job (REQ-CI-004): unit/differential/invariant under `sde64 -spr` and
    `-skx`, plus a sanitized differential leg under `-spr`.
  - A compile-gated, release-excluded detection seam (`QUIVER_TEST_FORCE_ISA`) for local
    dispatch-selection testing where no AVX-512 execution exists.

### Changed

- On AVX-512 hardware (or under SDE) the dispatcher selects the AVX-512 backend for the seven
  native families; K5 take, K6 reduce, and K10 arith_guarded run the AVX2 backend (no
  measurable AVX-512 win without hardware — R-06). Results are bit-identical to scalar.
- Fixed an empty x86 CPU brand string under SDE `-skx` (fall back to a generic brand).

## [0.4.0] — 2026-07-03

### Added

- M6 — Tier B kernel families (K7–K10) with scalar + AVX2 + NEON backends, completing the
  catalog on the first three tiers:
  - **K7 `hash`** — batch `qhash64` with cross-ISA/cross-platform bit-identical output.
    The algorithm and constants are **frozen** (ADR-012) with 256 committed golden vectors
    (+ 5 `combine` vectors) in `tests/golden/qhash64_vectors.txt`; any change is a v2 event. AVX2 decomposes the
    64-bit multiply into three `vpmuludq`; the NEON GPR-vs-vector choice is the
    evidence-gated REQ-KERNEL-007 decision — **measured on Apple M2 (2026-07-03): the GPR
    chain wins 1.32× over the vector decomposition**, so it ships (`kUseVectorHash=false`);
    the losing variant stays compiled and test-covered via `-DQUIVER_K7_HASH_VECTOR=1`
    (`docs/investigations/k7-neon-hash.md`). First-party avalanche/bias gate wired nightly.
  - **K8 `unpack` / `unpack_for`** — bit-unpacking with frame-of-reference fusion; the
    ADR-026 LSB-first layout; reads exactly `⌈n·w/8⌉` bytes from the untrusted `packed`
    input (REQ-SEC-004, guard-page and raw-byte-fuzz tested); byte-aligned widths use SIMD
    widening loads, others delegate to the scalar core.
  - **K9 `arith`** — elementwise wrapping (integer) / IEEE (float) add/sub/mul with the
    validity-composition overload; **no signed-overflow UB path** (narrow operands compute
    in a promotion-safe unsigned type — REQ-K9-001).
  - **K10 `arith_guarded`** — `arith_checked` (wrapped results + exact overflow count +
    optional position bitmap, ADR-014) and `arith_saturating` (exact clamps); 64-bit
    checked/saturating multiply is the documented scalar concession (REQ-K10-003).
- Four Tier B family doc pages (`docs/api/{hash,unpack,arith}.md`) and the K7 investigation
  page; the qhash64 algorithm section with constants, golden-vector count, and the
  SMHasher-subset avalanche note.
- Tier B test suites: unit (boundary/edge matrices, golden reproduction), property
  (avalanche, wrap/checked/saturate laws, K9-002 composition), width-exhaustive and
  NaN-class differential, and raw-byte differential fuzz targets with committed corpora.
- Tier B microbenchmarks with the `bit_width` and `overflow_density` axes and pre-timing
  validation against independent recomputes.
- The first committed Tier B ledger results (Apple M2, secondary platform) with entry-id
  verdict blocks on all three family pages (REQ-LEDGER-011): hash parity; unpack 12.8×–42.4×
  for byte-aligned widths; a published arith loss (~0.90×); and checked-arith wins that are
  flat across overflow density. Wins and losses alike; no numbers invented (one µarch — the
  ≥2-machine coverage gate remains an open deferral).

### Changed

- The umbrella header `quiver/quiver.h` now exposes the Tier B surface
  (`hash.h`, `unpack.h`, `arith.h`).
- Nightly CI now fuzzes nine family targets (≥4 h total) and runs the K7 avalanche gate at
  ≥100k samples/type inside the `QUIVER_NIGHTLY` differential sweep.

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
