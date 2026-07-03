// K1 benchmarks. Hypothesis (REQ-BENCH-003): compare cost is flat in selectivity — the
// branch-free core makes cost data-independent (REQ-KERNEL-003; Survey §3.4). Axes:
// selectivity × pattern × batch (QLM-1); variant = active ISA (scalar at M3; ISA variants
// register per cpu_supports as backends land).
// Validation (REQ-BENCH-004): output vs an independent per-element recompute, pre-timing.
// Module: MOD-BENCH / MOD-K1 | REQs: REQ-BENCH-002..004, REQ-KERNEL-003
#include <cstdint>
#include <string>
#include <vector>

#include <benchmark/benchmark.h>

#include "bench/baselines/baseline_avx2.h"
#include "bench/harness/bench_common.h"
#include "bench/harness/distributions.h"
#include "bench/harness/meta.h"
#include "bench/harness/pmu.h"
#include "quiver/quiver.h"

namespace {

quiver::bench::PmuGroup g_pmu;
constexpr std::uint64_t kSeed = 0xBE5CB001ull;

void attach_pmu(benchmark::State& state, const quiver::bench::PmuCounters& c, std::int64_t values) {
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

template <bool kAutovec>
void bm_compare_bitmap(benchmark::State& state) {
  const auto n = static_cast<std::int64_t>(state.range(0));
  const int sel_pct = static_cast<int>(state.range(1));
  quiver::bench::Rng rng(kSeed);
  std::vector<std::int64_t> v(static_cast<std::size_t>(n));
  quiver::bench::fill_uniform(rng, v.data(), n);
  // Comparand at the requested selectivity percentile of the uniform range.
  const std::int64_t comparand = static_cast<std::int64_t>(
      static_cast<std::uint64_t>(~0ull) / 100 * (100 - sel_pct) - (~0ull >> 1));
  std::vector<std::uint8_t> bits(static_cast<std::size_t>((n + 7) / 8));

  const auto run = [&]() {
#if defined(QUIVER_BENCH_HAVE_AUTOVEC_AVX2)
    if constexpr (kAutovec) {
      return quiver::bench::autovec_avx2::compare_bitmap_i64(quiver::CompareOp::kGt, v.data(), n,
                                                             comparand, nullptr, bits.data());
    }
#endif
    return quiver::compare_bitmap(quiver::CompareOp::kGt,
                                  quiver::BatchView<std::int64_t>{v.data(), n}, comparand,
                                  quiver::BitmapView{nullptr}, bits.data());
  };
  const std::int64_t got = run();
  std::int64_t want = 0;  // independent recompute (REQ-BENCH-004)
  for (std::int64_t i = n - 1; i >= 0; --i) {
    want += v[static_cast<std::size_t>(i)] > comparand ? 1 : 0;
  }
  quiver::bench::validate_or_abort("BM_compare", got == want, "count vs independent recompute");

  g_pmu.start();
  for (auto _ : state) {
    benchmark::DoNotOptimize(run());
  }
  attach_pmu(state, g_pmu.stop_and_read(), n);
}

void register_benchmarks() {
  const char* variant = quiver::bench::variant_name(quiver::active_isa());
  for (const std::int64_t n : {1024, 4096, 65536}) {
    for (const int pct : {1, 10, 50, 90, 99}) {
      benchmark::RegisterBenchmark(
          quiver::bench::bench_name("compare", "bitmap_gt", variant, "i64",
                                    "n=" + std::to_string(n) + "/sel=" + std::to_string(pct)),
          bm_compare_bitmap<false>)
          ->Args({n, pct});
    }
  }
#if defined(QUIVER_BENCH_HAVE_AUTOVEC_AVX2)
  // Equal-ISA autovec baseline variants (ADR-011; verdict pair for `avx2`, REQ-BENCH-002).
  if (quiver::cpu_supports(quiver::Isa::kAvx2)) {
    for (const std::int64_t n : {1024, 4096, 65536}) {
      for (const int pct : {1, 10, 50, 90, 99}) {
        benchmark::RegisterBenchmark(
            quiver::bench::bench_name("compare", "bitmap_gt", "autovec-avx2", "i64",
                                      "n=" + std::to_string(n) + "/sel=" + std::to_string(pct)),
            bm_compare_bitmap<true>)
            ->Args({n, pct});
      }
    }
  }
#endif
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
