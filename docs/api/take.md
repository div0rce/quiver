# API reference — `quiver/take.h` (K5)

**Purpose.** K5 — gather by index; dictionary decode; selection-fused decode. Roofline class: **Latency/MLP-bound** (PRD [08 §4](../prd/08-kernel-design.md)). Introduced: v0.1 (scalar backend). Stability: 0.x-fluid until the v1.0 freeze.

All functions inherit the common contract: `noexcept`, allocation-free, borrowed views only, thread-compatible pure functions; contract violations assert in debug builds and are UB in release; in-contract inputs are memory-safe and sanitizer-clean (PRD [04 §2](../prd/04-public-api.md), [16](../prd/16-error-handling.md)).

## Contract

| API | Semantics |
|---|---|
| `take(values, indices, out)` | arbitrary order + duplicates legal; every index in-bounds (debug-asserted full scan; release UB) |
| `dict_decode(dict, codes, n, out)` | `out[i] = dict[codes[i]]` |
| `dict_decode(dict, codes, n, sel, out)` | fused: packed `out[j] = dict[codes[sel[j]]]`; unselected code positions are **never read** (guard-page proven) |

Codes are `uint8/16/32`. Interpret performance via memory-level parallelism, not IPC — the dict-size sweep crosses cache levels (PRD 08 §4).

Capacity contracts: PRD [06 §6](../prd/06-memory-model.md). Aliasing rules: [ADR-023](../adr/ADR-023-aliasing-contract.md).

## Scalar reference (the specification)

The family's semantics are defined by `src/kernels/take/take_scalar_impl.h` (Charter T3) — readable, intrinsic-free, and the oracle every backend must match bit-for-bit.

## Per-ISA notes

- **scalar** (v0.1): 4×-unrolled independent loads expose MLP (Survey §3.9). The gather-vs-scalar decision is evidence-gated per ISA at M4/M7 (REQ-K5-004; Survey §4.2).
- **AVX2** (v0.2): the gather-vs-scalar question is evidence-gated (REQ-K5-004). Both paths are compiled in `take_avx2.cpp`; the shipped default is the **scalar 4×-unrolled MLP path**, held by the Survey §4.2 prior (gather does not beat scalar independent loads on Haswell–Zen 3 for cache-resident batches). No registered x86 hardware was available at v0.2 to measure, so the decision is provisional-by-prior — protocol and reopening criteria in [investigations/k5-gather-avx2.md](../investigations/k5-gather-avx2.md); the M4 gate records the deferral. The gather path additionally requires `idx < 2^31` (`vpgatherd*` sign-extends 32-bit indices — debug-asserted). `dict_decode` delegates to the scalar core (bounds-check structure must stay identical, REQ-K5-002/-003).
- **NEON (M5) / AVX-512 (M7):** land with their milestones; techniques per PRD [08 §5](../prd/08-kernel-design.md) and [09](../prd/09-simd-architecture.md).

## Ledger

*Pending v0.3* — the first ledger publication (three microarchitectures) lands at M5 with the explicit-vs-autovec verdict block (wins **and** losses, REQ-LEDGER-011). No performance numbers are published without it (Charter T2).

## Validation

`tests/unit/test_take.cpp` · `tests/property/prop_take.cpp` · `tests/differential/diff_isa_take.cpp` (backends vs the naive oracle, byte-exact) · invariant + guard-page suites · `bench/micro/bench_take.cpp` (hypothesis in-source).

---
*Traceability: REQ-K5-*, REQ-KERNEL-*; ADR-006/-016/-023/-025 (+ ADR-013 for K6); PRD 04 §5, 08 §5.*
