# Data

## apple-m2 (NEON) indicative slice — one ISA

**Grade: indicative bench run, NOT a ledger publication.** Direct Google Benchmark run of
`bench_compare` on `apple-m2-mba` (NEON), `--benchmark_min_time=0.5s`, machine not in the pinned-
quiet state the ledger requires (this was an active dev session). Numbers are real and stable
across repeats (see below), but they are **not** CV-gated, empty-`deviations` ledger entries, so
they carry no `qle:` id. The publishable per-µarch entries now exist for x86 AVX2 (intel-coffee-lake run
20260805-f7b016f85d08, below); entries for the remaining µarchs are pending further
registrations (R-06). Reported to characterize one point of H2, not to conclude the study.

`n = 65536` (DRAM-resident), `i64`, `kGt`, wall-clock per call:

| selectivity | bitmap (ns) | selvec (ns) | selvec / bitmap |
|-------------|-------------|-------------|-----------------|
| 1 %  | 20205 | 22402 | 1.11 |
| 10 % | 20295 | 21982 | 1.08 |
| 50 % | 20161 | 22293 | 1.11 |
| 90 % | 20125 | 22011 | 1.09 |
| 99 % | 20227 | 22024 | 1.09 |

Stability check (`sel=50`, two extra repeats): bitmap 20034 / 21313 ns, selvec 22283 / 22018 ns —
the ~7–11 % selvec penalty is robust to run-to-run noise on this host.

## Publishable ledger entries — PENDING (R-06)

## intel-coffee-lake (AVX2) — committed ledger entries

Run `20260805-f7b016f85d08` (i9-9900K, PMU-carrying, zero CV rejections) commits the study's
`compare` axes for both representations under the same ISA, e.g. at i64 n=65536 sel=50%:
`qle:intel-coffee-lake-20260805-f7b016f85d08-bm-compare-bitmap-gt-avx2-i64-n-65536-sel-50-65536-50`
(bitmap) versus
`qle:intel-coffee-lake-20260805-f7b016f85d08-bm-compare-selvec-gt-avx2-i64-n-65536-sel-50-65536-50`
(selection vector). `bitmap_gt` additionally carries its equal-ISA `autovec-avx2` verdict pairs;
`selvec_gt` registers no autovec baseline (bench registration, not a measurement gap).

The CV-gated, entry-referenced data across ≥3 ISAs and ≥5 µarchs is not yet complete — still
needed: an AVX-512 x86 machine and a Graviton-class ARM part. When those are
registered, `quiver_ledger.py run --machine <id> --filter compare` produces the entries per
[method.md](method.md), committed under `ledger/results/` and referenced here as `qle:<entry_id>`.
No cross-µarch numbers are recorded until then (Charter T2 — no invented data).

*Traceability: bench_compare (`selvec_gt`/`bitmap_gt`); ledger machine `apple-m2-mba`; gate M9.*
