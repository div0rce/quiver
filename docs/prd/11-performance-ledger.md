# 11 — Performance Ledger

## 1. Purpose

The ledger is Quiver's second product (Charter §1): a versioned, machine-readable, reproducible record of what the kernels cost per (variant, microarchitecture, input configuration). This chapter fixes its data model (Surface D), statistics, axis definitions, machine registry, publication workflow, and regression policy. Upstream authority: Charter §6.4 in full; Survey §7.3–§7.5; OA §13 (ledger identity).

## 2. Requirements

| ID | Requirement |
|---|---|
| REQ-LEDGER-001 | Every published entry shall conform to `ledger/schema/ledger-entry.schema.json` (schema version `QLS-1`) and reference a manifest conforming to `manifest.schema.json`. Schemas are Surface D: versioned independently of the library (Charter §7.5); breaking schema changes increment `QLS-n` and never rewrite published data. |
| REQ-LEDGER-002 | Entry fields (normative): `entry_id`, `schema`, `methodology` (`QLM-1`), `benchmark` (REQ-BENCH-002 name), `family`, `api`, `variant`, `element_type`, `axes` (object: the §4 axis values), `machine_id`, `manifest_ref`, `library_version`, `git_commit`, `git_dirty`, `timestamp_utc`, `repetitions`, `results` (per metric: `median`, `min`, `ci95_lo`, `ci95_hi`, `cv`), `metrics` = {`ns_per_batch`, `values_per_s`, `bytes_per_s`, `cycles_per_value`?}, `pmu` (nullable object), `flags` (array ⊆ {`noisy`, `no_pmu`, `secondary_platform`}), `notes`. |
| REQ-LEDGER-003 | Manifest fields (normative): CPU model string, `machine_id`, µarch label, core used + pinning state, frequency governor, turbo/boost state, SMT state, ASLR state, OS + kernel version, compiler + exact flags, LTO state, GB version, library commit, timestamp; plus a free-form `deviations` field that shall be empty for publishable runs. |
| REQ-LEDGER-004 | Statistics (ADR-020): ≥ 10 process-level repetitions per entry; report median and min with seeded percentile-bootstrap 95% CIs (B = 10,000) and CV. Single-run numbers are unpublishable. Never average across different axis values (Survey §7.4/§7.5). |
| REQ-LEDGER-005 | Noise policy: CV > 5% → entry flagged `noisy` and excluded from publication until rerun; 3% < CV ≤ 5% → published with `noisy` flag and a `notes` explanation (Charter §6.4 "investigate above a few percent"). |
| REQ-LEDGER-006 | Repetition orchestration: each repetition is a fresh process; the run order across benchmarks is shuffled per repetition with a recorded seed (interleaving defense — Survey §7.1/§7.3 bias-sampling school). |
| REQ-LEDGER-007 | Ledger runs shall execute only on registered machines (`ledger/machines/<machine_id>.json`: hardware description, µarch, memory config, OS baseline, owner). CI runners are prohibited as ledger sources (shared-tenancy noise; Survey §7.3; db-benchmark bare-metal lesson Survey §2.2). |
| REQ-LEDGER-008 | Apple Silicon entries carry `no_pmu` + `secondary_platform` flags and omit `cycles_per_value` (Charter §6.4; Survey §7.3). The limitation is stated on every page that renders such entries. |
| REQ-LEDGER-009 | Reproduction: `python3 ledger/runner/quiver_ledger.py run --machine <id> --filter <pattern>` shall regenerate any entry class from a clean checkout; the exact command for each entry is derivable from the entry (`benchmark` + `axes`) and documented in the dispute guide. Disputes are handled via the benchmark-dispute issue template requiring a manifest + command output (Charter §12). |
| REQ-LEDGER-010 | Storage: `ledger/results/<uarch>/<yyyymmdd>-<shortsha>/` containing `entries.json` (aggregates), `raw/*.json` (per-repetition GB output — retained), `manifest.json`. Committed via PR; results are append-only (corrections add superseding entries with `notes`, never edit history). |
| REQ-LEDGER-011 | **Publish-losses pledge (Charter T7):** for every (family × ISA) with an explicit backend, the family doc page shall state the explicit-vs-`autovec` verdict derived from ledger entries — wins and losses with equal prominence; at least one loss-or-parity finding being documented is an explicit v0.3 acceptance item if the data shows one (and Survey §4.4/§4.5 predicts it will, e.g. K4). |
| REQ-LEDGER-012 | Coverage gates: v0.3 (M5): ≥ 3 µarchs — one Zen 4/5, one Golden-Cove-class, one Apple M-series; v1.0 (M10): ≥ 5 µarchs including Graviton-class ARM (Charter §6.4/§9.1). |
| REQ-LEDGER-013 | The runner shall validate the environment before a publishable run (governor = performance, SMT state recorded, dirty git tree rejected) and record any deviation; deviations make a run non-publishable (REQ-LEDGER-003). |
| REQ-LEDGER-014 | Methodology is versioned (`QLM-1` = this chapter's §4–§6). Any change to axes, statistics, or orchestration bumps QLM and is documented in `docs/benchmarks/methodology.md` with migration notes. |
| REQ-LEDGER-015 | Docs shall reference entries by `entry_id`; a docs-build check verifies every referenced `entry_id` exists (no hand-copied numbers without provenance — Charter T2). |

## 3. Ledger data flow

```text
Title: Ledger production pipeline
Purpose: REQ-LEDGER-002/006/010 visualization

 registered machine (ledger/machines/<id>.json)
   └─ quiver_ledger.py run
        ├─ env checklist (REQ-LEDGER-013) ──► manifest.json
        ├─ repetition r = 1..R (fresh process each, shuffled order, seed recorded)
        │    └─ quiver_bench_<family> --benchmark_filter=... --quiver_variant=...
        │         ├─ validation vs scalar (REQ-BENCH-004)
        │         └─ GB JSON + PMU counters ──► raw/r<k>.json
        └─ aggregate: median/min/bootstrap-CI/CV ──► entries.json (schema QLS-1)
              └─ PR review ──► ledger/results/** (append-only)
                    └─ docs reference by entry_id (REQ-LEDGER-015)
```

## 4. Axis definitions (single source of truth; QLM-1)

| Axis | Values | Notes |
|---|---|---|
| `batch` | 256, 1024, 4096, 16384, 65536 | Charter §6.2 envelope; cache-residency sweep (Survey §1.4, §11.3 #1) |
| `selectivity` | 1, 10, 50, 90, 99 (%) | Charter §6.4 |
| `pattern` | `uniform`, `clustered` | clustered = geometric runs, mean 64; defeats predictor over-learning (Survey §3.4) |
| `null_density` | 0, 1, 10, 50 (%) | Charter §6.4; 0% must take the no-mask fast path (REQ-K1 spec) |
| `value_dist` | `sequential`, `uniform_random`, `zipf` | zipf: θ = 1.0 over 1,000 distinct values (K7 primary) |
| `alignment` | `aligned64`, `offset1` | offset1 = base + 1 element (REQ-MEM-004 evidence) |
| `dict_size` | 4 KiB, 32 KiB, 256 KiB, 8 MiB, 64 MiB | K5 cache-level sweep |
| `bit_width` | 0..8·sizeof(Out) | K8 |
| `overflow_density` | 0, 0.1, 50 (%) | K10 |

Generators implementing these axes are seeded and portable (REQ-INT-002 discipline; bench-local per REQ-BENCH-015 with drift-alarm conformance test).

## 5. ADR-020 — Statistics implementation

- **Status:** Accepted.
- **Context:** Charter §6.4 fixes the policy surface (≥10 process repetitions, median+min, nonparametric 95% CIs, CV); the PRD owes the estimator mechanics. Survey §7.4 documents the two legitimate schools (min-is-best; distribution-aware) — the charter already committed to reporting both median and min.
- **Alternatives for the CI:** (1) parametric t-intervals — rejected: timing distributions are skewed, non-normal (Survey §7.4); (2) rank-based exact intervals — viable but awkward at R = 10 for the median; (3) **seeded percentile bootstrap** (selected): distribution-free, works for both median and min, trivially reproducible with a recorded seed, B = 10,000 is cheap offline.
- **Decision:** percentile bootstrap, B = 10,000, seed recorded per entry; CV = stddev/mean of repetitions (reported for stability screening only, never as the headline statistic).
- **Consequences:** first-party ~80-line implementation in the runner with golden tests against committed fixtures (REQ [05 §9](05-internal-architecture.md)); documented caveat: CI width at R = 10 is coarse — R may be raised per entry, recorded in `repetitions`.
- **Reconsideration:** if a methodology reviewer (paper referee, M9) requires Kalibera-Jones multi-level design, bump QLM.
- **Related:** REQ-LEDGER-004/014.

## 6. ADR-021 — Ledger data model and storage

- **Status:** Accepted.
- **Context:** Charter Surface D promises machine-readable, independently versioned results; disputes must be answerable from artifacts alone (Charter §12).
- **Alternatives:** (1) database/service — rejected: infrastructure burden, repo-external trust; (2) one growing JSON/CSV file — rejected: merge conflicts, no per-run manifests; (3) **append-only per-run directories with schema-validated JSON committed via PR** (selected): reviewable diffs, git provenance, zero infrastructure.
- **Consequences:** repo size grows with runs (raw JSON retained) — acceptable at v1 scale (~tens of MB); revisit with git-lfs if >200 MB ([21](21-future-work.md)).
- **Reconsideration:** result volume or an external dashboard need.
- **Related:** REQ-LEDGER-001/010.

## 7. Runner failure modes

| Condition | Behavior |
|---|---|
| Env checklist failure (governor, dirty tree) | run marked non-publishable; proceeds only with `--allow-deviations` for local investigation |
| Bench validation abort | run stops; no partial entry emitted |
| GB JSON unparseable / schema drift | versioned error naming the offending field; no silent tolerance beyond documented ignore-unknown-fields |
| CV > 5% after R repetitions | entry flagged `noisy`, excluded; runner suggests rerun with environment guidance |
| PMU open failure | entry proceeds with `no_pmu` flag (REQ-BENCH-005) |

## 8. Machine registry and reference machines

Initial registry targets (M5): one Zen 4/5 Linux desktop, one Golden-Cove-class Linux machine, one Apple M-series (macOS, secondary). M10 additions: Graviton-class ARM Linux, plus one further distinct µarch. Each machine file records the [REQ-LEDGER-003] hardware facts once; entries reference by `machine_id`. Machines are the maintainer's registered hardware — the ledger never claims coverage it does not have (Charter T7).

## 9. Regression policy

Per release (M-gates from v0.2 on): run the tagged regression subset (REQ-BENCH-011) on at least one registered x86 and one ARM machine; compare medians against the previous release's entries for the same (machine, benchmark, axes). Any regression > 3% on the median with non-overlapping CIs → release blocker until explained (a `notes`-documented, accepted regression with cause is a valid resolution; silent regressions are not). Workflow and report format in `docs/benchmarks/methodology.md`.

## 10. Acceptance criteria

Schemas exist and validate all committed results; runner golden statistics tests pass; a full v0.3 ledger run exists for Tier A on ≥3 registered µarchs with zero schema violations; REQ-LEDGER-011 verdicts present on all Tier A family pages; dispute template live; reproduction command verified end-to-end by an M5 gate dry run (fresh clone → entry match within CI bounds).

## 11. Traceability

Charter §1, §6.4, §7.5 Surface D, §9.2, §12, T2/T7 → REQ-LEDGER-001..015 → ADR-020/021 (here), ADR-008/011/022 ([10](10-benchmark-architecture.md)) → MOD-LEDGER ([05 §9](05-internal-architecture.md)) → milestones M5 (v1 ledger), M9 (≥5 µarchs + representation study), M10 (coverage gate). Survey authority: §7.1, §7.3, §7.4, §7.5; OA §13/§14 (ledger as citable artifact; success criteria).
