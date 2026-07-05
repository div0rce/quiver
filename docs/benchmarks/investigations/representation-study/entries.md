# Data

## apple-m2 (NEON) indicative slice — one ISA

**Grade: indicative bench run, NOT a ledger publication.** Direct Google Benchmark run of
`bench_compare` on `apple-m2-mba` (NEON), `--benchmark_min_time=0.5s`, machine not in the pinned-
quiet state the ledger requires (this was an active dev session). Numbers are real and stable
across repeats (see below), but they are **not** CV-gated, empty-`deviations` ledger entries, so
they carry no `qle:` id. The publishable per-µarch entries are pending a quiet registered run
(R-06). Reported to characterize one point of H2, not to conclude the study.

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

The CV-gated, entry-referenced data across ≥3 ISAs and ≥5 µarchs is not yet collected — only
`apple-m2-mba` is registered. When x86 (AVX2, AVX-512) and a Graviton-class ARM part are
registered, `quiver_ledger.py run --machine <id> --filter compare` produces the entries per
[method.md](method.md), committed under `ledger/results/` and referenced here as `qle:<entry_id>`.
No cross-µarch numbers are recorded until then (Charter T2 — no invented data).

*Traceability: bench_compare (`selvec_gt`/`bitmap_gt`); ledger machine `apple-m2-mba`; gate M9.*
