# Testkit (MOD-TESTKIT)

Seeded, portable input generation and oracles for every kernel test (PRD [05 §7](../prd/05-internal-architecture.md), [12](../prd/12-testing-architecture.md)). Everything is deterministic from a `uint64` seed and byte-identical across x86-64, ARM64, and macOS — enforced in CI by committed golden FNV-1a hashes (`Testkit.GoldenHashesAreCrossPlatformStable`).

## Components

| File | Contents |
|---|---|
| `tests/testkit/generators.h/.cpp` | SplitMix64 `Rng`; QLM-1 axis implementations (PRD [11 §4](../prd/11-performance-ledger.md)): sequential/uniform/Zipf(θ=1.0, 1000 values) value distributions, uniform/clustered/alternating bitmap patterns, selection-vector derivation, boundary-value and float-special sets, `AlignedBuffer` offset factory |
| `tests/testkit/reference.h` | The *naive second oracle* (dual-oracle scheme, REQ-TEST-002): LSB-first bit accessors, reference popcount, ADR-016 tail checks. Family oracles land with their families (M3/M6) |
| `tests/testkit/assertions.h` | First-divergence diagnostics: index, ±4-element hex context, seed, requirement ID (REQ-TEST-012 format) |
| `tests/testkit/drift_check.cpp` | Conformance runner asserting the bench-local distribution copies are byte-identical (REQ-BENCH-015); intentionally free of GoogleTest *and* Google Benchmark (REQ-TEST-018) |

## Portability rules (REQ-INT-002)

Integer-only core; uniform doubles built as `(next() >> 11) * 2⁻⁵³`; Zipf CDF from harmonic sums (IEEE `+`/`/` only — **no libm anywhere**); geometric run lengths via Bernoulli trials. These rules are what make the golden hashes platform-independent.

## Updating the generator spec

The bench harness maintains an independent copy of the same spec (`bench/harness/distributions.*`, REQ-BENCH-015). A change therefore touches, in one PR: both implementations, the golden hashes (regenerate with `quiver_drift_check --print-golden`), and a CHANGELOG note. `Testkit.BenchDistributionDriftAlarm` (ctest) fails if the copies diverge.

## Reproduction recipe

Every failure message carries the seed and axis values (REQ-TEST-012); re-run with `--gtest_filter=<Test>` — generation is deterministic, so the failure reproduces exactly.
