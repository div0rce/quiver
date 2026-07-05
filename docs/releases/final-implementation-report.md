# Quiver — final implementation report

**Date:** 2026-07-05. This report closes the doc-driven implementation pipeline (Literature
Review → Opportunity Analysis → Design Charter → Engineering PRD → Implementation). It records
what shipped, what is honestly deferred, and why — with no invented performance data (Charter T2).

## What Quiver is

A dependency-free C++23 library of ten vectorized analytical kernel families (K1 compare, K2
filter, K3 select, K4 mask algebra, K5 take/dict-decode, K6 reduce, K7 hash, K8 unpack, K9 arith,
K10 guarded arith) behind a runtime-dispatch layer (scalar / AVX2 / NEON / AVX-512), plus a public
cross-ISA performance ledger and a single-file amalgamation for drop-in vendoring. Chosen over
building another execution engine (Opportunity Analysis C2 8.34 vs C1 4.91); the "Engine Test"
guards against scope creep back toward one.

## Milestones and releases

| Milestone | Delivered | Release |
|-----------|-----------|---------|
| M0 | Repo bootstrap: governance, CMake presets, CI skeleton, 26 ADRs, MkDocs, repo-lint | — |
| M1 | Core types, dispatch framework, testkit | v0.1.0 groundwork |
| M2 | CI/quality gates, red-PR discipline | — |
| M3 | Tier A scalar kernels + dispatch tables | **v0.1.0** |
| M4 | Tier A AVX2 backends + differential fuzzing | **v0.2.0** |
| M5 | Tier A NEON + the performance ledger (public launch, charter shrink point) | **v0.3.0** |
| M6 | Tier B (hash, unpack, arith, guarded arith) on scalar+AVX2+NEON | **v0.4.0** |
| M7 | AVX-512 tier across 7 families, Intel-SDE correctness CI | **v0.5.0** |
| M8 | Amalgamation, installable package, examples, `QUIVER_PIN_ISA`, release workflow | **v0.6.0** |
| M9 | Bitmap-vs-selvec representation study (pre-registered) | — (partial) |
| M10 | API-freeze audit | — (v1.0 deferred) |

## Status

- **M0–M8: complete and shipped.** v0.1.0–v0.6.0 tagged; each milestone has a PASS gate record
  under `docs/releases/gates/` and its CI gate set green on the release SHA. The v0.6.0 release
  workflow (`release.yml`) produces the amalgamation pair + source archive + `SHA256SUMS` + a
  build-provenance attestation and opens a draft for manual publication.
- **M9: partial / deferred.** The representation study is pre-registered (question, method,
  decision rule) with a one-ISA indicative slice (apple-m2/NEON); the ≥3-ISA/≥5-µarch
  entry-referenced data and the workshop paper are hardware/external-blocked (gate M9).
- **M10: partial / deferred.** The API-freeze audit is clean (signature equality
  headers↔PRD↔docs; traceability complete; 26 ADRs current; zero `DISABLED_` tests); v1.0
  certification + tag are deferred pending the release-candidate coverage/regression gates
  (gate M10).

## Open deferrals (all hardware/external, none a correctness gap)

- **R-06 / REQ-LEDGER-012 — ≥5-µarch ledger coverage.** Only `apple-m2-mba` is registered. Blocks:
  the AVX-512 performance ledger (M7), the multi-µarch representation study + paper (M9), and the
  v1.0 certification's coverage/regression gates (M10). Every deferral traces here.
- **R-17 — MSVC amalgamation `/arch`-consumer narrowing.** The amalgamation is verified on MSVC
  with the default `/arch` (baseline-safe); a narrowing scheme for consumers who raise `/arch` is a
  documented tier-2 deferral (ADR-018 M8 amendment).
- **R-18 — two tier-2 MSVC test gaps** surfaced by the first MSVC CI (no-`__int128` checked-sum
  fallback; Windows guard-page test harness) — excluded on the MSVC leg with cited reasons, not
  amalgamation-related.
- **Version constant** stays at `0.1.0` by the deliberate tag-driven scheme; the pre-1.0
  constant-vs-tag relationship is a proposed REQ-REL-001 clarification (gate M8 §8).

## What a downstream can do today

Consume via `find_package(Quiver CONFIG)`, `FetchContent`, or the amalgamation drop-in
(`quiver.h` + `quiver.cpp`, one compiler command) — all three CI-verified, including a
`-fno-exceptions` consumer. Pin a build to one ISA tier with `QUIVER_PIN_ISA`. Correctness is
proven on scalar/AVX2/NEON natively and AVX-512 under Intel SDE; the ledger publishes verified
apple-m2 numbers with an honest CV gate.

## To reach v1.0

Register a second (ideally several) benchmark machine spanning ≥5 µarchs (x86 AVX2/AVX-512 +
Graviton-class ARM). That single unblock closes the M7 ledger, the M9 study + paper, and the M10
v1.0 certification — after which the API surface (already frozen-clean) is tagged v1.0.0.

*Traceability: gates M0–M10; risk register R-06/R-17/R-18; Charter §9.1.*
