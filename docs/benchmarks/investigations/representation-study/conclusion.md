# Conclusion — OPEN

**No conclusion is drawn.** The study's decision rule (method.md) is cross-µarch by construction,
and only two µarchs across two ISAs are registered (`apple-m2-mba` NEON; `intel-i9-9900k` AVX2,
run 20260805-f7b016f85d08) — short of the bar. Drawing a general bitmap-vs-selvec
recommendation from a single ISA would violate both the pre-registered rule (≥3 ISAs, ≥5 µarchs)
and Charter T2 (no over-claimed data).

## What can be stated now (two µarchs, still below the pre-registered bar)

On apple-m2 / NEON, bitmap and selvec K1 production are both flat in selectivity, and bitmap is
the cheaper *producer* by a flat ~9 % (no producer-side crossover). This is an existence result
for H1 and one point of H2 — not a recommendation.

## What closing the study requires (deferred, R-06)

1. Registered ≥3 ISAs across ≥5 µarchs (x86 AVX-512 + Graviton-class ARM added to
   `apple-m2-mba` and `intel-i9-9900k`, whose `compare` entries are committed in run
   20260805-f7b016f85d08).
2. `qle:`-referenced ledger entries for `compare` (both representations) per µarch.
3. The `bench_pipeline` producer+consumer measurement per µarch (the crossover is downstream).
4. Evaluate the pre-registered decision rule; then this file states the recommendation.

The DaMoN/ADMS workshop-paper draft (Charter §9.1) is tracked outside the repo; its link is
recorded in the M9 gate when it exists. It, too, waits on the multi-µarch data.

*Traceability: gate M9; risk R-06 / REQ-LEDGER-012 (owned by M10).*
