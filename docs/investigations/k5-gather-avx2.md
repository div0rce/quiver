# Investigation — K5 gather vs scalar loads on AVX2 (REQ-K5-004, REQ-KERNEL-007)

**Status: OPEN — decision held by prior, no measurements yet.** This page contains **no
performance numbers**: none have been measured on registered hardware (Charter T2 — no
invented data).

## Question

Should the AVX2 `take` backend use hardware gather (`vpgatherdd`/`vpgatherdq`/`vpgatherps`/
`vpgatherpd`) or the scalar 4×-unrolled independent-load path?

## Current state (v0.2, gate M4)

- Both paths are **compiled** in `src/kernels/take/take_avx2.cpp` behind the
  `kUseGatherTake` constexpr switch (PRD 08 §5 K5 requires both to exist).
- The shipped default is **scalar unrolled loads**, held by the Survey §4.2 prior: on
  Haswell through Zen 3, gather is microcoded or issue-limited and does not beat scalar
  independent loads for cache-resident batches; the OoO core already extracts the MLP.
- The evidence-gated measurement could **not** be run at M4: the only machine available to
  the project is an Apple M-series (ARM64) host, and x86 emulation timing is not valid
  evidence. Recorded as a deferral in `docs/releases/gates/M4.md`.

## Domain constraint discovered during implementation

`vpgatherd*` sign-extends its 32-bit indices, so the gather path is only correct while every
`idx < 2^31`. The scalar default has no such limit (indices are `uint32_t` up to
`values.len`). Any future flip to gather must either keep a guard for large-index batches or
restrict the gather path to `values.len ≤ 2^31`. Debug-asserted in the gather path.

## Measurement protocol (the first x86 machine, `intel-i9-9900k`, is now registered; this experiment has NOT run — it needs the `-DQUIVER_K5_TAKE_GATHER` toggle and a dedicated dict-size sweep)

1. Machine registered per REQ-LEDGER-013 (pinned frequency governor, isolated core set).
2. `bench_take` dict-size sweep (L1/L2/L3/DRAM-resident) × `{i32, i64, f32, f64}` ×
   index patterns (sequential, random, clustered), gather build vs default build
   (`-DQUIVER_K5_TAKE_GATHER` toggle to be added with the experiment).
3. Decision rule (pre-registered): flip the default only if gather wins by ≥ 10% geomean
   across the dict-size sweep on ≥ 2 distinct µarchs, with no regression > 5% on any swept
   point; otherwise keep scalar and record the verdict in the ledger.

## Reopening criteria

Registered access to any of: Intel Skylake-SP/Ice Lake+ (gather improved), AMD Zen 4+, or
any µarch where the ledger shows K5 far below its MLP roofline.

---
*Traceability: REQ-K5-004, REQ-KERNEL-007; Survey §4.2; gate M4 deferral.*
