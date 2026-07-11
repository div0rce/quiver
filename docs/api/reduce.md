# API reference — `quiver/reduce.h` (K6)

**Purpose.** K6 — single-pass reductions and SMA with optional validity and selection. Roofline class: **Mixed (int) / compute-bound (float)** (PRD [08 §4](../prd/08-kernel-design.md)). Introduced: v0.1 (scalar backend). Stability: 0.x-fluid until the v1.0 freeze.

All functions inherit the common contract: `noexcept`, allocation-free, borrowed views only, thread-compatible pure functions; contract violations assert in debug builds and are UB in release; in-contract inputs are memory-safe and sanitizer-clean (PRD [04 §2](../prd/04-public-api.md), [16](../prd/16-error-handling.md)).

## Contract

| API | Semantics |
|---|---|
| `reduce_min/max(in, validity[, sel])` | participation = selected ∧ valid; identities on empty (min→max(), max→lowest()); float NaN → canonical qNaN |
| `reduce_sum_wrap(in, validity[, sel])` | integers: wrapping into `SumType<T>`; floats: the documented **ADR-013 reassociation policy** — the scalar backend is the strict-order recourse (A=1 left fold), SIMD backends use the blocked A=4 accumulator; results are reproducible per (version, ISA, build) but not bit-identical across ISAs |
| `reduce_sum_checked(in, validity[, sel], &sum)` | integers only; → true iff mathematically unrepresentable (exact, 128-bit accumulation); `sum` holds the wrapped value |
| `compute_sma(in, validity[, sel])` | one pass: min + max + null_count (selected-but-invalid) |
| `reduce_count_valid(validity, n[, sel])` | delegates to K4 popcount (the documented cross-family exception) |

Narrow integer types provably cannot overflow their 64-bit accumulators (2³¹ × 2³² < 2⁶³) — checked sums for them always return false. -0.0/+0.0 ties keep the first-encountered value (deterministic).

Capacity contracts: PRD [06 §6](../prd/06-memory-model.md). Aliasing rules: [ADR-023](../adr/ADR-023-aliasing-contract.md).

## Scalar reference (the specification)

The family's semantics are defined by `src/kernels/reduce/reduce_scalar_impl.h` (Charter T3) — readable, intrinsic-free, and the oracle every backend must match bit-for-bit.

## Per-ISA notes

- **scalar** (v0.1): SIMD variants (M4+) use the ADR-013 blocked policy (A=4 vectors) and will quantify the reassociation win against this strict scalar baseline honestly.
- **AVX2** (v0.2): *min/max* use native vector min/max with validity lane-mask expansion plus two exactness rescues that make lane blocking bit-identical to the scalar fold: any valid NaN yields the canonical qNaN (PRD 08 §3.3), and a result equal to 0.0 triggers a rescan for the first participating zero (±0.0 is the only bit-visible float tie). *Integer sums* widen elements to 64-bit lanes before wrap-adding (wrapping is defined at `SumType` width; commutative, so blocking is exact). *Float sums* follow ADR-013 with A=4 vector accumulators (f32 W=8, f64 W=4): masked lanes add `-0.0` (the exact neutral), accumulators combine in the frozen `(0+2),(1+3), then +` order, lanes fold low→high from `+0.0`, and the tail adds sequentially — mirrored exactly by the testkit policy oracle for non-NaN results. **NaN sums compare as a class:** IEEE addition propagates whichever operand's payload the hardware sees first and C++ does not pin FP operand order, so payloads stay deterministic per (version, ISA, build) but are not oracle-reproducible (M4 gate amendment). *Selected shapes* (`SelVec` overloads) delegate to the scalar core — the AVX2 backend's selected float sum is therefore the strict fold (documented policy). *SMA* fuses vector min+max with a validity popcount.
- **NEON** (v0.3): native vector min/max (compare+`bsl` for 64-bit lanes) with the same canonical-qNaN and first-zero exactness rescues as AVX2; integer sums use the PRD's staged pairwise widening (`vpaddlq` chains to 64-bit lanes — no intermediate can overflow, wrap-exact); float sums follow ADR-013 with A=4 accumulators at NEON widths (f32 W=4, f64 W=2), the frozen `(0+2),(1+3), then +` combine, `-0.0` masked neutral, and sequential tails — mirrored by the testkit policy oracle. Selected shapes delegate to the scalar core (strict fold).
- **AVX-512 (M7):** lands with its milestone; techniques per PRD [08 §5](../prd/08-kernel-design.md) and [09](../prd/09-simd-architecture.md).

## Ledger

**Verdict (Apple M2, v0.3, `neon` vs `autovec`):** **explicit NEON wins** (geomean 4.43× over 3 published pairs). **Float-sum caveat (ADR-013, stated per its mandate):** the ~8× f64 wins compare a *reassociated* blocked accumulation (A=4) against a *strict-order* baseline — the compiler cannot reassociate FP without fast-math, which the charter prohibits, so this gap measures the documented reassociation policy, not codegen quality alone.

| configuration | neon vs autovec | entries |
|---|---|---|
| `f64` n=4096/nulls=0 | 8.00× | `qle:apple-m2-20260703-4ec273e2904d-b-bm-reduce-sum-wrap-neon-f64-n-4096-nulls-0-4096` `qle:apple-m2-20260703-4ec273e2904d-bm-reduce-sum-wrap-autovec-f64-n-4096-nulls-0-4096` |
| `f64` n=65536/nulls=0 | 7.84× | `qle:apple-m2-20260703-4ec273e2904d-bm-reduce-sum-wrap-neon-f64-n-65536-nulls-0-65536` `qle:apple-m2-20260703-4ec273e2904d-b-bm-reduce-sum-wrap-autovec-f64-n-65536-nulls-0-65536` |
| `i64` n=4096/nulls=10 | 1.39× | `qle:apple-m2-20260703-4ec273e2904d-bm-reduce-sum-wrap-neon-i64-n-4096-nulls-10-4096-10` `qle:apple-m2-20260703-4ec273e2904d-bm-reduce-sum-wrap-autovec-i64-n-4096-nulls-10-4096-10` |

**Grid-completion update (quiet machine, runs `20260710-989c0f6a88b7-f/g/h/j`):** all 8 registered shapes are now paired. The f64 wins hold (7.7-7.8×, same ADR-013 caveat). Two new findings on the `nulls` axis: **dense `i64` (nulls=0) is parity** at both sizes (`qle:apple-m2-20260710-989c0f6a88b7-h-bm-reduce-sum-wrap-neon-i64-n-65536-nulls-0-65536-0` `qle:apple-m2-20260710-989c0f6a88b7-h-bm-reduce-sum-wrap-autovec-i64-n-65536-nulls-0-65536-0`) — the autovectorizer handles a dense integer sum, so the NEON win is the **null-masked** path — and that win grows with n at high null density: 2.02× at n=4096 but **8.39×** at n=65536/nulls=50 (`qle:apple-m2-20260710-989c0f6a88b7-h-bm-reduce-sum-wrap-neon-i64-n-65536-nulls-50-65536-50` `qle:apple-m2-20260710-989c0f6a88b7-j-bm-reduce-sum-wrap-autovec-i64-n-65536-nulls-50-65536-50`), consistent with the scalar path's data-dependent validity branches: the predictor can learn a 4096-element mask repeated across iterations but not a 65536-element one, while the branchless NEON path is immune to the pattern.

Apple M2 is a **secondary platform** (`secondary_platform`, `no_pmu`: no cycle counters — REQ-LEDGER-008); this is the only registered machine at v0.3 (the three-µarch coverage gate is an open deferral, [gate M5](../releases/gates/M5.md)). Entries flagged `noisy` sit in the 3–5% CV band (REQ-LEDGER-005). Reproduction: [disputes guide](../guides/disputes.md).
## Validation

`tests/unit/test_reduce.cpp` · `tests/property/prop_reduce.cpp` · `tests/differential/diff_isa_reduce.cpp` (backends vs the naive oracle, byte-exact) · invariant + guard-page suites · `bench/micro/bench_reduce.cpp` (hypothesis in-source).

---
*Traceability: REQ-K6-*, REQ-KERNEL-*; ADR-006/-016/-023/-025 (+ ADR-013 for K6); PRD 04 §5, 08 §5.*
