# The Quiver Performance Ledger

Quiver's second product (Charter §1): a versioned, machine-readable, reproducible record of
what the kernels cost per (variant, microarchitecture, input configuration). Data model and
policies: PRD [11](../docs/prd/11-performance-ledger.md); methodology QLM-1:
[docs/benchmarks/methodology.md](../docs/benchmarks/methodology.md); disputes:
[docs/guides/disputes.md](../docs/guides/disputes.md).

## Layout (Surface D)

| Path | Contents |
|---|---|
| `schema/` | QLS-1 JSON Schemas for entries and run manifests (normative; versioned independently of the library) |
| `runner/quiver_ledger.py` | the stdlib-only runner: environment checklist → shuffled fresh-process repetitions → bootstrap aggregation → append-only results |
| `runner/qledger/` | runner modules (statistics per ADR-020, schema checks, environment capture, GB driver) |
| `runner/tests/` | golden statistics tests + schema accept/reject fixtures (run in ctest) |
| `machines/` | the machine registry — runs execute **only** on registered machines (REQ-LEDGER-007); CI runners are prohibited as ledger sources |
| `results/<uarch>/<yyyymmdd>-<shortsha>/` | committed runs: `entries.json` (aggregates), `raw/` (per-repetition GB output, retained), `manifest.json` — append-only, corrections supersede, history is never rewritten |

## Reading results

Entries are referenced from docs by `entry_id` (checked at docs build, REQ-LEDGER-015).
Statistics per entry: median and min with seeded percentile-bootstrap 95% CIs and CV
(ADR-020; CI pair reported for the median estimator). Flags: `noisy` (CV in the 3–5% band —
published with an explanation; above 5% the entry is excluded until rerun), `no_pmu`,
`secondary_platform` (Apple Silicon entries carry both and omit `cycles_per_value`,
REQ-LEDGER-008).

## Coverage — read this before citing numbers

The ledger never claims coverage it does not have (Charter T7). The registry currently
holds **one** machine (Apple M2, a secondary platform). The REQ-LEDGER-012 v0.3 gate calls
for three microarchitectures (Zen 4/5, Golden-Cove-class, Apple M-series); the missing x86
machines are an **open, recorded deferral** — see `docs/releases/gates/M5.md`. x86 numbers
will appear only when measured on registered x86 hardware; none are invented in the interim.
