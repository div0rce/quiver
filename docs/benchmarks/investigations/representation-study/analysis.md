# Analysis

## What the one-ISA slice shows

On `apple-m2-mba` (NEON), at `n = 65536`, `i64`:

- **H1 confirmed (bitmap flat in selectivity).** Bitmap production is 20.1–20.3 µs across the
  whole 1 %–99 % range — a ≤ 1 % spread, i.e. data-independent, as the branch-free fixed-width
  core predicts (REQ-KERNEL-003).
- **H2, NEON point: selvec is *also* flat, not selectivity-scaling.** Selvec production is
  22.0–22.4 µs across the range — likewise flat. This matches the full-vector-store compaction
  core (the NEON K1 selvec path stores a whole permuted lane group per input group and advances
  the cursor by popcount; store traffic is data-independent within the capacity region,
  REQ-MEM-008). So on this ISA the "selvec scales with matches" intuition (H2's scalar case) does
  **not** hold — a concrete instance of H2's ISA-dependence claim.
- **Producer cost: bitmap wins by a flat ~9 %.** Selvec is 1.08–1.11× the bitmap cost with no
  selectivity dependence, so on this µarch there is **no producer-side crossover** (`s*` does not
  exist within the range) — bitmap is the cheaper producer everywhere. The extra ~9 % is the
  selvec core's per-group LUT permute + index-vector store over the bitmap's single packed store.

## What this does NOT settle

The decision rule (H3, the practically useful crossover) is explicitly cross-µarch and
producer+consumer. One ISA cannot answer it:

- **Other compaction primitives may behave differently.** AVX-512 `vpcompress` compresses to a
  register with a mask-driven store whose cost model differs from NEON's TBL/LUT path; AVX2 uses
  the LUT-permute path. Whether selvec stays flat and whether the ~9 % penalty holds is a
  per-primitive question.
- **The crossover is downstream.** A cheaper-to-*produce* bitmap can still lose end-to-end when
  its consumer (K2 filter) touches more memory than the selvec's consumer (K3/K5) at low
  selectivity. That is the `bench_pipeline` producer+consumer measurement, run per µarch once the
  producer numbers exist.
- **µarch spread.** Vector width, store throughput, and gather cost move the numbers; the ≥5-µarch
  bar exists precisely so the recommendation is not an apple-m2 artifact.

## Next data required

Per [method.md](method.md): x86 AVX2 is collected — intel-coffee-lake `compare` entries are
committed (run 20260805-f7b016f85d08) and referenced from [entries.md](entries.md). Still needed:
an AVX-512 x86 machine and a Graviton-class ARM part; run the `compare` filter on each; add their
`qle:`-referenced entries to
[entries.md](entries.md); then evaluate the pre-registered decision rule across the ≥3-ISA /
≥5-µarch set and draw the [conclusion](conclusion.md).

*Traceability: hypotheses H1–H3 (question.md); REQ-KERNEL-003, REQ-MEM-008; gate M9.*
