# Differential fuzzing (REQ-TEST-007, ADR-009)

One libFuzzer target per kernel family (`tests/fuzz/fuzz_<family>.cpp`). Each target decodes
the fuzz byte stream through a first-party bounded decoder (`fuzz_common.h`) into
**contract-valid** parameters only (ADR-025: selection vectors sorted-unique in range,
indices and dictionary codes in bounds by construction; exhausted input decodes as zeroes),
then runs the same call on **every host-available backend** and asserts equality against the
scalar baseline:

- bitmap outputs: byte-exact over `bitmap_bytes(n)` (tail zeroing included, ADR-016);
- compaction outputs: count, then the first *count* elements — the scratch region past the
  cursor is intentionally backend-specific (REQ-MEM-008);
- reductions: bit-exact, except float sums, which are checked against the **per-backend
  ADR-013 policy oracle** (dense AVX2 = blocked; everything else = strict fold), with NaN
  results compared as a class (payloads follow hardware operand order, which C++ does not
  pin — see the M4 gate).

## Building and running

Clang only (libFuzzer). The fuzz preset composes `QUIVER_SANITIZE=address;undefined` so the
**library** is instrumented, not just the harness:

```sh
CC=clang-18 CXX=clang++-18 cmake --preset fuzz
cmake --build --preset fuzz -j
build/fuzz/tests/quiver_fuzz_filter -max_total_time=60 tests/fuzz/corpus/filter
```

Reproduce a crash: `build/fuzz/tests/quiver_fuzz_<family> <crash-file>`.

## Corpus policy

Seed + regression corpora are committed under `tests/fuzz/corpus/<family>/`; crash inputs
join the corpus (named `crash-<what>`) once fixed. CI runs every family ≥ 30 s per PR
(fuzz-smoke, REQ-CI-005); nightly runs 40 min per family (≥ 4 h total) with corpus
minimization and crash-artifact upload.

## Track record

The very first M4 fuzz session found two real defects and one spec-level gap: the
empty-`SelVec{nullptr,0}` façade defect (heap overflow via fused `dict_decode`; now
`tests/regression/reg_empty_selvec.cpp` — the fix that activated the regression suite per
REQ-TEST-011), a harness null-`memcmp` UB, and the NaN-payload non-reproducibility of float
sums that produced the gate M4 oracle amendment.

---
*Traceability: REQ-TEST-007/-011, REQ-CI-005, REQ-SEC-001; ADR-009, ADR-013, ADR-025.*
