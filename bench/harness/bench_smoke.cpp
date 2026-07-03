// Harness smoke benchmark (PRD 18 M2 "Benchmarks required"): proves the Google Benchmark +
// PMU + naming + validation pipeline end-to-end with a placeholder checksum loop.
// Hypothesis (REQ-BENCH-003): the harness measures a known loop and reports sane counters —
// this benchmark validates infrastructure, not kernels (which do not exist until M3).
// The --quiver_validate_fail flag deliberately corrupts the expected value to demonstrate
// the REQ-BENCH-004 abort path (gate evidence; exercised negatively by the CI bench-smoke job).
// Module: MOD-BENCH | REQs: REQ-BENCH-001..005, REQ-INT-003 | ADR-008, ADR-022
#include <cstdint>
#include <cstring>
#include <vector>

#include <benchmark/benchmark.h>

#include "bench/harness/bench_common.h"
#include "bench/harness/distributions.h"
#include "bench/harness/meta.h"
#include "bench/harness/pmu.h"

namespace {

bool g_force_validation_failure = false;
quiver::bench::PmuGroup g_pmu;

std::uint64_t checksum(const std::uint64_t* v, std::int64_t n) {
  std::uint64_t acc = 0;
  for (std::int64_t i = 0; i < n; ++i) {
    acc += v[i];
  }
  return acc;
}

void bm_smoke_checksum(benchmark::State& state) {
  constexpr std::int64_t kN = 4096;
  quiver::bench::Rng rng(0x51E5EEDBA5E0001ull);
  std::vector<std::uint64_t> data(kN);
  quiver::bench::fill_uniform(rng, data.data(), kN);

  // Pre-timing validation (REQ-BENCH-004): recompute independently and compare; excluded
  // from the timed region; touched data doubles as first-touch warmup (Survey §7.5).
  std::uint64_t expected = 0;
  for (std::int64_t i = kN - 1; i >= 0; --i) {
    expected += data[static_cast<std::size_t>(i)];  // reverse-order independent recompute
  }
  if (g_force_validation_failure) {
    expected ^= 0xDEADBEEFull;  // deliberate corruption: must abort, never emit timing
  }
  quiver::bench::validate_or_abort("BM_smoke/checksum/scalar/u64/n=4096",
                                   checksum(data.data(), kN) == expected,
                                   "checksum mismatch vs independent recompute");

  g_pmu.start();
  for (auto _ : state) {
    std::uint64_t acc = checksum(data.data(), kN);
    benchmark::DoNotOptimize(acc);
  }
  const quiver::bench::PmuCounters c = g_pmu.stop_and_read();
  state.SetItemsProcessed(state.iterations() * kN);
  if (c.valid) {
    const double iters = static_cast<double>(state.iterations());
    state.counters["cycles_per_value"] =
        static_cast<double>(c.cycles) / (iters * static_cast<double>(kN));
    state.counters["ipc"] =
        c.cycles > 0 ? static_cast<double>(c.instructions) / static_cast<double>(c.cycles) : 0.0;
    state.counters["branch_miss_pct"] =
        c.branches > 0
            ? 100.0 * static_cast<double>(c.branch_misses) / static_cast<double>(c.branches)
            : 0.0;
  }
}

}  // namespace

int main(int argc, char** argv) {
  // Harness-specific flags are consumed before Google Benchmark parses the rest.
  int kept = 1;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--quiver_validate_fail") == 0) {
      g_force_validation_failure = true;
    } else {
      argv[kept++] = argv[i];
    }
  }
  argc = kept;

  const bool pmu = g_pmu.open();
  quiver::bench::add_run_context(pmu);
  benchmark::RegisterBenchmark("BM_smoke/checksum/scalar/u64/n=4096", bm_smoke_checksum);
  benchmark::Initialize(&argc, argv);
  if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
    return 1;
  }
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
  return 0;
}
