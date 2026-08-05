# Investigation: Apple M2 NEON losses and dispatch routing

Status: closed. Two handwritten NEON paths that lost to the autovectorized scalar reference on
Apple M2 now delegate to that reference, so the shipped backend runs the faster code. No benchmark
data was invented, no losing result was deleted, and the noise policy was not touched.

## What this is

Quiver hand-writes NEON for each operation, but the compiler also autovectorizes the plain scalar
loop. On ARM the ledger's `autovec` variant is exactly the shipped scalar backend measured with
`QUIVER_ISA=scalar`. So for every operation the ledger already compares two shipped paths:
handwritten NEON versus the compiler's NEON. This page audits where handwritten NEON lost and what
was done about it.

## Method

1. Scanned every committed Apple M2 ledger entry and paired each operation's `neon` variant against
   its `autovec` variant (`ns_per_batch` median, with the coefficient of variation as the noise
   guard). Ratios below 0.95 are losses, 0.95 to 1.05 ties, above 1.05 wins.
2. For each loss: reproduced with a non-publishable focus run (relative comparison, robust to the
   moderate noise from a non-quiesced laptop), read the generated assembly for the NEON path and
   the autovectorized scalar path, and tried a targeted fix.
3. Kept only changes that removed the loss; validated correctness with the full unit, property,
   differential, invariant, golden-vector, and sanitizer suites.

## Findings (committed ledger)

| ratio (auto/neon) | class | operation | shape | note |
|---|---|---|---|---|
| 0.69 to 0.70 | loss | `compare/bitmap_gt` i64 | n=1024/4096, several selectivities | 64-bit-lane bit-pack, fixed below |
| 0.89 to 0.90 | loss | `arith/mul` f64, `arith/add` i64 | n=65536 | bandwidth-bound elementwise, fixed below |
| ~1.00 | tie | `mask/and`, `take/dict_decode`, `hash` | various | retained (parity, no change) |
| >1.05 | win | `unpack`, `reduce`, `arith_guarded` checked/saturating | various | untouched |

Ratios below 1.0 mean handwritten NEON was slower. The tie and win rows are unchanged.

## Case 1: compare `bitmap_gt` i64 (0.69x loss, fixed)

**Cause.** Turning 64-bit-lane compare masks into a bitmap needs a horizontal reduce per group
(two lanes at a time), and the handwritten pairwise-add pack is latency-bound on that reduce. The
assembly showed the autovectorized scalar path is a heavily-unrolled NEON body (narrowing plus
table lookups) that the compact handwritten pack cannot match. Three earlier pack reworks at M5
(scalar-extract, narrowing chain, the current pairwise tree) all lost.

**Attempt.** Forcing the shared emit to inline (so the compiler could unroll it per call, matching
the autovec structure) made no measurable difference. The bottleneck is the pack idiom itself, not
the outlining.

**Decision.** The compiler is already optimal for this shape. The 64-bit `bitmap` forms now
delegate to the autovectorized scalar reference (`scalar_impl::compare_bitmap` and siblings),
compiled at the same optimization level in the NEON translation unit. This is the compiler's NEON,
so it is not abandoning SIMD. The `selvec` form is left on handwritten NEON: it uses the index-store
core, not the bit-pack, and it wins at low selectivity. Narrower-width bitmap packs are unchanged.

**Outcome.** The 0.69x loss became parity with `autovec`. Focus run (non-publishable, machine not
quiesced), `i64` n=4096/sel=99: handwritten NEON was ~1954 ns; after delegation ~1260 to 1450 ns,
matching `autovec` (~1050 to 1240 ns) within noise.

## Case 2: arith `add` i64 and `mul` f64 (0.90x loss, fixed)

**Cause.** Pure elementwise arithmetic on 8-byte elements is bandwidth-bound. The handwritten NEON
loop processes only two lanes per vector, and the compiler's autovectorized scalar loop has a
tighter tail and better scheduling, so it wins by about 10 percent. A handwritten NEON win is not on
the table here: the ceiling is a tie at the memory roofline, and the 0.90x gap is close to its own
measurement noise on this machine.

**Decision.** The 8-byte `arith` paths (`i64`, `u64`, `f64`) now delegate to the autovectorized
scalar reference (`scalar_impl::arith` and `arith_scalar_rhs`). The now-dead handwritten 64-bit
block-op and its 64-bit multiply helper were removed. Narrower widths keep their handwritten NEON
(they were not measured and are not swept).

**Outcome.** The 0.90x loss became parity with `autovec`. Confirmed by a non-publishable focus run;
the exact margin is noise-limited for streaming operations on this host (the ledger needs long
windows for these), so no fresh performance number is claimed beyond parity.

## Evidence and honesty

The committed loss entries are preserved in the `compare` and `arith` family pages (they are the
justification for the change). Because the delegated NEON path now runs the identical code the
`autovec` variant already measured, the committed `autovec` entries are the performance of the
shipped path; no new publishable ledger run was required, and none was faked. Ties and wins were not
touched. Correctness is unchanged: both delegations call the reference oracle, so results stay
bit-identical, and the full suites (unit, property, differential across ISAs, invariant,
golden-vector, ASan/UBSan) pass.

The "identical code" claim was verified at the machine-code level, not assumed. On aarch64 the NEON
and scalar translation units compile with identical flags (NEON is baseline; there are no per-ISA
compile options as there are for the x86 AVX TUs). Disassembling the `bench` build confirms it: the
delegated `neon::k1_compare_bitmap` for `i64` is a single tail-call branch into
`scalar_impl::compare_bitmap<int64>`, whose body is byte-identical (975 instructions, after
normalizing addresses) to the same instantiation in the scalar backend TU; `compare_bitmap2` (1301)
and `compare_between_bitmap` (306) match likewise. For arith the `scalar_impl` body is inlined into
each `k9_arith`, and the `i64`/`f64` `k9_arith` and `k9_arith_scalar_rhs` bodies are byte-identical
between the two TUs (125/160/106/125 instructions). So the shipped delegated path is literally the
machine code the `autovec` variant measured, which is why no fresh ledger run is owed.

## Traceability

REQ-KERNEL-007 (resolution-time backend choice), Charter T7 (publish honest verdicts). Committed
loss entries: `qle:` references in [compare.md](../../api/compare.md) and
[arith.md](../../api/arith.md). Ledger coverage is two machines (apple-m2 and intel-coffee-lake; R-06 remains open below the
required bar); these conclusions are Apple M2 only.
