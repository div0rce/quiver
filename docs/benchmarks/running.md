# Running benchmarks

## Build and run

```sh
cmake --preset bench && cmake --build --preset bench -j
./build/bench/bin/quiver_bench_smoke                 # harness smoke (M2)
./build/bench/bin/quiver_bench_smoke --benchmark_filter=...   # standard GB flags apply
```

Benchmark binaries land in `build/bench/bin/`. Family benchmarks (`quiver_bench_<family>`) arrive with their kernels (M3+). The end-to-end pipeline benchmark `quiver_bench_pipeline` composes the demo-layer query chain compare → select → take → reduce over synthetic data (composition sanity, not a cross-engine comparison — REQ-BENCH-012).

## Environment preparation (REQ-BENCH-013; Survey §7.3)

Numbers intended for comparison — and everything ledger-bound — require a controlled machine (never a shared/virtualized runner):

1. Performance governor: `cpupower frequency-set -g performance` (Linux).
2. Disable turbo/boost for stability (`intel_pstate/no_turbo`, `boost` sysfs) — record the state either way.
3. Note SMT state; avoid placing other load on the sibling.
4. ASLR: for A/B comparisons on one machine, disabling (`randomize_va_space=0`) reduces variance; the ledger's cross-run defense is **randomized interleaving of repetitions** — both schools, per Survey §7.3: fixed layout for A/B *plus* interleaving to sample layout bias.
5. Quiesce background services; prefer a wired, idle machine.
6. Record everything you touched — the M5 ledger runner captures this manifest automatically and rejects publishable runs with unexplained deviations (REQ-LEDGER-013).

macOS note: no public PMU access; wall-clock only; entries are labeled secondary (Survey §7.3, Charter §6.4).

## PMU counters

On Linux with PMU access (`kernel.perf_event_paranoid ≤ 2` or CAP_PERFMON), benchmarks report `cycles_per_value`, `ipc`, and `branch_miss_pct` per REQ-BENCH-005; without access they run and mark `pmu: unavailable`.

## Flamegraphs (REQ-BENCH-014)

```sh
bench/harness/flamegraph.sh /tmp/investigation build/bench/bin/quiver_bench_smoke --benchmark_min_time=1s
```

Collapsed stacks + a question/environment/conclusion note belong under `docs/benchmarks/investigations/<topic>/`. Flamegraphs support investigations; they never replace ledger numbers.
