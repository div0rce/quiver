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
- **AVX2** (v0.2): emulated-compress compaction. 32-bit lanes: per selection byte, `kCompactLut32` row → `vpermd`, full-vector store at the cursor, advance by popcount — all stores stay inside the n-element capacity region (REQ-MEM-008). 64-bit lanes: per nibble via `kCompactLut64` expanded to `epi32` pair indices. 8/16-bit lanes: BMI2 `PDEP`/`PEXT` byte/word compaction — a documented technique substitution for the PRD's `pshufb` nibble sketch (same output, simpler and exact; recorded in the M4 gate). Note for the ledger: `PEXT` is microcoded on Zen 1/2 (fast from Zen 3) — the per-µarch verdicts must call this out. Exact-alias in-place (`out == in`) stays safe: every block's lanes are fully loaded before its stores. The selvec-driven form delegates to the scalar core (random access). Requires BMI2 — the `avx2` dispatch tier is reported only on AVX2+BMI2 CPUs ([cpu-detection](../internals/cpu-detection.md)).
- **NEON** (v0.3): nibble `TBL` compaction (simdprune lineage, Survey §4.1): per selection nibble (pair for 64-bit lanes) a `kCompactNib*` control row shuffles selected lanes front-packed; exact-width stores keep every write inside both the loaded group (in-place safety, ADR-023) and the n-element capacity region (REQ-MEM-008) — 8-bit lanes use two 4-byte nibble stores per loaded 8-byte group. Selvec-driven filtering delegates to the scalar core.
- **AVX-512 (M7):** lands with its milestone; techniques per PRD [08 §5](../prd/08-kernel-design.md) and [09](../prd/09-simd-architecture.md).

## Ledger

**Verdict (Apple M2, v0.3, `neon` vs `autovec`):** **explicit NEON wins** (geomean 1.72× over 6 published pairs).

| configuration | neon vs autovec | entries |
|---|---|---|
| `i64` n=4096/sel=50/pat=clustered | 1.69× | `qle:apple-m2-20260703-4ec273e2904d-bm-filter-bitmap-neon-i64-n-4096-sel-50-pat-clustered-4096-50-1` `qle:apple-m2-20260703-4ec273e2904d-bm-filter-bitmap-autovec-i64-n-4096-sel-50-pat-clustered-4096-50-1` |
| `i64` n=4096/sel=90/pat=uniform | 1.83× | `qle:apple-m2-20260703-4ec273e2904d-bm-filter-bitmap-neon-i64-n-4096-sel-90-pat-uniform-4096-90-0` `qle:apple-m2-20260703-4ec273e2904d-bm-filter-bitmap-autovec-i64-n-4096-sel-90-pat-uniform-4096-90-0` |
| `i64` n=65536/sel=10/pat=clustered | 1.65× | `qle:apple-m2-20260703-4ec273e2904d-bm-filter-bitmap-neon-i64-n-65536-sel-10-pat-clustered-65536-10-1` `qle:apple-m2-20260703-4ec273e2904d-bm-filter-bitmap-autovec-i64-n-65536-sel-10-pat-clustered-65536-10-1` |
| `i64` n=65536/sel=99/pat=clustered | 1.76× | `qle:apple-m2-20260703-4ec273e2904d-bm-filter-bitmap-neon-i64-n-65536-sel-99-pat-clustered-65536-99-1` `qle:apple-m2-20260703-4ec273e2904d-bm-filter-bitmap-autovec-i64-n-65536-sel-99-pat-clustered-65536-99-1` |
| `i64` n=65536/sel=1/pat=uniform | 1.68× | `qle:apple-m2-20260703-4ec273e2904d-bm-filter-bitmap-neon-i64-n-65536-sel-1-pat-uniform-65536-1-0` `qle:apple-m2-20260703-4ec273e2904d-bm-filter-bitmap-autovec-i64-n-65536-sel-1-pat-uniform-65536-1-0` |
| `i64` n=65536/sel=90/pat=uniform | 1.74× | `qle:apple-m2-20260703-4ec273e2904d-bm-filter-bitmap-neon-i64-n-65536-sel-90-pat-uniform-65536-90-0` `qle:apple-m2-20260703-4ec273e2904d-bm-filter-bitmap-autovec-i64-n-65536-sel-90-pat-uniform-65536-90-0` |

Apple M2 is a **secondary platform** (`secondary_platform`, `no_pmu`: no cycle counters — REQ-LEDGER-008); this is the only registered machine at v0.3 (the three-µarch coverage gate is an open deferral, [gate M5](../releases/gates/M5.md)). Entries flagged `noisy` sit in the 3–5% CV band (REQ-LEDGER-005). Reproduction: [disputes guide](../guides/disputes.md).
## Validation

`tests/unit/test_filter.cpp` · `tests/property/prop_filter.cpp` · `tests/differential/diff_isa_filter.cpp` (backends vs the naive oracle, byte-exact) · invariant + guard-page suites · `bench/micro/bench_filter.cpp` (hypothesis in-source).

---
*Traceability: REQ-K2-*, REQ-KERNEL-*; ADR-006/-016/-023/-025 (+ ADR-013 for K6); PRD 04 §5, 08 §5.*
