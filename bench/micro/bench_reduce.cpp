// K6 benchmarks. Hypotheses: (a) integer sums are bandwidth-bound at large n; (b) the
// scalar float sum is latency-bound BY DESIGN (strict left fold — the charter's strict-order
// recourse, ADR-013 scalar A=1); SIMD variants with A=4 blocked accumulators quantify the
// reassociation win at M4+. Axes: null density × selection × batch.
// Module: MOD-BENCH / MOD-K6 | REQs: REQ-BENCH-002..004; ADR-013 evidence
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


void bm_sum_i64(benchmark::State& state) {
  const auto n = static_cast<std::int64_t>(state.range(0));
  const int null_pct = static_cast<int>(state.range(1));
  quiver::bench::Rng rng(kSeed);
  std::vector<std::int64_t> v(static_cast<std::size_t>(n));
  quiver::bench::fill_uniform(rng, v.data(), n);
  std::vector<std::uint8_t> validity(static_cast<std::size_t>((n + 7) / 8));
  quiver::bench::fill_bitmap_uniform(rng, validity.data(), n, 100 - null_pct);
  const std::uint8_t* vd = null_pct == 0 ? nullptr : validity.data();

  const std::int64_t got = quiver::reduce_sum_wrap(quiver::BatchView<std::int64_t>{v.data(), n},
                                                   quiver::BitmapView{vd});
  std::uint64_t want = 0;
  for (std::int64_t i = 0; i < n; ++i) {
    const bool ok = vd == nullptr || ((vd[static_cast<std::size_t>(i) >> 3] >> (i & 7)) & 1u);
    if (ok) {
      want += static_cast<std::uint64_t>(v[static_cast<std::size_t>(i)]);
    }
  }
  quiver::bench::validate_or_abort("BM_reduce", static_cast<std::uint64_t>(got) == want,
                                   "sum vs independent recompute");

  g_pmu.start();
  for (auto _ : state) {
    benchmark::DoNotOptimize(quiver::reduce_sum_wrap(
        quiver::BatchView<std::int64_t>{v.data(), n}, quiver::BitmapView{vd}));
  }
  attach_pmu(state, g_pmu.stop_and_read(), n);
}

void bm_sum_f64(benchmark::State& state) {
  const auto n = static_cast<std::int64_t>(state.range(0));
  quiver::bench::Rng rng(kSeed);
  std::vector<double> v(static_cast<std::size_t>(n));
  quiver::bench::fill_uniform(rng, v.data(), n);
  double want = 0.0;  // the strict fold IS the scalar spec — same order recompute
  for (std::int64_t i = 0; i < n; ++i) {
    want += v[static_cast<std::size_t>(i)];
  }
  const double got =
      quiver::reduce_sum_wrap(quiver::BatchView<double>{v.data(), n}, quiver::BitmapView{nullptr});
  quiver::bench::validate_or_abort("BM_reduce_f64", got == want,
                                   "strict-fold sum vs same-order recompute");
  g_pmu.start();
  for (auto _ : state) {
    benchmark::DoNotOptimize(quiver::reduce_sum_wrap(quiver::BatchView<double>{v.data(), n},
                                                     quiver::BitmapView{nullptr}));
  }
  attach_pmu(state, g_pmu.stop_and_read(), n);
}

void register_benchmarks() {
  const char* variant = quiver::active_isa() == quiver::Isa::kScalar ? "scalar"
                        : quiver::active_isa() == quiver::Isa::kNeon ? "neon"
                        : quiver::active_isa() == quiver::Isa::kAvx2 ? "avx2"
                                                                     : "avx512";
  for (const std::int64_t n : {4096, 65536}) {
    for (const int null_pct : {0, 10, 50}) {
      benchmark::RegisterBenchmark(
          quiver::bench::bench_name("reduce", "sum_wrap", variant, "i64",
                                    "n=" + std::to_string(n) + "/nulls=" +
                                        std::to_string(null_pct)),
          bm_sum_i64)
          ->Args({n, null_pct});
    }
    benchmark::RegisterBenchmark(
        quiver::bench::bench_name("reduce", "sum_wrap", variant, "f64",
                                  "n=" + std::to_string(n) + "/nulls=0"),
        bm_sum_f64)
        ->Args({n});
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
