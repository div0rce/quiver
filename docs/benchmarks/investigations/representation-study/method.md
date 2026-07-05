# Method — pre-registered protocol

Registered before the cross-µarch data is collected, so the decision rule cannot be fitted to the
result (Charter T2; the k5-gather investigation follows the same discipline).

## Instrument

`bench_compare` registers both output forms of the same K1 predicate over identical input and
comparand (`bench/micro/bench_compare.cpp`):

- `BM_compare/bitmap_gt/<variant>/i64/n=<n>/sel=<pct>`
- `BM_compare/selvec_gt/<variant>/i64/n=<n>/sel=<pct>`

Both validate their result against an independent recompute before timing (REQ-BENCH-004). The
comparand is placed at the requested selectivity percentile of a seeded uniform `i64` stream.

## Axes

- **Selectivity** `sel ∈ {1, 10, 50, 90, 99}` % (≥5 points, per the acceptance bar).
- **Batch size** `n ∈ {1024, 4096, 65536}` — L1-resident to DRAM-resident; the headline analysis
  uses `n = 65536` (the DRAM-resident size that dominates ledger signal on this hardware).
- **ISA / µarch** — every registered machine, each ISA variant it supports (driven per process by
  the `QUIVER_ISA` cap, as the ledger runner already does). Acceptance needs ≥3 ISAs, ≥5 µarchs.

These axes are within the current QLM; any change bumps QLM per REQ-LEDGER-014.

## Procedure

1. Registered machine per REQ-LEDGER-013 (pinned governor, isolated core, quiet); ledger runner
   `quiver_ledger.py run --machine <id> --filter compare` — fresh process per rep, ≥10 reps,
   shuffled order, seeded percentile-bootstrap CIs, CV > 5 % excluded (ADR-020/021).
2. Commit entries under `ledger/results/<machine>/<date>-<sha>/entries.json`; reference them from
   [entries.md](entries.md) as `qle:<entry_id>` (repo-lint enforces provenance).
3. Repeat per registered µarch until the ≥3-ISA / ≥5-µarch bar is met.

## Decision rule (pre-registered)

The study reports, per µarch, the **break-even selectivity** `s*` where bitmap and selvec
production cost cross, and whether one representation dominates across the whole selectivity range
(“flat winner”). The recommendation to callers is:

- If a representation wins by ≥ 10 % across the whole `sel` range on **every** registered µarch →
  recommend it as the default producer for that consumer chain.
- Otherwise → report `s*` per µarch and recommend selectivity-aware selection; if `s*` is
  µarch-unstable, recommend the representation the *downstream* kernel prefers (bitmap→K2,
  selvec→K3/K5) and note the producer cost is second-order.

Production cost is measured here; end-to-end (producer + consumer) is the `bench_pipeline`
follow-up once the per-µarch producer numbers exist.

## Reopening / completion criteria

Registered access to ≥3 ISAs across ≥5 µarchs — concretely an x86 machine (AVX2, and AVX-512 if
present) and an ARM Graviton-class part in addition to `apple-m2-mba`. Until then the analysis and
conclusion stay open (R-06).

*Traceability: REQ-LEDGER-013/-014, REQ-BENCH-004, ADR-020/021; gate M9.*
