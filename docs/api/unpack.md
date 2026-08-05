# API reference — `quiver/unpack.h` (K8)

**Purpose.** K8 — bit-unpacking with frame-of-reference (FOR) fusion: expands `bit_width`-wide packed values into full-width integers, optionally adding a `base` in the same pass (PFOR lineage, Survey §1.4). Roofline class: **bandwidth-bound at large n** (PRD [08 §4](../prd/08-kernel-design.md)) — throughput tracks the *packed* byte stream, so narrower widths yield more values per second. Introduced: v0.4. Stability: 0.x-fluid until the v1.0 freeze.

All functions inherit the common contract: `noexcept`, allocation-free, borrowed views only, thread-compatible pure functions; contract violations assert in debug builds and are UB in release; in-contract inputs are memory-safe and sanitizer-clean (PRD [04 §2](../prd/04-public-api.md), [16](../prd/16-error-handling.md)).

## Contract

| API | Semantics |
|---|---|
| `unpack<Out>(packed, n, bit_width, out)` | value *i* = bits `[i·w, (i+1)·w)` of `packed`, LSB-first little-endian (ADR-026); writes exactly `n` elements |
| `unpack_for<Out>(packed, n, bit_width, base, out)` | FOR fusion: `out[i] = base + value_i` (wrapping unsigned add) |

`Out` ∈ {`u8`,`u16`,`u32`,`u64`} with `0 ≤ bit_width ≤ 8·sizeof(Out)`. `bit_width == 0` means every value is 0 (plain) or `base` (FOR) and `packed` may be null (REQ-K8-003).

**Security surface.** `packed` is the library's one *untrusted-input* argument (REQ-SEC-004): the kernels read **exactly** `⌈n·w/8⌉` bytes and never beyond — enforced by guard-page tests at every width and prioritized by a dedicated raw-byte fuzz target (REQ-K8-002).

**Layout (ADR-026, frozen).** LSB-first little-endian contiguous: bit *j* of the logical stream lives at byte `j/8`, bit position `j%8`. Parquet-RLE-bit-packing-compatible and byte-order-independent by definition. Alternatives (MSB-first, FastLanes 32-value interleaving) were rejected for v1 — see the ADR in PRD [08](../prd/08-kernel-design.md).

Capacity contracts: PRD [06 §6](../prd/06-memory-model.md). Aliasing rules: [ADR-023](../adr/ADR-023-aliasing-contract.md) (`out` never aliases `packed`).

## Scalar reference (the specification)

The family's semantics are defined by `src/kernels/unpack/unpack_scalar_impl.h` (Charter T3): a direct per-value transcription of the ADR-026 sentence — `gather_bits` collects each value's bit range with no shift-buffer state. (A refilling shift-buffer was rejected during implementation: refill shifts lose bits for `w ∈ {58..63}`; the direct form is the oracle precisely because it cannot exhibit that class of bug.)

## Per-ISA notes

- **scalar** (v0.4): direct `gather_bits` loop; byte-aligned widths (8/16/32/64) reduce to widening copies the auto-vectorizer handles well.
- **AVX2** (v0.4): byte-aligned widths {8, 16, 32, 64} use vector widening loads (`vpmovzx*`); sub-byte and non-byte-aligned widths delegate to the scalar core — the general shift-network unpacker is a documented follow-up, so for those widths `avx2` ≡ scalar reference and the ledger row says what that costs. The exact `⌈n·w/8⌉` read bound holds on every path (REQ-K8-002).
- **NEON** (v0.4; sub-byte vectorized post-v0.6): byte-aligned widths {8, 16, 32, 64} via `vmovl` widening chains. Sub-byte widths w ∈ [1,7] use a vectorized `ushl`+mask expansion, 8 values per iteration: 8·w bits = w bytes, so every block re-aligns to a byte boundary and reads exactly its own w bytes (the exact `⌈n·w/8⌉` bound holds, REQ-K8-002). Promoted from the evaluated candidate on quiet-machine ledger evidence (6.9×–11.0× over the scalar gather, table below; REQ-KERNEL-007); building with `-DQUIVER_K8_SUBBYTE_VECTOR=0` reverts sub-byte to the scalar core. Irregular widths (e.g. w=24) still delegate to the scalar core.
- **AVX-512** (v0.5): byte-aligned widths {8, 16, 32, 64} use 512-bit widening loads (`_mm512_cvtepu*`), reading exactly the value bytes; sub-byte and irregular widths delegate to the scalar core (same posture as AVX2). Base set F+BW+DQ+VL only (correct on SDE `-skx`). Exact `⌈n·w/8⌉` read bound on every path; validated (incl. width-exhaustive differential + guard-page) under Intel SDE (ADR-010).

## Ledger

**Verdict (Apple M2, `neon` vs `autovec`):** **every measured width now wins.** Byte-aligned widths take the SIMD widening-load fast path and win by **12.2×–42.4×** over the generic scalar gather the autovectorizer produces. Sub-byte widths, which delegated to the scalar core through v0.6 and sat at ~1.07–1.10× (the v0.4 rows below, preserved as the pre-promotion baseline), now use the vectorized `ushl` expansion and win **6.9×–11.0×** (the post-v0.6 rows; the promotion measurement, quiet machine, every CV under 0.9%). The irregular w=24 still delegates and stays near parity. This is the headline evidence for the `bit_width` axis.

| width | neon vs autovec | path | entries |
|---|---|---|---|
| **w=1** | **10.97×** (was 1.09× delegated) | vectorized sub-byte (post-v0.6) | `qle:apple-m2-20260710-220d2e0236b8-b-bm-unpack-unpack-for-neon-u32-n-65536-w-1-65536-1` `qle:apple-m2-20260710-220d2e0236b8-b-bm-unpack-unpack-for-autovec-u32-n-65536-w-1-65536-1` |
| **w=4** | **6.93×** (was 1.09× delegated) | vectorized sub-byte (post-v0.6) | `qle:apple-m2-20260710-220d2e0236b8-b-bm-unpack-unpack-for-neon-u32-n-65536-w-4-65536-4` `qle:apple-m2-20260710-220d2e0236b8-b-bm-unpack-unpack-for-autovec-u32-n-65536-w-4-65536-4` |
| **w=7** | **8.34×** (was 1.07× delegated) | vectorized sub-byte (post-v0.6) | `qle:apple-m2-20260710-220d2e0236b8-bm-unpack-unpack-for-neon-u32-n-65536-w-7-65536-7` `qle:apple-m2-20260710-220d2e0236b8-bm-unpack-unpack-for-autovec-u32-n-65536-w-7-65536-7` |
| **w=8** | **12.83×** (12.15× re-measured) | SIMD widening | `qle:apple-m2-20260704-883c08552f35-c-bm-unpack-unpack-for-neon-u32-n-65536-w-8-65536-8` `qle:apple-m2-20260710-220d2e0236b8-bm-unpack-unpack-for-neon-u32-n-65536-w-8-65536-8` |
| **w=16** | **19.87×** (18.49× re-measured) | SIMD widening | `qle:apple-m2-20260704-883c08552f35-d-bm-unpack-unpack-for-neon-u32-n-65536-w-16-65536-16` `qle:apple-m2-20260710-220d2e0236b8-bm-unpack-unpack-for-neon-u32-n-65536-w-16-65536-16` |
| w=24 | 1.10× | delegated (3-byte) | `qle:apple-m2-20260704-883c08552f35-d-bm-unpack-unpack-for-neon-u32-n-65536-w-24-65536-24` `qle:apple-m2-20260704-883c08552f35-d-bm-unpack-unpack-for-autovec-u32-n-65536-w-24-65536-24` |
| **w=32** | **42.42×** | SIMD widening | `qle:apple-m2-20260704-883c08552f35-d-bm-unpack-unpack-for-neon-u32-n-65536-w-32-65536-32` `qle:apple-m2-20260704-883c08552f35-d-bm-unpack-unpack-for-autovec-u32-n-65536-w-32-65536-32` |

v0.4 pre-promotion sub-byte baseline (preserved, not deleted): `qle:apple-m2-20260704-883c08552f35-c-bm-unpack-unpack-for-neon-u32-n-65536-w-1-65536-1` `qle:apple-m2-20260704-883c08552f35-c-bm-unpack-unpack-for-neon-u32-n-65536-w-4-65536-4` `qle:apple-m2-20260704-883c08552f35-c-bm-unpack-unpack-for-neon-u32-n-65536-w-7-65536-7`. Those rows quantified the then-open follow-up (both sides ran the same scalar gather); the w=8/16 re-measured controls in the promotion run match the v0.4 numbers, which is what makes the two sessions comparable. Apple M2 is a **secondary platform** (`no_pmu`, secondary; one of two registered machines — `intel-i9-9900k` (Coffee Lake AVX2) also has committed runs; broader coverage remains open, [hardware coverage plan](../benchmarks/hardware-coverage-plan.md)). Reproduction: [disputes guide](../guides/disputes.md).

## Validation

`tests/unit/test_unpack.cpp` (hand-derived layout cases + guard-page exact-bound sweep at every width) · `tests/property/prop_unpack.cpp` (pack/unpack round-trip) · `tests/differential/diff_isa_unpack.cpp` (width-exhaustive 0..8·sizeof(Out), REQ-K8-004) · `tests/fuzz/fuzz_unpack.cpp` (raw-byte untrusted-input target, REQ-SEC-004) · `bench/micro/bench_unpack.cpp` (bit_width axis; hypothesis in-source).

---
*Traceability: REQ-K8-001..004, REQ-SEC-004, REQ-KERNEL-*; ADR-006/-016/-023/-026; PRD 04 §5, 08 §5.*
