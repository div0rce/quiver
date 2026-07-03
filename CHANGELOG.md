# Changelog

All notable changes to Quiver are documented here. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and Quiver adheres to [Semantic Versioning](https://semver.org/) (0.x rules per Charter §7.5: breaking changes permitted with a minor bump and an entry here).

## [Unreleased]

### Added

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
