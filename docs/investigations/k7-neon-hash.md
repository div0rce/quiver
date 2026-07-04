# Investigation — K7 qhash64 NEON vector vs GPR chain (REQ-K7-002, REQ-KERNEL-007)

**Status: CLOSED at M6 — decision measured on registered hardware.** Ship the **GPR
scalar chain**; the vector decomposition loses by ~1.3× on Apple M2. Numbers below are
measured on `apple-m2-mba` (Charter T2 — no invented data).

## Question

Should the NEON `hash64` backend compute the frozen ADR-012 `fmix64` chain with (a) a
**vector** decomposition — the 64×64→low-64 multiply built from `umull`/`umlal` 32×32
partials, two 64-bit lanes at a time — or (b) the reference's **4×-unrolled GPR scalar
chain** (Survey §3.9/§4.1: Apple Firestorm/Avalanche have two integer-multiply pipes, so
scalar 64-bit multiplies may out-throughput the 3–4-op vector emulation of one)?

Both must produce bit-identical output (REQ-K7-002); this is purely a throughput question.

## Current state (v0.4, gate M6)

- Both paths are **compiled** in `src/kernels/hash/hash_neon.cpp`: `hash64_vector<T>`
  (variant a) and delegation to `scalar_impl::hash64<T>` (variant b), selected by the
  `kUseVectorHash` constexpr (PRD 08 §5 K7 requires both to exist).
- The constant is `-D`-overridable: building with `-DQUIVER_K7_HASH_VECTOR=1` flips the
  default to the vector path. This is the documented **coverage mechanism** for the
  non-shipped variant (REQ-KERNEL-007 "losing variant remains compiled, test-covered"): the
  differential, golden-vector, avalanche, and fuzz suites all run against the vector path in
  that build configuration, so the dormant variant is never un-exercised.

## Measurement (apple-m2-mba, 2026-07-03)

`bench_hash` `hash64/neon/i64`, Release, 11 repetitions, CPU-time medians (Apple M2 has no
user-accessible PMU — REQ-LEDGER-008 — so this is wall-clock throughput, not cycles):

| n | GPR chain (shipped) | vector decomposition | GPR advantage |
|---|---|---|---|
| 4096 | 1.51 G/s | 1.19 G/s | 1.27× |
| 65536 | 1.54 G/s (CV 3.8%) | 1.17 G/s (CV 3.2%) | **1.32×** |

The n=65536 point is the low-noise measurement (both variants under 4% CV, REQ-LEDGER-005);
the small-n GPR figure is noisier (CV ~13%) but agrees in direction and magnitude across two
independent runs (observed range 1.27–1.36×).

## Decision (pre-registered rule)

Rule fixed before measuring: ship the vector path only if it wins by ≥10% geomean with no
regression; otherwise ship the GPR chain. The vector path **lost** by ~30%, so:

**Ship `kUseVectorHash = false` (GPR 4×-unrolled chain).** The result matches the Survey
prior: emulating one 64-bit multiply costs ~3–4 NEON multiply-family ops (`umull` + two
`umlal`/`mla` + shifts), and with only two 64-bit lanes per vector that does not amortize
against two native scalar multiply pipes issuing 64-bit `mul` directly.

## Reopening criteria

- A NEON/SVE µarch with a wider or cheaper 64-bit vector multiply (e.g. SVE2 `mul` on
  64-bit elements, or a core with >2 vector-multiply ports and ≤2 scalar).
- AVX-512 is unaffected: it has native `vpmullq` and is decided independently at M7.
- Any ledger showing K7 far below its compute roofline on a registered ARM machine.

---
*Traceability: REQ-K7-002, REQ-KERNEL-007; ADR-012; Survey §3.9/§4.1; gate M6.*
