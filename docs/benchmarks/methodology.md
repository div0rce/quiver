# Benchmark methodology — QLM-1

The Quiver Ledger Methodology, version **QLM-1**, complete as of v0.3: the in-process measurement layer (§1–§3) and the ledger runner, statistics, and publication workflow (§4) are all live. Normative source: PRD [10](../prd/10-benchmark-architecture.md) and [11](../prd/11-performance-ledger.md); any change to axes, statistics, or orchestration bumps the QLM version (REQ-LEDGER-014).

## 1. Principles

- Every benchmark answers a stated engineering question (REQ-BENCH-003); benchmarks without a hypothesis are prohibited.
- **Validation before timing** (REQ-BENCH-004): every benchmark validates its kernel's output before the first timed iteration — against the scalar reference, or for float reductions against the bench-local ADR-013 policy oracle. A mismatch aborts the binary; no timing output exists for incorrect code (Survey §7.5).
- **CI never gates on timing** (REQ-BENCH-011): shared-runner noise makes CI timing meaningless (Survey §7.3). CI verifies benchmarks build, run, and validate; numbers come only from registered ledger machines (M5+).
- No performance claim without reproducible methodology (Charter T2); losses are published with the same prominence as wins (Charter T7, REQ-LEDGER-011).

## 2. Naming and variants (REQ-BENCH-002)

`BM_<family>/<api>/<variant>/<type>/<axis>=<value>/...` — names are the ledger's join key. Variants: `scalar`, per-ISA (`neon`, `avx2`, `avx512`), and the equal-ISA auto-vectorized baselines (`autovec` on ARM; `autovec-avx2`/`autovec-avx512` on x86 — ADR-011; on x86 the baseline-ISA autovec build is byte-identical to `scalar` and reported once, as `scalar`). Verdict pairs are fixed: (`avx2` vs `autovec-avx2`), (`avx512` vs `autovec-avx512`), (`neon` vs `autovec`).

## 3. Axes (QLM-1, PRD 11 §4)

batch {256, 1024, 4096, 16384, 65536} · selectivity {1, 10, 50, 90, 99}% · pattern {uniform, clustered(geometric runs, mean 64)} · null density {0, 1, 10, 50}% · value distribution {sequential, uniform_random, zipf(θ=1.0, 1000 values)} · alignment {aligned64, offset1} · plus family-specific axes (dict size, bit width, overflow density). Input generation is seeded and identical between the testkit and the bench harness (drift-checked, REQ-BENCH-015).

## 4. Statistics and publication (live from v0.3)

≥10 process-level repetitions, each a **fresh process**, run order shuffled per repetition with a recorded seed (REQ-LEDGER-006 interleaving defense); median + min with seeded percentile-bootstrap 95% CIs (B=10,000, ADR-020 — the entry's CI pair reports the **median** estimator; the min is a point estimate); CV screening (>5% excluded until rerun, 3–5% published with the `noisy` flag and a note); environment manifests per run with an empty-`deviations` requirement for publishability (REQ-LEDGER-013); append-only results under `ledger/results/` (ADR-021). Registered machines only — never CI runners.

Run it: `python3 ledger/runner/quiver_ledger.py run --machine <id> [--filter <regex>]` from a clean checkout with the bench preset built. Variant selection is per-process via the `QUIVER_ISA` env cap; on ARM the scalar cap registers under the `autovec` name (the portable scalar build IS the NEON-baseline autovec variant, REQ-BENCH-010). Validation: `quiver_ledger.py validate` structurally checks every committed run against QLS-1 and runs in ctest.

Docs reference entries as `` `qle:<entry_id>` `` inline code; the repo lint verifies every referenced id exists in a committed `entries.json` (REQ-LEDGER-015).

## 5. PMU collection (ADR-022)

Linux `perf_event_open`, one non-multiplexed group {cycles, instructions, branches, branch-misses}; fail-and-drop on any open failure — benchmarks proceed and their output is marked `pmu: unavailable` (REQ-BENCH-005). Apple entries ship without PMU columns and are labeled secondary (Charter §6.4). Note: virtualized CI runners generally expose no PMU; the degrade path is what CI exercises.

## 6. Regression policy (active from the first tagged release)

Per release: run the tagged regression subset on registered machines; median regressions >3% with non-overlapping CIs block the release until explained (PRD [11 §9](../prd/11-performance-ledger.md)).
