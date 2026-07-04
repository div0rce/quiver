# API reference — `quiver/arith.h` (K9 + K10)

**Purpose.** K9 — elementwise wrapping/IEEE arithmetic with validity composition; K10 — overflow-guarded arithmetic that is **never silent UB** (Charter §7.4). Roofline classes: K9 is **bandwidth-bound at large n**; K10-checked is **compute-bound** (PRD [08 §4](../prd/08-kernel-design.md)). Introduced: v0.4. Stability: 0.x-fluid until the v1.0 freeze.

All functions inherit the common contract: `noexcept`, allocation-free, borrowed views only, thread-compatible pure functions; contract violations assert in debug builds and are UB in release; in-contract inputs are memory-safe and sanitizer-clean (PRD [04 §2](../prd/04-public-api.md), [16](../prd/16-error-handling.md)).

## Contract

| API | Semantics |
|---|---|
| `arith(op, a, b, out)` / `arith(op, a, b_scalar, out)` | integers **wrap** (two's-complement/modular — REQ-K9-001, no UB path: unsigned internal arithmetic); floats native IEEE-754 |
| `arith(op, a, b, a_validity, b_validity, out, out_validity)` | **defined as the composition**: values at all lanes (invalid lanes hold defined-behavior garbage the validity bit masks) + `mask_combine(kAnd)` on validities — implemented via K4's public API (the documented cross-family exception, PRD [02 §6](../prd/02-repository-architecture.md)) |
| `arith_checked(op, a, b[, b_scalar], out, overflow_bits) → count` | wrapped results at **all** lanes; bit *i* of the nullable `overflow_bits` (capacity `⌈n/8⌉`, tail-zeroed) set iff lane *i* overflowed; returns the exact overflow count (ADR-014); integers only |
| `arith_saturating(op, a, b[, b_scalar], out)` | clamps exactly at `numeric_limits<T>::min()/max()` (REQ-K10-002); integers only |

`op` ∈ {`kAdd`, `kSub`, `kMul`}. Overflow positions compose with K2/K3 for extraction — filter the batch by `overflow_bits` to retrieve the offending lanes (the ADR-014 design reuses the library's own vocabulary instead of inventing an error type).

**ADR-014 (overflow reporting).** Count + optional position bitmap was selected over a boolean flag (callers needing positions would rerun scalar) and over first-overflow-index (forces an early-exit data-dependent branch into the hot loop). The count accumulates branchlessly — cost is flat in overflow density (the benchmark's `overflow_density` axis is the evidence).

Capacity contracts: PRD [06 §6](../prd/06-memory-model.md). Aliasing: exact aliasing `out == a.data`/`b.data` permitted; `out_validity` may alias an input validity ([ADR-023](../adr/ADR-023-aliasing-contract.md)).

## Scalar reference (the specification)

K9's semantics are defined by `src/kernels/arith/arith_scalar_impl.h` (wrapping via unsigned internal arithmetic — no signed-overflow UB anywhere, PRD [15 §3](../prd/15-security-and-ub.md)); K10's by `src/kernels/arith_guarded/arith_guarded_scalar_impl.h` (sign-trick overflow detection for add/sub; widening multiply below 64-bit; 64-bit multiply checked via `__int128` where available with a portable division-based fallback).

## Per-ISA notes

- **scalar** (v0.4): the references above; `INT_MIN` negation/multiplication edge cases are enumerated in the unit boundary matrix (REQ-K10-002).
- **AVX2** (v0.4): K9 add/sub/mul direct per lane width; 64-bit multiply via the same three-`vpmuludq` decomposition as K7 (PRD 08 §5). K10 checked add/sub vectorized with the sign-trick (overflow ⇔ sign(result) differs from sign(a) while signs of a,b agree — branch-free lane masks feeding a popcount accumulator); saturating add/sub native (`vpadds*`/`vpsubs*`) for 8/16-bit lanes, compare+blend clamp for 32/64-bit. **All checked/saturating multiplies delegate to the scalar core** — REQ-K10-003 permits this and this page plus the ledger state it plainly: 64-bit checked multiply has no profitable SIMD form on this ISA (the 128-bit high half is scalar-only), and the narrow widths follow the same shape for cross-ISA structural identity.
- **NEON** (v0.4): K9 direct per lane width; 64-bit multiply via `umull`/`umlal` decomposition. K10 checked add/sub sign-trick vectorized; saturating add/sub native (`vqadd`/`vqsub`) at **all** widths — one of NEON's genuine conveniences over AVX2. Checked/saturating multiplies delegate to the scalar core (REQ-K10-003, same rationale as AVX2).
- **AVX-512** (v0.5): **K9 `arith`** runs direct vertical add/sub/mul per lane width; the 64-bit multiply is a native `vpmullq` (`_mm512_mullo_epi64`, AVX-512DQ) instead of AVX2's 3×`vpmuludq`; 8-bit multiply widens even/odd bytes and repacks; floats native IEEE. Base set F+BW+DQ+VL only (correct on SDE `-skx`); bit-identical to scalar, validated (incl. NaN-class) under Intel SDE. **K10 `arith_guarded`** AVX-512 backend lands later in the M7 rollout.

## Ledger

**Verdict (Apple M2, v0.4, `neon` vs `autovec`):**

- **K9 `arith` — a published loss (~0.90×).** For pure elementwise `add`/`mul` the autovectorizer is the ideal case and slightly beats the explicit NEON loop; the family is bandwidth-bound and this is the honest T7 outcome (the bench hypothesis predicted parity/loss territory).
- **K10 `arith_checked` — neon wins 1.42×, and is FLAT across overflow density.** The three `checked_add` rows at 0 / 0.1% / 50% overflow are within 0.4% of each other — the direct measurement of ADR-014's branch-free-accumulation claim (cost does not scale with overflow count).
- **K10 `arith_saturating` — neon wins 1.65×** via native `vqadd` versus the autovectorized compare-and-clamp.

| api | configuration | neon vs autovec | entries |
|---|---|---|---|
| `arith` add | i64 n=65536 | 0.90× (loss) | `qle:apple-m2-20260704-883c08552f35-e-bm-arith-add-neon-i64-n-65536-65536` `qle:apple-m2-20260704-883c08552f35-f-bm-arith-add-autovec-i64-n-65536-65536` |
| `arith` mul | f64 n=65536 | 0.90× (loss) | `qle:apple-m2-20260704-883c08552f35-e-bm-arith-mul-neon-f64-n-65536-65536` `qle:apple-m2-20260704-883c08552f35-f-bm-arith-mul-autovec-f64-n-65536-65536` |
| `arith_checked` add | i64 n=65536 / ovf=0 | 1.42× | `qle:apple-m2-20260704-883c08552f35-b-bm-arith-guarded-checked-add-neon-i64-n-65536-ovf-0-65536-0` `qle:apple-m2-20260704-883c08552f35-b-bm-arith-guarded-checked-add-autovec-i64-n-65536-ovf-0-65536-0` |
| `arith_checked` add | i64 n=65536 / ovf=0.1% | 1.42× | `qle:apple-m2-20260704-883c08552f35-b-bm-arith-guarded-checked-add-neon-i64-n-65536-ovf-1-65536-1` `qle:apple-m2-20260704-883c08552f35-b-bm-arith-guarded-checked-add-autovec-i64-n-65536-ovf-1-65536-1` |
| `arith_checked` add | i64 n=65536 / ovf=50% | 1.42× | `qle:apple-m2-20260704-883c08552f35-b-bm-arith-guarded-checked-add-neon-i64-n-65536-ovf-500-65536-500` `qle:apple-m2-20260704-883c08552f35-b-bm-arith-guarded-checked-add-autovec-i64-n-65536-ovf-500-65536-500` |
| `arith_saturating` add | i64 n=65536 | 1.65× | `qle:apple-m2-20260704-883c08552f35-g-bm-arith-guarded-saturating-add-neon-i64-n-65536-65536` `qle:apple-m2-20260704-883c08552f35-g-bm-arith-guarded-saturating-add-autovec-i64-n-65536-65536` |

The `arith` (K9) `neon` medians come from a 4 s window and the `autovec` medians from an 8 s window — both CV-screened; `ns_per_batch` medians are window-length independent, and the longer autovec window was needed to bring this fanless secondary platform's streaming baseline under the 5% CV policy (REQ-LEDGER-005; several 2 s attempts were excluded). Apple M2 is a **secondary platform** (`no_pmu`, secondary; the only registered machine — ≥2-µarch is an open deferral, [gate M6](../releases/gates/M6.md)). Reproduction: [disputes guide](../guides/disputes.md).

## Validation

`tests/unit/test_arith.cpp` (boundary matrix × 8 integer types incl. `INT_MIN` cases) · `tests/property/prop_arith.cpp` (wrap/checked/saturate laws + K9-002 composition property, REQ-K9-002) · `tests/differential/diff_isa_arith.cpp` (backends vs the wide-math oracles, byte-exact) · `tests/fuzz/fuzz_arith.cpp` (K9+K10 differential) · `bench/micro/bench_arith.cpp` + `bench_arith_guarded.cpp` (overflow_density axis; hypotheses in-source).

---
*Traceability: REQ-K9-001..002, REQ-K10-001..003, REQ-KERNEL-*; ADR-006/-014/-016/-023; PRD 04 §5, 08 §5.*
