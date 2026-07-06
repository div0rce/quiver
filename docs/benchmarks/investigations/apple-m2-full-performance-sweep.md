# Investigation: Apple M2 full performance sweep

Status: closed for this pass. Every implemented operation group was inventoried against the
committed Apple M2 ledger, every one-sided (unknown) shape was explained, the parity and roofline
claims were proven at the assembly level rather than asserted, and a per-family assembly audit
mapped where real optimization headroom exists. One optimization candidate (sub-byte NEON unpack)
was identified as high value and is pursued as an experimental candidate in a separate follow-up
change; it is not wired to production dispatch until it is proven correct and measured on a quiet
machine. No benchmark data was invented, no losing result was deleted, and the CV and noise policy
was not touched.

This sweep is Apple M2 only (REQ-LEDGER, R-06: one registered machine). No claim here generalizes to
any other microarchitecture.

## What this is

Quiver hand-writes a NEON backend for each kernel family, and the compiler also autovectorizes the
plain scalar loop. On ARM the ledger's `autovec` variant is exactly the shipped scalar backend
measured with `QUIVER_ISA=scalar`. So for every operation the ledger already compares two shipped
paths: handwritten NEON versus the compiler's NEON. This page audits all ten families, classifies
every measured shape, and records where the handwritten NEON path wins, ties, loses, or has not been
resolved.

## Method

1. Aggregated every committed Apple M2 ledger entry across all run directories and paired each
   operation's `neon` variant against its `autovec` variant by a stable key
   (family, api, element_type, sorted axes). The ratio reported is `autovec_ns / neon_ns`: above 1
   means the handwritten NEON path is faster, below 1 means it is slower.
2. Classified each shape with a noise-aware rule (see below), cross-referencing `rejected_noisy.json`
   and the `raw/` measurements so every one-sided shape has a documented reason.
3. Ran a per-family assembly and roofline audit (read-only): for each family, read the NEON backend,
   the scalar reference, and the dispatch table; classified the dominant roofline with a concrete
   bytes-moved or dependency argument; disassembled the hot loop; and either proved the current
   dispatch is optimal or produced a concrete, mechanistically-justified optimization candidate.
4. Proved the load-bearing parity and roofline claims at the machine-code level (bandwidth
   arithmetic and objdump byte-diffing), not by assertion.

### Classification rule

- clear win: ratio at or above 1.05 and the margin exceeds three times the worst CV of the pair.
- parity: ratio in [0.95, 1.05] and the worst CV is under 3 percent.
- loss: ratio at or below 0.95 and the margin exceeds three times the worst CV.
- suspicious: near-parity where the CV is comparable to the gap (noise is at least as large as the
  signal), so the sign of the difference is not trustworthy.
- unknown: only one variant is published for that shape (the other failed the CV gate).

## Machine-state caveat (why no fresh publishable numbers were added)

A gate experiment tested whether the noise-rejected shapes can be recovered by longer measurement
windows. They cannot, under the machine's current state. Example: `filter/bitmap` i64 n=4096 sel=90
was published at CV 0.051 on the original quiet run (2026-07-03), but a rerun with 2 second windows
and 12 repetitions measured CV 0.174 to 0.185. The machine was under heavy concurrent load during
this sweep (load average 6 to 9, a second compute-heavy application resident), which is exactly the
condition the CV policy is designed to reject. Rather than weaken the CV policy or publish noisy
numbers, this sweep treats the committed ledger (captured quiet) as the reference and draws
optimization conclusions from deterministic assembly plus the committed measurements. Any new
publishable ledger entry, and any change to production dispatch based on a new measurement, is
deferred to a quiet-machine run. This is the honest outcome allowed by the definition of done:
unknowns are explained, and re-measurement that does not converge is a valid explanation.

## Full inventory (committed ledger)

ratio = autovec_ns / neon_ns (above 1 means the handwritten NEON path is faster). cv columns are the
published coefficient of variation.

| family | op | type | axes | neon ns | autovec ns | ratio | neon cv | autovec cv | class |
|---|---|---|---|---|---|---|---|---|---|
| arith | add | i64 | n=65536 | 13520.2 | 12123.6 | 0.897 | 0.031 | 0.043 | loss (noisy) |
| arith | mul | f64 | n=65536 | 13809.5 | 12306.2 | 0.891 | 0.039 | 0.034 | loss (noisy) |
| arith_guarded | checked_add | i64 | n=65536,ovf=0 | 26625.3 | 37677.9 | 1.415 | 0.006 | 0.01 | win |
| arith_guarded | checked_add | i64 | n=65536,ovf=1 | 26526 | 37675 | 1.42 | 0.009 | 0.006 | win |
| arith_guarded | checked_add | i64 | n=65536,ovf=500 | 26564.7 | 37631.4 | 1.417 | 0.013 | 0.008 | win |
| arith_guarded | saturating_add | i64 | n=65536 | 13417.6 | 22090.8 | 1.646 | 0.021 | 0.006 | win |
| compare | bitmap_gt | i64 | n=1024,sel=1 | - | 223.4 | - | - | 0.05 | unknown |
| compare | bitmap_gt | i64 | n=1024,sel=10 | 323 | 225.4 | 0.698 | 0.028 | 0.023 | loss |
| compare | bitmap_gt | i64 | n=1024,sel=50 | - | 226.5 | - | - | 0.03 | unknown |
| compare | bitmap_gt | i64 | n=4096,sel=10 | - | 884.1 | - | - | 0.029 | unknown |
| compare | bitmap_gt | i64 | n=4096,sel=50 | 1317.7 | - | - | 0.033 | - | unknown |
| compare | bitmap_gt | i64 | n=4096,sel=90 | 1289.8 | 890 | 0.69 | 0.033 | 0.042 | loss |
| compare | bitmap_gt | i64 | n=4096,sel=99 | 1272.7 | 885.9 | 0.696 | 0.028 | 0.017 | loss |
| compare | bitmap_gt | i64 | n=65536,sel=1 | 20569.2 | - | - | 0.045 | - | unknown |
| compare | bitmap_gt | i64 | n=65536,sel=50 | 20277.8 | - | - | 0.029 | - | unknown |
| compare | bitmap_gt | i64 | n=65536,sel=90 | - | 14181.3 | - | - | 0.022 | unknown |
| filter | bitmap | i64 | n=4096,pat=clustered,sel=10 | 1470.1 | - | - | 0.013 | - | unknown |
| filter | bitmap | i64 | n=4096,pat=clustered,sel=50 | 1519.6 | 2569.8 | 1.691 | 0.044 | 0.037 | win |
| filter | bitmap | i64 | n=4096,pat=uniform,sel=1 | - | 2608 | - | - | 0.03 | unknown |
| filter | bitmap | i64 | n=4096,pat=uniform,sel=50 | - | 2551.3 | - | - | 0.022 | unknown |
| filter | bitmap | i64 | n=4096,pat=uniform,sel=90 | 1400.7 | 2561 | 1.828 | 0.048 | 0.048 | win |
| filter | bitmap | i64 | n=4096,pat=uniform,sel=99 | 1462.5 | - | - | 0.033 | - | unknown |
| filter | bitmap | i64 | n=65536,pat=clustered,sel=10 | 24361.8 | 40291.2 | 1.654 | 0.032 | 0.031 | win |
| filter | bitmap | i64 | n=65536,pat=clustered,sel=50 | - | 40340.3 | - | - | 0.033 | unknown |
| filter | bitmap | i64 | n=65536,pat=clustered,sel=99 | 23201.1 | 40887.8 | 1.762 | 0.042 | 0.034 | win |
| filter | bitmap | i64 | n=65536,pat=uniform,sel=1 | 24345.6 | 40803.7 | 1.676 | 0.018 | 0.02 | win |
| filter | bitmap | i64 | n=65536,pat=uniform,sel=90 | 23247.3 | 40369.3 | 1.737 | 0.023 | 0.018 | win |
| hash | combine | u64 | n=65536 | 59414.4 | 59362 | 0.999 | 0.004 | 0.007 | parity |
| hash | hash64 | i64 | n=65536 | 37778.8 | 37761.2 | 1 | 0.008 | 0.006 | parity |
| mask | and | bitmap | n=1048576 | 3150.5 | 3133.5 | 0.995 | 0.05 | 0.046 | suspicious |
| mask | and | bitmap | n=4096 | 9.6 | 9.5 | 0.993 | 0.024 | 0.011 | parity |
| mask | and | bitmap | n=65536 | 125.3 | 123.6 | 0.987 | 0.012 | 0.015 | parity |
| reduce | sum_wrap | f64 | n=4096,nulls=0 | 441.8 | 3510.9 | 7.946 | 0.026 | 0.011 | win |
| reduce | sum_wrap | f64 | n=65536,nulls=0 | 7347.4 | 58141.2 | 7.913 | 0.009 | 0.017 | win |
| reduce | sum_wrap | i64 | n=4096,nulls=10 | 799.2 | 1108.5 | 1.387 | 0.03 | 0.018 | win |
| reduce | sum_wrap | i64 | n=65536,nulls=10 | - | 18475.1 | - | - | 0.036 | unknown |
| select | bitmap_to_selvec | u32 | density=1,n=4096 | 342.3 | 2625.5 | 7.671 | 0.019 | 0.032 | win |
| select | bitmap_to_selvec | u32 | density=1,n=65536 | - | 41155 | - | - | 0.027 | unknown |
| select | bitmap_to_selvec | u32 | density=10,n=4096 | - | 2546 | - | - | 0.026 | unknown |
| select | bitmap_to_selvec | u32 | density=10,n=65536 | - | 40842.1 | - | - | 0.025 | unknown |
| select | bitmap_to_selvec | u32 | density=50,n=65536 | - | 40531.6 | - | - | 0.032 | unknown |
| select | bitmap_to_selvec | u32 | density=90,n=4096 | 339.9 | 2550.5 | 7.503 | 0.023 | 0.018 | win |
| select | bitmap_to_selvec | u32 | density=90,n=65536 | - | 40712.9 | - | - | 0.02 | unknown |
| select | bitmap_to_selvec | u32 | density=99,n=4096 | - | 2519.8 | - | - | 0.024 | unknown |
| take | dict_decode | i64_u32 | dict=256KiB,n=65536 | 26509.9 | 27261.4 | 1.028 | 0.02 | 0.035 | suspicious |
| take | dict_decode | i64_u32 | dict=32KiB,n=65536 | 13160.7 | 13124.9 | 0.997 | 0.018 | 0.017 | parity |
| take | dict_decode | i64_u32 | dict=4KiB,n=65536 | 12950.7 | 12924.6 | 0.998 | 0.014 | 0.027 | parity |
| take | dict_decode | i64_u32 | dict=65536KiB,n=65536 | 59270.5 | 59535.5 | 1.004 | 0.016 | 0.014 | parity |
| take | dict_decode | i64_u32 | dict=8192KiB,n=65536 | 44793.2 | 43834.1 | 0.979 | 0.021 | 0.015 | parity |
| unpack | unpack_for | u32 | n=65536,w=1 | 64802.8 | 70901.3 | 1.094 | 0.008 | 0.004 | win |
| unpack | unpack_for | u32 | n=65536,w=16 | 6558.7 | 130344 | 19.873 | 0.016 | 0.009 | win |
| unpack | unpack_for | u32 | n=65536,w=24 | 179976 | 197710 | 1.099 | 0.024 | 0.01 | win |
| unpack | unpack_for | u32 | n=65536,w=32 | 6331 | 268575 | 42.422 | 0.027 | 0.019 | win |
| unpack | unpack_for | u32 | n=65536,w=4 | 64784.2 | 70891.6 | 1.094 | 0.006 | 0.006 | win |
| unpack | unpack_for | u32 | n=65536,w=7 | 108056 | 116122 | 1.075 | 0.008 | 0.002 | win |
| unpack | unpack_for | u32 | n=65536,w=8 | 5490.4 | 70425.6 | 12.827 | 0.012 | 0.005 | win |

### Classification counts

- win: 22
- unknown: 19 (every one explained below; all are the opposite variant failing the CV gate)
- parity: 8
- loss: 5 (all routed to fallback by PR #27)
- suspicious: 2

## Unknown resolution (every one-sided shape explained)

There is no join or parsing bug: the key is stable and correct. Every unknown is a shape where the
opposite variant was measured but rejected by the CV > 5 percent policy (REQ-LEDGER-005), so only one
side is published. The committed ledger is a CV-filtered subset, not a complete paired sweep.

| op | type | axes | present variant | missing variant diagnosis |
|---|---|---|---|---|
| compare/bitmap_gt | i64 | n=1024,sel=1 | autovec | neon rejected-noisy cv=0.064 |
| compare/bitmap_gt | i64 | n=1024,sel=50 | autovec | neon rejected-noisy cv=0.104 |
| compare/bitmap_gt | i64 | n=4096,sel=10 | autovec | neon rejected-noisy cv=0.122 |
| compare/bitmap_gt | i64 | n=4096,sel=50 | neon | autovec rejected-noisy cv=0.121 |
| compare/bitmap_gt | i64 | n=65536,sel=1 | neon | autovec rejected-noisy cv=2.477 |
| compare/bitmap_gt | i64 | n=65536,sel=50 | neon | autovec rejected-noisy cv=0.094 |
| compare/bitmap_gt | i64 | n=65536,sel=90 | autovec | neon rejected-noisy cv=0.056 |
| filter/bitmap | i64 | n=4096,pat=clustered,sel=10 | neon | autovec rejected-noisy cv=0.052 |
| filter/bitmap | i64 | n=4096,pat=uniform,sel=1 | autovec | neon rejected-noisy cv=0.177 |
| filter/bitmap | i64 | n=4096,pat=uniform,sel=50 | autovec | neon rejected-noisy cv=0.161 |
| filter/bitmap | i64 | n=4096,pat=uniform,sel=99 | neon | autovec rejected-noisy cv=0.075 |
| filter/bitmap | i64 | n=65536,pat=clustered,sel=50 | autovec | neon rejected-noisy cv=0.131 |
| reduce/sum_wrap | i64 | n=65536,nulls=10 | autovec | neon rejected-noisy cv=0.067 |
| select/bitmap_to_selvec | u32 | density=1,n=65536 | autovec | neon rejected-noisy cv=0.062 |
| select/bitmap_to_selvec | u32 | density=10,n=4096 | autovec | neon rejected-noisy cv=0.072 |
| select/bitmap_to_selvec | u32 | density=10,n=65536 | autovec | neon rejected-noisy cv=0.119 |
| select/bitmap_to_selvec | u32 | density=50,n=65536 | autovec | neon rejected-noisy cv=0.057 |
| select/bitmap_to_selvec | u32 | density=90,n=65536 | autovec | neon rejected-noisy cv=0.284 |
| select/bitmap_to_selvec | u32 | density=99,n=4096 | autovec | neon rejected-noisy cv=0.231 |

The paired shapes in these same families point one direction: where both sides cleared the CV gate,
filter, select, and reduce all show large handwritten-NEON wins (1.65x to 7.9x). The unknowns are a
coverage gap on a noisy machine, not evidence of a hidden loss.

## Losses (all routed to fallback by PR #27)

All five losses are the two shapes already resolved by PR #27: the 64-bit compare bitmap pack and
the 8-byte elementwise arithmetic. Production dispatch already runs the faster autovectorized scalar
code for these (the NEON backend function delegates internally). The committed loss rows are
preserved as the justification; they are the pre-delegation handwritten measurement.

| op | type | axes | ratio | committed entries (qle) |
|---|---|---|---|---|
| arith/add | i64 | n=65536 | 0.897 | `qle:apple-m2-20260704-883c08552f35-e-bm-arith-add-neon-i64-n-65536-65536` `qle:apple-m2-20260704-883c08552f35-f-bm-arith-add-autovec-i64-n-65536-65536` |
| arith/mul | f64 | n=65536 | 0.891 | `qle:apple-m2-20260704-883c08552f35-bm-arith-mul-neon-f64-n-65536-65536` `qle:apple-m2-20260704-883c08552f35-f-bm-arith-mul-autovec-f64-n-65536-65536` |
| compare/bitmap_gt | i64 | n=1024,sel=10 | 0.698 | `qle:apple-m2-20260703-4ec273e2904d-bm-compare-bitmap-gt-neon-i64-n-1024-sel-10-1024-10` |
| compare/bitmap_gt | i64 | n=4096,sel=99 | 0.696 | `qle:apple-m2-20260703-4ec273e2904d-bm-compare-bitmap-gt-autovec-i64-n-4096-sel-99-4096-99` |

See the [Apple M2 NEON losses investigation](apple-m2-neon-losses.md) for the full PR #27 record,
including the byte-identical codegen proof that the delegated path runs exactly the autovec-measured
machine code.

## Suspicious (noise at least as large as the signal)

| op | type | axes | ratio | neon cv | autovec cv | why |
|---|---|---|---|---|---|---|
| mask/and | bitmap | n=1048576 | 0.995 | 0.05 | 0.046 | near-parity; both CVs at the exclusion boundary |
| take/dict_decode | i64_u32 | dict=256KiB,n=65536 | 1.028 | 0.02 | 0.035 | near-parity; gap within the autovec CV |

Both are near-parity memory- or latency-bound shapes where the sign of the tiny difference is not
trustworthy at the measured CV. The roofline analysis below shows both are bandwidth or gather bound,
so a true tie is the expected physics; a quiet-machine rerun would confirm the exact number but would
not change the dispatch decision.

## Roofline and parity proofs (the "no headroom" claims, proven not asserted)

- mask/and (bitmap AND). This is a pure move: read two bitmaps, write one, so 3 bytes moved per 8
  elements. Achieved bandwidth from the committed medians: 160 GB/s at n=4096, 196 GB/s at n=65536,
  125 GB/s at n=1048576. These exceed the roughly 100 GB/s DRAM figure because the working sets (2 KB
  to 384 KB) fit in the L1 or the 16 MB L2, so the op runs at cache bandwidth. You cannot move bytes
  faster than cache bandwidth, so parity is the roofline and there is no NEON headroom for the
  transform. The loop is already 4-way unrolled `vandq`.
- hash64 and hash64_combine. On Apple M2 the NEON backend function is a single tail-call branch into
  `scalar_impl::hash64`, and that instantiation is byte-identical (69 instructions for hash64, 88 for
  combine, after address normalization) between the NEON translation unit and the scalar translation
  unit. On this microarchitecture the NEON hash is literally the scalar GPR multiply chain
  (`kUseVectorHash = false`), which the K7 investigation selected because emulating one 64-bit
  multiply costs three to four NEON multiply-family ops against only two 64-bit lanes. Parity is
  code-identity; there is no headroom without a wider or cheaper 64-bit vector multiply.
  See [K7 NEON hash](../../investigations/k7-neon-hash.md).
- take/dict_decode. The NEON backend delegates every entry point to the scalar reference by design:
  NEON has no gather instruction, so a random-access `out[i] = dict[codes[i]]` gather is inherently
  scalar loads with several in flight (memory-level parallelism). It is latency and load-port bound,
  which is why it ties the scalar path across every dictionary size (from L1-resident 4 KiB to
  64 MiB). Parity is code-identity; there is no NEON headroom without gather hardware.

## Per-family assembly audit and dispatch decisions

Dispatch on Apple M2 is uniform: the feature mask resolves to NEON, and every family's backend row
selects the `neon::` function (`QUIVER_NEON_BACKEND`). For the PR #27 shapes the `neon::` function
delegates internally to `scalar_impl` for the losing widths, so the fastest correct code runs without
a dispatch-table change.

| family | roofline (dominant op) | audit verdict | dispatch decision |
|---|---|---|---|
| compare (K1) | compute-bound (pack reduce) | i64 bitmap settled by PR #27 (delegates); narrow widths and selvec keep handwritten NEON | keep; narrow-width measurement is an open coverage gap |
| filter (K2) | compute / issue-port-bound | handwritten NEON wins 1.65x to 1.83x; selvec delegates (gather) | keep NEON |
| select (K3) | store-throughput / LSU-bound | handwritten NEON wins about 7.5x; no headroom | keep NEON, optimal |
| mask (K4) | memory-bound (transforms) | combine/not/popcount optimal; all/any/none are a deferred candidate | keep NEON |
| take (K5) | gather latency-bound | already fully delegates to scalar (no gather hw); parity is code-identity | keep (delegation), optimal |
| reduce (K6) | latency / compute-bound | sum wins up to 7.9x; dense min/max is a deferred candidate | keep NEON |
| hash (K7) | compute-bound (64-bit mul port) | neon is the GPR chain; parity is code-identity | keep (settled) |
| unpack (K8) | mixed; sub-byte compute-bound | byte-aligned widths win 12x to 42x; sub-byte and w=24 delegate to scalar (about 1.09x) and are the primary optimization candidate | keep scalar for sub-byte until the candidate is proven and measured quiet |
| arith (K9) | memory-bound | 8-byte settled by PR #27 (delegates); narrow widths keep NEON | keep |
| arith_guarded (K10) | mixed | checked and saturating add/sub win 1.42x to 1.65x; all multiplies delegate (REQ-K10-003) | keep |

## Optimization candidates

Three buckets, kept distinct on purpose.

Proven (no code change): mask/and, take/dict_decode, and hash are at their roofline or are
code-identical to the scalar path; the proofs are above. select is store-port bound and already wins.
These are documented as optimal, not left as open questions.

Relative-only, pursued now (measurement deferred to a quiet machine): sub-byte NEON unpack. The
committed ledger shows byte-aligned widths reach 12x to 42x through vectorization while sub-byte and
irregular widths (w = 1, 4, 7, 24) delegate to the scalar gather and sit at about 1.09x. The
mechanism (replace a per-value serial bit gather with a vectorized widen that streams toward the same
output-write roofline the byte-aligned path already hits) is near-certain in direction. This is a
recorded follow-up (gate M6, unpack API notes), not a rejected option. It is implemented as an
experimental candidate behind an internal seam in a follow-up change and is not wired to production
dispatch until it is proven correct and measured on a quiet machine.

Deferred and unmeasured (mechanism identified, not implemented here): each of these is a plausible
improvement whose net benefit is either input-distribution dependent or below the current noise
floor, so none can be honestly measured under the present load. They are recorded for a quiet-machine
follow-up.

- reduce dense min/max: give it four independent accumulators, mirroring dense_sum, to break the
  single-accumulator serial dependency chain (the same ILP move that earns sum its 7.9x). Bit-exact
  for integer min/max and safe for float here because the existing NaN and signed-zero rescues
  normalize the fold. Not benchmarked today, so it needs an added min/max benchmark on a quiet
  machine. Not combined with the unpack change.
- mask all/any/none: replace the byte-at-a-time scalar delegation with a NEON `vandq` or `vorrq`
  reduce plus coarse-grained early exit. Large potential win on no-early-exit inputs (the common
  null-free fast path), roughly neutral on inputs that exit in the first bytes. Net benefit is
  input-distribution dependent and unmeasured.
- filter i8 bitmap: a precomputed high-nibble control table would shave a few vector-ALU ops per
  group. Only helps if the i8 loop is ALU-bound rather than TBL- or load/store-bound; i8 filter is
  not in the ledger. Low confidence.
- compare narrow-width and selvec: a vertical lane counter would remove one across-lane reduce per
  group, but it is only correct without extra work for the non-inverting, all-valid operations, and
  narrow-width compare is not benchmarked. The higher-value open item here is a measurement, not a
  code change: narrow-width handwritten NEON compare has never been benchmarked on M2, and the i64
  delegation does not transfer to narrow widths (their pack is far cheaper per element), so narrow
  NEON plausibly wins and should stay non-delegated. Adding narrow-width coverage is legitimate
  future work.
- arith_guarded narrow widening-multiply: recorded follow-up on the family page; not pursued here.

## Remaining losses, ties, and unknowns

- Losses: none unaddressed. All five are PR #27 shapes already routed to the faster path.
- Ties: mask, take, and hash are at their roofline or code-identical to scalar; kept as-is. The two
  suspicious near-parity rows would be confirmed by a quiet-machine rerun but the dispatch decision
  does not depend on the exact number.
- Unknowns: all 19 are noise rejections of the opposite variant, explained above. Recovering them
  needs a quiet machine; the gate experiment shows longer windows do not help under the current load.
- Coverage gaps worth a future quiet run: narrow-width compare (i8/i16/i32), reduce min/max, and the
  sub-byte unpack candidate validation.

## Honesty statement

No benchmark data was invented. No losing result was deleted; the five losses are preserved with
their committed entry ids. The CV and noise policy was not weakened, and no benchmark shape was
changed to manufacture a win. Every "no headroom" claim is backed by bandwidth arithmetic or an
objdump byte-diff, not assertion. Where the machine could not produce a trustworthy measurement, that
is stated plainly and the conclusion is deferred rather than forced. All conclusions are Apple M2
only (R-06, one registered machine).

## Traceability

REQ-KERNEL-007 (resolution-time backend choice), REQ-LEDGER-005 (CV policy), REQ-LEDGER-015 (ledger
entry references), Charter T7 (publish honest verdicts). Related: [Apple M2 NEON
losses](apple-m2-neon-losses.md), [K7 NEON hash](../../investigations/k7-neon-hash.md). The sub-byte
unpack candidate and its investigation land in a follow-up change.
