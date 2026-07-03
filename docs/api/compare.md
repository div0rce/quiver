# API reference — `quiver/compare.h` (K1)

**Purpose.** K1 — predicate evaluation into either selection representation. Roofline class: **Mixed / selectivity-shaped** (PRD [08 §4](../prd/08-kernel-design.md)). Introduced: v0.1 (scalar backend). Stability: 0.x-fluid until the v1.0 freeze.

All functions inherit the common contract: `noexcept`, allocation-free, borrowed views only, thread-compatible pure functions; contract violations assert in debug builds and are UB in release; in-contract inputs are memory-safe and sanitizer-clean (PRD [04 §2](../prd/04-public-api.md), [16](../prd/16-error-handling.md)).

## Contract

| API | Semantics |
|---|---|
| `compare_bitmap(op, in, comparand, validity, out_bits)` | bit *i* = valid(i) ∧ pred(i); all `⌈n/8⌉` bytes written, tails zero; → popcount |
| `compare_bitmap(op, a, b, a_validity, b_validity, out_bits)` | two-batch form; valid(i) = a_valid(i) ∧ b_valid(i); requires `a.len == b.len` |
| `compare_between_bitmap(in, lo, hi, validity, out_bits)` | inclusive both ends |
| `compare_selvec(...)` ×3 | strictly increasing defined output (first *count* of capacity region `n`); → count |

Floats use IEEE ordered comparisons: every comparison with NaN is false **except `kNe`**, which is true; `-0.0 == +0.0`. A null `validity` takes a mask-free fast path.

Capacity contracts: PRD [06 §6](../prd/06-memory-model.md). Aliasing rules: [ADR-023](../adr/ADR-023-aliasing-contract.md).

## Scalar reference (the specification)

The family's semantics are defined by `src/kernels/compare/compare_scalar_impl.h` (Charter T3) — readable, intrinsic-free, and the oracle every backend must match bit-for-bit.

## Per-ISA notes

- **scalar** (v0.1): Branch-free byte-assembly core (cost flat in selectivity — the `bench_compare` selectivity sweep is this family's headline evidence; Survey §3.4).
- **AVX2** (v0.2): vector compares packed to predicate bits via width-specific movemask idioms (8-bit `movemask_epi8`; 16-bit BMI2 `PEXT` of the per-lane high-byte bits; 32-bit `movemask_ps`; 64-bit `movemask_pd` nibbles from two vectors). Unsigned orderings sign-bias both operands then compare signed; `ne/le/ge` derive by bit inversion after packing (exact for total-ordered integers); floats use `_mm256_cmp_ps/pd` ordered-quiet predicates directly (`NEQ_UQ` keeps NaN-true `!=`); `between` ANDs the `GE(lo)`/`LE(hi)` lane masks. Validity ANDs at byte granularity; selvec forms feed predicate bytes into the K3 LUT index-store core; tails are scalar and byte-assembled exactly like the reference (ADR-015/ADR-016). Bit-identical to scalar on all forms.
- **NEON** (v0.3): 128-bit vector compares packed to predicate bits per 8-element group (16 for byte lanes) via bit-weight AND + horizontal add (`vaddv`) — NEON has no movemask; the PRD's `shrn` narrowing idiom is the noted alternative to evaluate against ledger data. Unsigned orderings are native (`vcgtq_u*`, no bias trick); floats use native ordered compares with `kNe` as post-pack inversion of `eq` (exactly C++ `!=` NaN semantics); `between` ANDs `GE/LE` lane masks. Selvec forms share the K3 LUT index-store core.
- **AVX-512 (M7):** lands with its milestone; techniques per PRD [08 §5](../prd/08-kernel-design.md) and [09](../prd/09-simd-architecture.md).

## Ledger

**Verdict (Apple M2, v0.3, `neon` vs `autovec`):** **the autovec baseline wins — a published loss** (geomean 0.69× over 3 published pairs).

| configuration | neon vs autovec | entries |
|---|---|---|
| `i64` n=1024/sel=10 | 0.70× | `qle:apple-m2-20260703-4ec273e2904d-bm-compare-bitmap-gt-neon-i64-n-1024-sel-10-1024-10` `qle:apple-m2-20260703-4ec273e2904d-bm-compare-bitmap-gt-autovec-i64-n-1024-sel-10-1024-10` |
| `i64` n=4096/sel=90 | 0.69× | `qle:apple-m2-20260703-4ec273e2904d-bm-compare-bitmap-gt-neon-i64-n-4096-sel-90-4096-90` `qle:apple-m2-20260703-4ec273e2904d-bm-compare-bitmap-gt-autovec-i64-n-4096-sel-90-4096-90` |
| `i64` n=4096/sel=99 | 0.70× | `qle:apple-m2-20260703-4ec273e2904d-bm-compare-bitmap-gt-neon-i64-n-4096-sel-99-4096-99` `qle:apple-m2-20260703-4ec273e2904d-bm-compare-bitmap-gt-autovec-i64-n-4096-sel-99-4096-99` |

Apple M2 is a **secondary platform** (`secondary_platform`, `no_pmu`: no cycle counters — REQ-LEDGER-008); this is the only registered machine at v0.3 (the three-µarch coverage gate is an open deferral, [gate M5](../releases/gates/M5.md)). Entries flagged `noisy` sit in the 3–5% CV band (REQ-LEDGER-005). Reproduction: [disputes guide](../guides/disputes.md).
## Validation

`tests/unit/test_compare.cpp` · `tests/property/prop_compare.cpp` · `tests/differential/diff_isa_compare.cpp` (backends vs the naive oracle, byte-exact) · invariant + guard-page suites · `bench/micro/bench_compare.cpp` (hypothesis in-source).

---
*Traceability: REQ-K1-*, REQ-KERNEL-*; ADR-006/-016/-023/-025 (+ ADR-013 for K6); PRD 04 §5, 08 §5.*
