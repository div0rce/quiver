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
- **AVX-512** (v0.5): `bitmap_to_selvec` compresses an index iota — opmask from the selection bits, **compress to a register** (`_mm512_maskz_compress_epi32` on `base + [0..15]`), then store advancing by popcount (Zen 4-safe; store within the `[0,n)` capacity region). Base set F+BW+DQ+VL (correct on SDE `-skx`); bit-identical to scalar under Intel SDE. `selvec_to_bitmap` is a scatter with no clean AVX-512 win and falls through to the AVX2 backend.

## Ledger

**Verdict (Apple M2, v0.3, `neon` vs `autovec`):** **explicit NEON wins** (geomean 7.59× over 2 published pairs).

| configuration | neon vs autovec | entries |
|---|---|---|
| `u32` n=4096/density=1 | 7.67× | `qle:apple-m2-20260703-4ec273e2904d-bm-select-bitmap-to-selvec-neon-u32-n-4096-density-1-4096-1` `qle:apple-m2-20260703-4ec273e2904d-bm-select-bitmap-to-selvec-autovec-u32-n-4096-density-1-4096-1` |
| `u32` n=4096/density=90 | 7.50× | `qle:apple-m2-20260703-4ec273e2904d-bm-select-bitmap-to-selvec-neon-u32-n-4096-density-90-4096-90` `qle:apple-m2-20260703-4ec273e2904d-bm-select-bitmap-to-selvec-autovec-u32-n-4096-density-90-4096-90` |

Apple M2 is a **secondary platform** (`secondary_platform`, `no_pmu`: no cycle counters — REQ-LEDGER-008); a second machine — `intel-i9-9900k`, Coffee Lake AVX2 — is registered with committed runs; the three-µarch coverage gap remains open ([hardware coverage plan](../benchmarks/hardware-coverage-plan.md)). Entries flagged `noisy` sit in the 3–5% CV band (REQ-LEDGER-005). Reproduction: [disputes guide](../guides/disputes.md).
## Validation

`tests/unit/test_select.cpp` · `tests/property/prop_select.cpp` · `tests/differential/diff_isa_select.cpp` (backends vs the naive oracle, byte-exact) · invariant + guard-page suites · `bench/micro/bench_select.cpp` (hypothesis in-source).

---
*Traceability: REQ-K3-*, REQ-KERNEL-*; ADR-006/-016/-023/-025 (+ ADR-013 for K6); PRD 04 §5, 08 §5.*
