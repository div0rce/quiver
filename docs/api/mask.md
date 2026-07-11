# API reference — `quiver/mask.h` (K4)

**Purpose.** K4 — bitmap boolean algebra and cardinality — the null-propagation primitive. Roofline class: **Bandwidth-bound** (PRD [08 §4](../prd/08-kernel-design.md)). Introduced: v0.1 (scalar backend). Stability: 0.x-fluid until the v1.0 freeze.

All functions inherit the common contract: `noexcept`, allocation-free, borrowed views only, thread-compatible pure functions; contract violations assert in debug builds and are UB in release; in-contract inputs are memory-safe and sanitizer-clean (PRD [04 §2](../prd/04-public-api.md), [16](../prd/16-error-handling.md)).

## Contract

| API | Semantics |
|---|---|
| `mask_combine(op, a, b, n, out_bits)` | AND / OR / ANDNOT / XOR over words; tails zeroed |
| `mask_not(a, n, out_bits)` | complement; tails zeroed |
| `mask_popcount(a, n)` | cardinality over the first `n` bits |
| `mask_all/any/none(a, n)` | queries; vacuous truths at n = 0: all=true, any=false, none=true |

All bitmap parameters are **non-null** here — these APIs are the mask operations themselves. Exact aliasing `out_bits == a.bits`/`b.bits` permitted (ADR-023).

Capacity contracts: PRD [06 §6](../prd/06-memory-model.md). Aliasing rules: [ADR-023](../adr/ADR-023-aliasing-contract.md).

## Scalar reference (the specification)

The family's semantics are defined by `src/kernels/mask/mask_scalar_impl.h` (Charter T3) — readable, intrinsic-free, and the oracle every backend must match bit-for-bit.

## Per-ISA notes

- **scalar** (v0.1): 64-bit word loops via `memcpy` (never a misaligned dereference). A designated honest-verdict candidate: auto-vectorization is expected to match explicit SIMD here, and the ledger will say so either way (Charter T7; Survey §4.4).
- **AVX2** (v0.2): `combine`/`not` run 256-bit bitwise ops over the byte stream with a scalar byte tail and tail-bit zeroing (ADR-016). `popcount`/`all`/`any`/`none` intentionally delegate to the scalar word cores — no distinct AVX2 technique exists for them short of AVX-512 `VPOPCNTDQ`, and the honest-verdict expectation is that autovec ties explicit SIMD for this family anyway (Charter T7).
- **NEON** (v0.3): 128-bit `combine`/`not` 4×-unrolled (REQ-SIMD-008: ≥4 independent 128-bit ops in flight for Firestorm's 4×128 pipes) with scalar byte tails and tail-bit zeroing; counting/queries delegate to the scalar word cores (no distinct NEON technique).
- **AVX-512** (v0.5): `combine`/`not` run 512-bit bitwise ops over the byte stream with a scalar byte tail and tail-bit zeroing; counting/queries delegate to the scalar word cores (a `VPOPCNTDQ` variant is a recorded option). Uses only the base required set F+BW+DQ+VL (REQ-SIMD-004) — no VBMI2/VPOPCNTDQ — so it is correct on the SDE `-skx` profile. This is the **first AVX-512 backend**; it lands with the AVX-512 dispatch foundation and SDE correctness gate (ADR-010) — the remaining families follow. Correctness is validated by the differential suite under Intel SDE (`-spr` and `-skx`); local emulators do not reliably execute AVX-512, so the dispatch-selection path is proven separately (`DispatchAvx512` unit test + the `QUIVER_TEST_FORCE_ISA` seam), with kernel output validated in CI under SDE.

## Ledger

**Verdict (Apple M2, v0.3, `neon` vs `autovec`):** **parity** (geomean 0.97× over 3 published pairs).

| configuration | neon vs autovec | entries |
|---|---|---|
| `bitmap` n=4096 | 0.97× | `qle:apple-m2-20260703-4ec273e2904d-bm-mask-and-neon-bitmap-n-4096-4096` `qle:apple-m2-20260703-4ec273e2904d-b-bm-mask-and-autovec-bitmap-n-4096-4096` |
| `bitmap` n=65536 | 0.95× | `qle:apple-m2-20260703-4ec273e2904d-bm-mask-and-neon-bitmap-n-65536-65536` `qle:apple-m2-20260703-4ec273e2904d-b-bm-mask-and-autovec-bitmap-n-65536-65536` |
| `bitmap` n=1048576 | 0.99× | `qle:apple-m2-20260703-4ec273e2904d-b-bm-mask-and-neon-bitmap-n-1048576-1048576` `qle:apple-m2-20260703-4ec273e2904d-b-bm-mask-and-autovec-bitmap-n-1048576-1048576` |

**Queries vectorized (runs `20260711-24a6cf037ae4` before, `20260711-033e3fa4e18f[-b]` after; pre-registered hypothesis in `bench_mask.cpp`):** the transform parity above was the roofline and stands; the *queries* (`all`/`any`/`none`) were a different story. The shipped per-byte early-exit loop does not autovectorize, and on the no-early-exit input class — an all-valid validity bitmap, the ubiquitous "any nulls?" fast path — it measured ~3.2 GB/s (`qle:apple-m2-20260711-24a6cf037ae4-bm-mask-all-neon-bitmap-n-65536-exit-none-65536-0`), an order of magnitude under read bandwidth. The NEON rework (64-byte `vandq`/`vorrq` blocks, one across-lane check per block, early exit preserved at block granularity, scalar suffix delegation) reaches read bandwidth: **30.3× at n=65536** (81 ns, ~101 GB/s: `qle:apple-m2-20260711-033e3fa4e18f-bm-mask-all-neon-bitmap-n-65536-exit-none-65536-0`) and **32.4× at n=1048576** (`qle:apple-m2-20260711-033e3fa4e18f-bm-mask-all-neon-bitmap-n-1048576-exit-none-1048576-0`), while the `exit=first` class is unchanged at ~1.7 ns (`qle:apple-m2-20260711-033e3fa4e18f-bm-mask-all-neon-bitmap-n-65536-exit-first-65536-1` vs baseline `qle:apple-m2-20260711-24a6cf037ae4-bm-mask-all-neon-bitmap-n-65536-exit-first-65536-1`) — both prongs of the pre-registered promotion rule, so the vectorized queries ship (REQ-KERNEL-007). Results stay bit-identical (the suffix and tail delegate to the scalar core; differential-tested through dispatch).

Apple M2 is a **secondary platform** (`secondary_platform`, `no_pmu`: no cycle counters — REQ-LEDGER-008); this is the only registered machine at v0.3 (the three-µarch coverage gate is an open deferral, [gate M5](../releases/gates/M5.md)). Entries flagged `noisy` sit in the 3–5% CV band (REQ-LEDGER-005). Reproduction: [disputes guide](../guides/disputes.md).
## Validation

`tests/unit/test_mask.cpp` · `tests/property/prop_mask.cpp` · `tests/differential/diff_isa_mask.cpp` (backends vs the naive oracle, byte-exact) · invariant + guard-page suites · `bench/micro/bench_mask.cpp` (hypothesis in-source).

---
*Traceability: REQ-K4-*, REQ-KERNEL-*; ADR-006/-016/-023/-025 (+ ADR-013 for K6); PRD 04 §5, 08 §5.*
