# API reference — `quiver/hash.h` (K7)

**Purpose.** K7 — batch `qhash64` with cross-ISA/cross-platform **bit-identical** output. Roofline class: **compute-bound** (ILP/port pressure dominate — PRD [08 §4](../prd/08-kernel-design.md)). Introduced: v0.4. Stability: the algorithm and constants are **frozen** ([ADR-012](../prd/08-kernel-design.md)) — stable per major version; any change is a v2 event. Non-cryptographic by contract (Charter §7.4).

All functions inherit the common contract: `noexcept`, allocation-free, borrowed views only, thread-compatible pure functions; contract violations assert in debug builds and are UB in release; in-contract inputs are memory-safe and sanitizer-clean (PRD [04 §2](../prd/04-public-api.md), [16](../prd/16-error-handling.md)).

## Contract

| API | Semantics |
|---|---|
| `hash64<T>(in, seed, out)` | `out[i] = qhash64(in[i], seed)`; writes exactly `in.len` values |
| `hash64_combine(a, b, n, out)` | `out[i] = fmix64(a[i] ^ (b[i] + GOLDEN + (a[i]<<6) + (a[i]>>2)))` — the frozen multi-column combine; exact aliasing `out == a` or `out == b` permitted |

**Frozen definition (ADR-012).** `GOLDEN = 0x9E3779B97F4A7C15`, `C1 = 0xFF51AFD7ED558CCD`, `C2 = 0xC4CEB9FE1A85EC53`; `fmix64(x): x^=x>>33; x*=C1; x^=x>>33; x*=C2; x^=x>>33`; `qhash64(v,seed) = fmix64(key64(v) ^ (seed + GOLDEN))`. `key64` zero-extends the value's bit pattern (i8/i16/i32 sign bits are **not** sign-extended — the two's-complement bits are zero-extended); floats hash by bit pattern with `-0.0` canonicalized to `+0.0` first, NaNs by bit pattern (PRD 08 §3.6).

**No validity parameter (REQ-K7-004), by design.** Null handling is engine policy, not hash policy. Hash every lane, then combine with validity externally — e.g. hash `out`, then zero the hash of invalid lanes or fold validity into a downstream key. The library deliberately does not pick a null-hash convention.

Capacity contracts: PRD [06 §6](../prd/06-memory-model.md). Aliasing rules: [ADR-023](../adr/ADR-023-aliasing-contract.md).

## Scalar reference (the specification)

The family's semantics are defined by `src/kernels/hash/hash_scalar_impl.h` (Charter T3) — readable, intrinsic-free, and the oracle every backend must match bit-for-bit. `hash64` is 4×-unrolled to expose ILP across the independent `fmix64` chains.

## Per-ISA notes

- **scalar** (v0.4): the `fmix64` chain, 4×-unrolled; the frozen output oracle.
- **AVX2** (v0.4): vertical `fmix64` across 4 lanes; the 64×64→low-64 multiply is decomposed into three `vpmuludq` + shifts (the documented standard technique — AVX2 has no 64-bit vector multiply). `combine` delegates to the scalar chain. Bit-identical to scalar.
- **NEON** (v0.4): **evidence-gated (REQ-KERNEL-007), decided at M6.** Both a vector `fmix64` (the 64-bit multiply built from `umull`/`umlal` 32×32 partials) and the reference's unrolled GPR scalar chain are compiled; the shipped default is the **GPR chain** (`kUseVectorHash = false`). *Measured on `apple-m2-mba`, 2026-07-03:* the GPR chain hashes `i64` at **1.54 G/s** vs the vector path's **1.17 G/s** at n=65536 (a **1.32×** GPR advantage; both CV < 4%). Apple Firestorm's two integer-multiply pipes issue native 64-bit `mul` faster than the 3–4-op vector emulation amortizes across two lanes — matching the Survey §3.9/§4.1 prior. Full protocol, both-run data, decision rule, and reopening criteria: [investigations/k7-neon-hash.md](../investigations/k7-neon-hash.md). The losing vector variant stays compiled and is kept differential-/golden-/fuzz-covered via `-DQUIVER_K7_HASH_VECTOR=1`. `combine` uses the GPR chain on both ISAs.
- **AVX-512** (v0.5): vertical `fmix64` over 8 lanes with the 64-bit multiply as a **native `vpmullq`** (`_mm512_mullo_epi64`, AVX-512DQ — in the base required set), so no 3×`vpmuludq` decomposition; key64 widens per type (`vpmovzx*`), floats canonicalize `-0.0`→`+0.0` via an opmask blend; `combine` is the frozen formula over 8 lanes. Base set F+BW+DQ+VL only (correct on SDE `-skx`). Bit-identical to scalar — validated against the committed golden vectors under Intel SDE (the NEON GPR-vs-vector verdict does not transfer; no AVX-512 hardware to benchmark yet — R-06).

## Quality — avalanche & determinism

**Cross-platform stability (REQ-K7-002).** 256 golden vectors covering all 10 element types × representative seeds (plus 5 `combine` rows) are frozen in `tests/golden/qhash64_vectors.txt`; the x86 and ARM CI jobs both reproduce them bit-for-bit, and the differential suite proves every backend matches the scalar oracle. Any drift is a build failure.

**Avalanche (REQ-K7-003).** A first-party SMHasher-subset gate: over ≥100k seeded samples per type (nightly), each input-bit flip flips each output bit with probability within 0.5 ± 0.02 and no output bit shows bias > 0.02. **Documented limitation:** for narrow key domains the ±0.02 band is not mathematically satisfiable by *any* function — an 8-bit key has only 2⁷ distinct `(x, x^bit)` flip pairs, so the sampling error floor exceeds 0.02. The gate therefore uses a distinct-pair-aware band (`max(0.02, 6·√(0.25/distinct_pairs))`); the PR-tier run uses a smaller sample with the same widened band. This is a subset of full SMHasher — a complete SMHasher run is a dev-time activity, not a CI dependency (PRD 08 §K7 quality gate).

## Ledger

**Verdict (Apple M2, v0.4, `neon` vs `autovec`):** **parity** (geomean 1.00× over 2 published pairs). Expected: the shipped NEON `hash64` *is* the GPR `fmix64` chain (the K7 decision, `kUseVectorHash=false`), so it neither beats nor trails the autovectorized reference — hashing is compute-bound and the scalar chain already saturates the integer-multiply pipes. `combine` delegates to the same chain.

| configuration | neon vs autovec | entries |
|---|---|---|
| `hash64` i64 n=65536 | 1.00× | `qle:apple-m2-20260704-883c08552f35-bm-hash-hash64-neon-i64-n-65536-65536` `qle:apple-m2-20260704-883c08552f35-bm-hash-hash64-autovec-i64-n-65536-65536` |
| `combine` u64 n=65536 | 1.00× | `qle:apple-m2-20260704-883c08552f35-bm-hash-combine-neon-u64-n-65536-65536` `qle:apple-m2-20260704-883c08552f35-bm-hash-combine-autovec-u64-n-65536-65536` |

Apple M2 is a **secondary platform** (`secondary_platform`, `no_pmu`: no cycle counters — REQ-LEDGER-008); it is the only registered machine at v0.4 (the ≥2-µarch coverage gate is an open deferral, [gate M6](../releases/gates/M6.md)). Reproduction: [disputes guide](../guides/disputes.md).

## Validation

`tests/unit/test_hash.cpp` (golden-vector reproduction, `-0.0` canonicalization, zero-extension) · `tests/property/prop_hash.cpp` (avalanche/bias gate, batch≡elementwise, seed sensitivity) · `tests/differential/diff_isa_hash.cpp` (backends vs the oracle, byte-exact) · `tests/fuzz/fuzz_hash.cpp` · `bench/micro/bench_hash.cpp` (the K7 NEON evidence instrument; hypothesis in-source).

---
*Traceability: REQ-K7-001..004, REQ-KERNEL-007, REQ-TEST-016; ADR-006/-012/-016/-023; PRD 04 §5, 08 §5.*
