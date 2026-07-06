# Investigation: sub-byte NEON unpack candidate (Apple M2)

Status: experimental candidate landed, NOT on production dispatch. Correctness and the no-over-read
bound are proven; a wall-clock measurement is deferred to a quiet machine, so production still uses
the scalar reference for sub-byte widths. This page records why the candidate exists, how it stays in
bounds, how it is tested, and what must happen before it can ship to dispatch.

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

The candidate is behind a compile-time seam, `QUIVER_K8_SUBBYTE_VECTOR` (default 0), which mirrors
the existing `QUIVER_K7_HASH_VECTOR` evidence-gate. With the default, production dispatch is
unchanged: sub-byte widths delegate to the scalar reference. Building with the flag routes sub-byte
widths to the candidate, which is how the full existing test suite can exercise it. The candidate is
also exposed as `detail::neon::unpack_subbyte_candidate` (see `unpack_neon_candidate.h`) so a
dedicated test calls it directly, giving CI correctness coverage on the arm64 runner without changing
what production runs.

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

Deferred. This sweep ran while the machine was under heavy concurrent load (load average 6 to 9, a
second compute-heavy application resident), which the CV policy is designed to reject, and the local
release toolchain was additionally blocked mid-session by an Xcode license gate. A wall-clock
number taken under that state would be noise, and publishing or dispatching on it would violate the
methodology. No exploratory number is claimed. The direction is motivated only by the committed
ledger: the analogous vectorization of the byte-aligned widths achieves 12x to 42x over the scalar
path, so vectorizing sub-byte widths is expected to help; that expectation is not a measurement and
does not gate dispatch.

## Dispatch decision

Production dispatch is unchanged: sub-byte widths delegate to the scalar reference
(`QUIVER_K8_SUBBYTE_VECTOR` defaults to 0). The candidate ships compiled and CI-tested but off the
production path. It may be promoted to dispatch only after a quiet-machine ledger measurement shows it
reliably beats the scalar path with the methodology satisfied (REQ-KERNEL-007, the same evidence gate
every backend choice uses). If a quiet-machine measurement shows it ties or loses, it stays an
investigation or is removed; it will not be shipped on a loaded-machine number.

## Remaining risks and next steps

- Performance is unmeasured; the current byte-assembly load is a first cut. The likely next
  optimization is per-width specialization (compile-time w) so the load and shifts inline to fixed
  forms, which is the standard fast bit-unpack shape. That work should come with the quiet-machine
  measurement, not before it.
- The candidate covers w in [1,7] only. Irregular widths such as 24 (multiple bytes per value) are a
  structurally different path and remain on the scalar reference.
- Coverage is one machine (R-06). A promotion decision needs the registered Apple M2 in a quiet
  state, and ideally the second and third microarchitectures the coverage plan calls for.

## Traceability

REQ-K8-001..004, REQ-SEC-004 (no over-read), REQ-KERNEL-007 (evidence-gated backend choice),
REQ-TEST-002/-003/-006. Parent: [Apple M2 full performance sweep](apple-m2-full-performance-sweep.md).
Seam mirrors the K7 hash evidence gate ([K7 NEON hash](../../investigations/k7-neon-hash.md)).
