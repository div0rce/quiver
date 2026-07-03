# API reference — `quiver/select.h` (K3)

**Purpose.** K3 — lossless conversion between the two selection representations. Roofline class: **Mixed / selectivity-shaped** (PRD [08 §4](../prd/08-kernel-design.md)). Introduced: v0.1 (scalar backend). Stability: 0.x-fluid until the v1.0 freeze.

All functions inherit the common contract: `noexcept`, allocation-free, borrowed views only, thread-compatible pure functions; contract violations assert in debug builds and are UB in release; in-contract inputs are memory-safe and sanitizer-clean (PRD [04 §2](../prd/04-public-api.md), [16](../prd/16-error-handling.md)).

## Contract

| API | Semantics |
|---|---|
| `bitmap_to_selvec(selection, n, out_idx)` | → count = popcount; strictly increasing; capacity region `n` |
| `selvec_to_bitmap(sel, n, out_bits)` | set bits exactly `sel`; everything else (incl. tails) zero |

Round-trips are identities in both directions (property-tested). Counts agree exactly with `mask_popcount`.

Capacity contracts: PRD [06 §6](../prd/06-memory-model.md). Aliasing rules: [ADR-023](../adr/ADR-023-aliasing-contract.md).

## Scalar reference (the specification)

The family's semantics are defined by `src/kernels/select/select_scalar_impl.h` (Charter T3) — readable, intrinsic-free, and the oracle every backend must match bit-for-bit.

## Per-ISA notes

- **scalar** (v0.1): The primary instrument of the M9 bitmap-vs-selvec representation study (Charter §6.2; Survey §11.3 #3).
- **AVX2** (v0.2): `bitmap_to_selvec` uses the emulated-compress core directly — each selection byte's `kCompactLut32` row *is* its compacted lane-index list; add a broadcast base, store 8 lanes, advance the cursor by popcount (full-vector stores stay inside the n-element capacity region, REQ-MEM-008). `selvec_to_bitmap` stays on the scalar core (sorted scatter is scalar-dominant by design, PRD 08 K3).
- **NEON** (v0.3): same emulated-compress core as AVX2 — per selection byte the `kCompactLut32` row is the compacted index list; add broadcast base, two 128-bit stores, advance by popcount. `selvec_to_bitmap` stays scalar (sorted scatter).
- **AVX-512 (M7):** lands with its milestone; techniques per PRD [08 §5](../prd/08-kernel-design.md) and [09](../prd/09-simd-architecture.md).

## Ledger

*Pending v0.3* — the first ledger publication (three microarchitectures) lands at M5 with the explicit-vs-autovec verdict block (wins **and** losses, REQ-LEDGER-011). No performance numbers are published without it (Charter T2).

## Validation

`tests/unit/test_select.cpp` · `tests/property/prop_select.cpp` · `tests/differential/diff_isa_select.cpp` (backends vs the naive oracle, byte-exact) · invariant + guard-page suites · `bench/micro/bench_select.cpp` (hypothesis in-source).

---
*Traceability: REQ-K3-*, REQ-KERNEL-*; ADR-006/-016/-023/-025 (+ ADR-013 for K6); PRD 04 §5, 08 §5.*
