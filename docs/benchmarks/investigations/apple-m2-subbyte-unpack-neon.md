# Investigation: sub-byte NEON unpack candidate (Apple M2)

Status: closed, PROMOTED. The candidate landed first as an experimental path off production dispatch
(correctness and the no-over-read bound proven, measurement deferred); a subsequent quiet-machine
ledger measurement showed it beats the scalar gather by 6.9x to 11.0x with every CV under 0.9%, so
`QUIVER_K8_SUBBYTE_VECTOR` now defaults to 1 and production dispatch runs the vectorized sub-byte
path (REQ-KERNEL-007). Building with `-DQUIVER_K8_SUBBYTE_VECTOR=0` reverts to the scalar reference.
This page records why the path exists, how it stays in bounds, how it is tested, and the measurement
that promoted it.

Apple M2 only (R-06, one registered machine). No claim here generalizes to another microarchitecture.

## Why this exists

The [full Apple M2 sweep](apple-m2-full-performance-sweep.md) found the one clear optimization
candidate with committed ledger evidence: K8 unpack. The committed ledger shows the byte-aligned
widths that the NEON backend vectorizes (w = 8, 16, 32) beat the autovectorized scalar path by 12x
to 42x, while the sub-byte and irregular widths (w = 1, 4, 7, 24) delegate to the scalar per-value
bit gather and sit at about 1.09x (barely above scalar). Sub-byte widths are the common Parquet-RLE
case, so this is the primary bit-packing workload left unvectorized. Vectorizing them is a recorded
follow-up (gate M6, the unpack API per-ISA notes), not a rejected option.

## The no-over-read contract (absolute)

REQ-K8-002 / REQ-SEC-004: `packed` is untrusted and the kernel must read exactly ceil(n*w/8) bytes,
no more. The candidate preserves this exactly. The vector loop reads only the bytes each block owns
(assembled with an inline bounded byte load, never a wider vector load that could spill past the
buffer), and the final fewer-than-8 values use the scalar reference's exact-byte gather. Nothing past
ceil(n*w/8) is ever touched.

## The 8-values-per-block alignment insight

For any integer bit width w, eight consecutive values occupy exactly 8*w bits = w bytes and re-align
to a byte boundary. That is what makes a bounded vector loop possible: process 8 values per
iteration reading exactly w bytes (w <= 7 fits in a 64-bit word), and handle the trailing n mod 8
values with the scalar tail. Block b reads bytes [b*w, b*w+w); the last full block ends at
(n/8)*w <= ceil(n*w/8), so the loop is strictly in bounds and the scalar tail covers the rest.

## Implementation strategy

For w in [1,7], each 8-value block:

1. Assemble the w packed bytes little-endian into a 64-bit word with an inline bounded loop (a
   runtime-size `memcpy` compiled to a function call per block, which would dominate the 8-value
   work, so it is written out explicitly and reads exactly w bytes).
2. Extract lanes 0..3 from the low 32 bits and lanes 4..7 from the word shifted right by 4*w, each
   with a per-lane right shift vector (`ushl` by [0, -w, -2w, -3w]) and an AND with the (1<<w)-1
   mask. This is valid because for w <= 7 each group of four values fits within a 32-bit window.
3. Apply the wrapping frame-of-reference add and narrow or widen to the output type, storing 8
   values per iteration.

The path is behind a compile-time seam, `QUIVER_K8_SUBBYTE_VECTOR`, which mirrors the existing
`QUIVER_K7_HASH_VECTOR` evidence-gate. It landed with default 0 (production unchanged, sub-byte
delegating to the scalar reference) while the measurement was pending; after the promotion the
default is 1 (production dispatch runs the vectorized path) and building with
`-DQUIVER_K8_SUBBYTE_VECTOR=0` reverts sub-byte to the scalar reference, which keeps both variants
compiled and A/B-comparable. The path is also exposed as `detail::neon::unpack_subbyte_candidate`
(see `unpack_neon_candidate.h`) so a dedicated test calls it directly, giving CI correctness
coverage on the arm64 runner independent of the seam's default.

## Correctness proof and tests

Correctness is machine-independent and is the hard gate. The candidate is bit-identical to the
ADR-026 scalar oracle. Coverage (in `tests/differential/diff_unpack_subbyte.cpp`, aarch64, runs in
CI):

- Exhaustive: every sub-byte width w in [1,7] against every output type (u8, u16, u32, u64) over
  boundary lengths n in {0, 1, 2, 7, 8, 9, 15, 16, 17, 31, 32, 33, 257} with random packed data,
  compared value-by-value to the oracle.
- Randomized sweep: 4000 iterations per output type over random (width, length, data), the ground a
  fuzzer would cover (libFuzzer itself cannot reach this aarch64 code because the CI fuzz leg is x86;
  this seeded randomized differential stands in for it).
- Bounds: the input buffer is sized to exactly ceil(n*w/8) bytes with a guard page immediately after
  it (and the output sized to exactly n with its own guard page), so any over-read or over-write
  faults. The candidate passes, proving it stays inside the contract bound.

Local validation (before optimizing further, per the plan): the full dev suite passed 211/211 with
the candidate compiled and tested directly; building the whole suite with
`-DQUIVER_K8_SUBBYTE_VECTOR=1` routed sub-byte through the candidate and the exhaustive differential
(diff_isa_unpack, widths 0..max via dispatch) passed; ASan plus UBSan were clean; and an independent
standalone harness under ASan with an mmap guard page confirmed bit-exactness and no over-read across
all widths and boundary lengths.

## Assembly notes

Built at -O3 for aarch64, the candidate compiles to a vectorized 8-values-per-iteration loop using
`ushl`, `and`, and vector stores, with no function call in the hot path (the inline byte load
replaced the per-block `memcpy` call the first cut emitted). The scalar reference is a per-value
LSB-first bit gather (a nested byte loop per value). The static instruction count of the candidate is
larger than the scalar function because it contains both the vector body and the scalar tail and the
inline byte assembler; static size is not a runtime-cost proxy here, and the real question is
throughput, which is unmeasured (see below).

## Measurements

At candidate time: deferred. The sweep ran while the machine was under heavy concurrent load (load
average 6 to 9), which the CV policy is designed to reject, and the local release toolchain was
additionally blocked mid-session by an Xcode license gate; no exploratory number was claimed.

Promotion measurement (machine quieted, load about 1.3 to 2.5; ledger runner, fresh process per
repetition, 10 repetitions, 1 s windows; u32, n=65536; committed run dirs
`20260710-220d2e0236b8` and `-b`):

| width | scalar gather (autovec) | vectorized (neon) | speedup | worst CV |
|---|---|---|---|---|
| w=1 | 67100 ns | 6118 ns | 10.97x | 0.43% |
| w=4 | 67117 ns | 9687 ns | 6.93x | 0.70% |
| w=7 | 108450 ns | 12997 ns | 8.34x | 0.53% |
| w=8 (control, unchanged path) | 66500 ns | 5475 ns | 12.15x | 0.29% |
| w=16 (control, unchanged path) | 121011 ns | 6543 ns | 18.49x | 0.83% |

Why this measurement is trustworthy: the w=8 and w=16 controls are code-untouched by the flag and
reproduce the committed v0.4 numbers (12.83x / 19.87x) within a few percent, so the session is
comparable to the historical ledger; a flag=0 build in the same session also reproduced the
historical scalar-delegation numbers. One methodological trap was caught and corrected before any
number was trusted: the first flag=1 comparison build was silently configured without `-O3`
(a hand-rolled cmake invocation that clobbered `CMAKE_CXX_FLAGS_RELEASE`), and the byte-identical
w=8 control exposed it immediately by "regressing" 43x. Controls that cannot legitimately change are
what make an A/B build comparison falsifiable.

## Dispatch decision

PROMOTED. `QUIVER_K8_SUBBYTE_VECTOR` defaults to 1: production dispatch on aarch64 runs the
vectorized sub-byte path for w in [1,7]. The measurement above satisfies the evidence gate
(REQ-KERNEL-007): a reliable win, well beyond noise, on the registered machine in a quiet state,
with the full correctness suite passing at the flipped default (212/212, including the exhaustive
width differential through real dispatch). Building with `-DQUIVER_K8_SUBBYTE_VECTOR=0` reverts
sub-byte to the scalar reference (the fallback and A/B coverage mechanism, mirroring
`QUIVER_K7_HASH_VECTOR`).

## Remaining risks and next steps

- The runtime-w byte-assembly load is a first cut. Per-width specialization (compile-time w) so the
  load and shifts inline to fixed forms could raise the win further; it is an evidence-gated
  follow-up, and the measured 6.9x to 11.0x already clears the promotion bar without it.
- The path covers w in [1,7] only. Irregular widths such as 24 (multiple bytes per value) are a
  structurally different shape and remain on the scalar reference (measured near-parity there).
- Coverage is one machine (R-06): the promotion evidence is Apple M2 only. The seam makes the
  decision per-build revisitable when the second and third microarchitectures the coverage plan
  calls for are registered. The AVX2/AVX-512 sub-byte paths still delegate to scalar; vectorizing
  them is the analogous x86 follow-up and is decided by their own ledger, not this page.

## Traceability

REQ-K8-001..004, REQ-SEC-004 (no over-read), REQ-KERNEL-007 (evidence-gated backend choice),
REQ-TEST-002/-003/-006. Parent: [Apple M2 full performance sweep](apple-m2-full-performance-sweep.md).
Seam mirrors the K7 hash evidence gate ([K7 NEON hash](../../investigations/k7-neon-hash.md)).
