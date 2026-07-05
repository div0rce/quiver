# Question — bitmap vs selection vector

## The question

K1 `compare` can materialize a predicate result in two representations (Charter §6.2, a
first-class research feature):

- **Bitmap** — `compare_bitmap`: one bit per element, `ceil(n/8)` bytes, written densely.
- **Selection vector** — `compare_selvec`: a sorted `uint32` list of the matching indices.

The same choice recurs downstream (K2 filter consumes a bitmap; K3 select / K5 take consume a
selvec). **When is each representation faster, and how does that depend on selectivity** (the
fraction of elements that match) **and on the ISA/µarch?** The library ships both and lets the
caller choose; this study is the evidence a caller (or a future planner) would use to choose.

## Hypotheses (pre-registered)

1. **H1 — bitmap cost is flat in selectivity.** The bitmap core is branch-free and writes a
   fixed `ceil(n/8)` bytes regardless of match count (REQ-KERNEL-003). Expected: cost depends on
   `n`, not on selectivity.
2. **H2 — selvec cost rises with selectivity on a scalar/scatter core**, because it emits one
   index per match — but is **flat on a full-vector-store compaction core** (the AVX2/NEON path
   stores a whole permuted lane group per input group and advances the cursor by popcount, so the
   store traffic is data-independent within the capacity region, REQ-MEM-008). So the selectivity
   dependence of selvec is itself **ISA-dependent** — a key thing the study measures.
3. **H3 — crossover.** At low selectivity the selvec is smaller and cheaper to *consume*
   downstream (fewer elements to gather); at high selectivity the bitmap is smaller and its
   consumers touch less memory. The break-even selectivity is the practically useful number, and
   it is expected to move with µarch (vector width, store throughput, gather cost).

## Why it needs multiple µarchs

H2/H3 are explicitly about ISA/µarch-dependent behavior (vector width, compaction primitive —
NEON TBL vs AVX2 LUT vs AVX-512 `vpcompress`, store throughput). A single µarch can confirm H1
and characterize one point of H2, but the crossover claim (H3) is only meaningful across ≥3 ISAs
and several µarchs — hence the ≥3-ISA / ≥5-µarch acceptance bar, and why the conclusion is held
open until that hardware is registered (R-06).

*Traceability: Charter §6.2; Survey §11.3 #3; REQ-KERNEL-003; REQ-MEM-008; gate M9.*
