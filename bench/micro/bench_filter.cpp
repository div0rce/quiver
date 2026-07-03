// K2 benchmarks. Hypothesis: bitmap-driven compaction cost is flat in selectivity
// (branch-free unconditional-store core, REQ-KERNEL-003); the selvec-driven form is
// O(sel.len). Axes: selectivity × pattern × batch (QLM-1).
// Module: MOD-BENCH / MOD-K2 | REQs: REQ-BENCH-002..004, REQ-KERNEL-003
#include <cstdint>
#include <string>
#include <vector>

#include <benchmark/benchmark.h>

#include "bench/harness/bench_common.h"
#include "bench/harness/distributions.h"
#include "bench/harness/meta.h"
#include "bench/harness/pmu.h"
#include "quiver/quiver.h"

namespace {

quiver::bench::PmuGroup g_pmu;
constexpr std::uint64_t kSeed = 0xBE5CB001ull;

void attach_pmu(benchmark::State& state, const quiver::bench::PmuCounters& c,
                std::int64_t values) {
  state.SetItemsProcessed(state.iterations() * values);
  if (c.valid) {
    const double total = static_cast<double>(state.iterations()) * static_cast<double>(values);
    state.counters["cycles_per_value"] = static_cast<double>(c.cycles) / total;
    state.counters["ipc"] =
        c.cycles > 0 ? static_cast<double>(c.instructions) / static_cast<double>(c.cycles) : 0.0;
    state.counters["branch_miss_pct"] =
        c.branches > 0
            ? 100.0 * static_cast<double>(c.branch_misses) / static_cast<double>(c.branches)
            : 0.0;
  }
}


void bm_filter_bitmap(benchmark::State& state) {
  const auto n = static_cast<std::int64_t>(state.range(0));
  const int sel_pct = static_cast<int>(state.range(1));
  const bool clustered = state.range(2) != 0;
  quiver::bench::Rng rng(kSeed);
  std::vector<std::int64_t> v(static_cast<std::size_t>(n));
  quiver::bench::fill_uniform(rng, v.data(), n);
  std::vector<std::uint8_t> sel(static_cast<std::size_t>((n + 7) / 8));
  if (clustered) {
    quiver::bench::fill_bitmap_clustered(rng, sel.data(), n, sel_pct);
  } else {
    quiver::bench::fill_bitmap_uniform(rng, sel.data(), n, sel_pct);
  }
  std::vector<std::int64_t> out(static_cast<std::size_t>(n));

  const std::int64_t got = quiver::filter(quiver::BatchView<std::int64_t>{v.data(), n},
                                          quiver::BitmapView{sel.data()}, out.data());
  std::int64_t want = 0;
  std::int64_t checksum_want = 0;
  for (std::int64_t i = 0; i < n; ++i) {
    if ((sel[static_cast<std::size_t>(i) >> 3] >> (i & 7)) & 1u) {
      ++want;
      checksum_want += v[static_cast<std::size_t>(i)];
    }
  }
  std::int64_t checksum_got = 0;
  for (std::int64_t j = 0; j < got; ++j) {
    checksum_got += out[static_cast<std::size_t>(j)];
  }
  quiver::bench::validate_or_abort("BM_filter", got == want && checksum_got == checksum_want,
                                   "count/checksum vs independent recompute");

  g_pmu.start();
  for (auto _ : state) {
    benchmark::DoNotOptimize(quiver::filter(quiver::BatchView<std::int64_t>{v.data(), n},
                                            quiver::BitmapView{sel.data()}, out.data()));
  }
  attach_pmu(state, g_pmu.stop_and_read(), n);
}

void register_benchmarks() {
  const char* variant = quiver::active_isa() == quiver::Isa::kScalar ? "scalar"
                        : quiver::active_isa() == quiver::Isa::kNeon ? "neon"
                        : quiver::active_isa() == quiver::Isa::kAvx2 ? "avx2"
                                                                     : "avx512";
  for (const std::int64_t n : {4096, 65536}) {
    for (const int pct : {1, 10, 50, 90, 99}) {
      for (const int clustered : {0, 1}) {
        benchmark::RegisterBenchmark(
            quiver::bench::bench_name("filter", "bitmap", variant, "i64",
                                      "n=" + std::to_string(n) + "/sel=" + std::to_string(pct) +
                                          (clustered ? "/pat=clustered" : "/pat=uniform")),
            bm_filter_bitmap)
            ->Args({n, pct, clustered});
      }
    }
  }
}

}  // namespace

int main(int argc, char** argv) {
  const bool pmu = g_pmu.open();
  quiver::bench::add_run_context(pmu);
  register_benchmarks();
  benchmark::Initialize(&argc, argv);
  if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
    return 1;
  }
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
  return 0;
}
