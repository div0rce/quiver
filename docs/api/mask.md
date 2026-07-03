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
- **AVX2 (M4) / NEON (M5) / AVX-512 (M7):** land with their milestones; techniques per PRD [08 §5](../prd/08-kernel-design.md) and [09](../prd/09-simd-architecture.md).

## Ledger

*Pending v0.3* — the first ledger publication (three microarchitectures) lands at M5 with the explicit-vs-autovec verdict block (wins **and** losses, REQ-LEDGER-011). No performance numbers are published without it (Charter T2).

## Validation

`tests/unit/test_mask.cpp` · `tests/property/prop_mask.cpp` · `tests/differential/diff_isa_mask.cpp` (backends vs the naive oracle, byte-exact) · invariant + guard-page suites · `bench/micro/bench_mask.cpp` (hypothesis in-source).

---
*Traceability: REQ-K4-*, REQ-KERNEL-*; ADR-006/-016/-023/-025 (+ ADR-013 for K6); PRD 04 §5, 08 §5.*
