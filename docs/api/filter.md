# API reference — `quiver/filter.h` (K2)

**Purpose.** K2 — dense order-preserving compaction. Roofline class: **Bandwidth-bound** (PRD [08 §4](../prd/08-kernel-design.md)). Introduced: v0.1 (scalar backend). Stability: 0.x-fluid until the v1.0 freeze.

All functions inherit the common contract: `noexcept`, allocation-free, borrowed views only, thread-compatible pure functions; contract violations assert in debug builds and are UB in release; in-contract inputs are memory-safe and sanitizer-clean (PRD [04 §2](../prd/04-public-api.md), [16](../prd/16-error-handling.md)).

## Contract

| API | Semantics |
|---|---|
| `filter(in, selection_bitmap, out)` | defined output = first *count* = popcount elements; capacity region `n`; → count |
| `filter(in, sel, out)` | selvec-driven; writes exactly `sel.len` elements in O(sel.len) |

Exact aliasing `out == in.data` is permitted (forward-scan invariant, ADR-023) — in-place compaction is a first-class pattern. `selection` must be non-null.

Capacity contracts: PRD [06 §6](../prd/06-memory-model.md). Aliasing rules: [ADR-023](../adr/ADR-023-aliasing-contract.md).

## Scalar reference (the specification)

The family's semantics are defined by `src/kernels/filter/filter_scalar_impl.h` (Charter T3) — readable, intrinsic-free, and the oracle every backend must match bit-for-bit.

## Per-ISA notes

- **scalar** (v0.1): Unconditional-store forward scan (scratch stays inside the capacity region, REQ-MEM-008). AVX-512's `vpcompress` showcase lands at M7 (Survey §4.1).
- **AVX2 (M4) / NEON (M5) / AVX-512 (M7):** land with their milestones; techniques per PRD [08 §5](../prd/08-kernel-design.md) and [09](../prd/09-simd-architecture.md).

## Ledger

*Pending v0.3* — the first ledger publication (three microarchitectures) lands at M5 with the explicit-vs-autovec verdict block (wins **and** losses, REQ-LEDGER-011). No performance numbers are published without it (Charter T2).

## Validation

`tests/unit/test_filter.cpp` · `tests/property/prop_filter.cpp` · `tests/differential/diff_isa_filter.cpp` (backends vs the naive oracle, byte-exact) · invariant + guard-page suites · `bench/micro/bench_filter.cpp` (hypothesis in-source).

---
*Traceability: REQ-K2-*, REQ-KERNEL-*; ADR-006/-016/-023/-025 (+ ADR-013 for K6); PRD 04 §5, 08 §5.*
