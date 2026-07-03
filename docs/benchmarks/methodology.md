# Benchmark methodology — QLM-1

The Quiver Ledger Methodology, version **QLM-1**. Normative source: PRD [10](../prd/10-benchmark-architecture.md) and [11](../prd/11-performance-ledger.md); any change to axes, statistics, or orchestration bumps the QLM version (REQ-LEDGER-014). **M2 status:** the in-process measurement layer (this page's §1–§3) is live; the ledger runner, statistics, and publication workflow (§4) arrive at M5 and are marked accordingly.

## 1. Principles

- Every benchmark answers a stated engineering question (REQ-BENCH-003); benchmarks without a hypothesis are prohibited.
- **Validation before timing** (REQ-BENCH-004): every benchmark validates its kernel's output before the first timed iteration — against the scalar reference, or for float reductions against the bench-local ADR-013 policy oracle. A mismatch aborts the binary; no timing output exists for incorrect code (Survey §7.5).
- **CI never gates on timing** (REQ-BENCH-011): shared-runner noise makes CI timing meaningless (Survey §7.3). CI verifies benchmarks build, run, and validate; numbers come only from registered ledger machines (M5+).
- No performance claim without reproducible methodology (Charter T2); losses are published with the same prominence as wins (Charter T7, REQ-LEDGER-011).

## 2. Naming and variants (REQ-BENCH-002)

`BM_<family>/<api>/<variant>/<type>/<axis>=<value>/...` — names are the ledger's join key. Variants: `scalar`, per-ISA (`neon`, `avx2`, `avx512`), and the equal-ISA auto-vectorized baselines (`autovec` on ARM; `autovec-avx2`/`autovec-avx512` on x86 — ADR-011; on x86 the baseline-ISA autovec build is byte-identical to `scalar` and reported once, as `scalar`). Verdict pairs are fixed: (`avx2` vs `autovec-avx2`), (`avx512` vs `autovec-avx512`), (`neon` vs `autovec`).

## 3. Axes (QLM-1, PRD 11 §4)

batch {256, 1024, 4096, 16384, 65536} · selectivity {1, 10, 50, 90, 99}% · pattern {uniform, clustered(geometric runs, mean 64)} · null density {0, 1, 10, 50}% · value distribution {sequential, uniform_random, zipf(θ=1.0, 1000 values)} · alignment {aligned64, offset1} · plus family-specific axes (dict size, bit width, overflow density). Input generation is seeded and identical between the testkit and the bench harness (drift-checked, REQ-BENCH-015).

## 4. Statistics and publication (M5; recorded here for completeness)

≥10 process-level repetitions with seeded shuffled interleaving; median + min with seeded percentile-bootstrap 95% CIs (B=10,000); CV screening (>5% excluded, 3–5% flagged); environment manifests per run; append-only results under `ledger/results/`; reproduction via a single documented runner command. Registered machines only — never CI runners.

## 5. PMU collection (ADR-022)

Linux `perf_event_open`, one non-multiplexed group {cycles, instructions, branches, branch-misses}; fail-and-drop on any open failure — benchmarks proceed and their output is marked `pmu: unavailable` (REQ-BENCH-005). Apple entries ship without PMU columns and are labeled secondary (Charter §6.4). Note: virtualized CI runners generally expose no PMU; the degrade path is what CI exercises.

## 6. Regression policy (active from the first tagged release)

Per release: run the tagged regression subset on registered machines; median regressions >3% with non-overlapping CIs block the release until explained (PRD [11 §9](../prd/11-performance-ledger.md)).
