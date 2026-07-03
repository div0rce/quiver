// K3 benchmarks — the M9 representation-study instrument. Hypothesis: conversion cost per
// value depends on density (bitmap->selvec) and is near-constant for selvec->bitmap.
// Module: MOD-BENCH / MOD-K3 | REQs: REQ-BENCH-002..004
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


void bm_bitmap_to_selvec(benchmark::State& state) {
  const auto n = static_cast<std::int64_t>(state.range(0));
  const int pct = static_cast<int>(state.range(1));
  quiver::bench::Rng rng(kSeed);
  std::vector<std::uint8_t> bits(static_cast<std::size_t>((n + 7) / 8));
  quiver::bench::fill_bitmap_uniform(rng, bits.data(), n, pct);
  std::vector<std::uint32_t> idx(static_cast<std::size_t>(n));

  const std::int64_t got = quiver::bitmap_to_selvec(quiver::BitmapView{bits.data()}, n,
                                                    idx.data());
  std::int64_t want = 0;
  for (std::int64_t i = 0; i < n; ++i) {
    want += (bits[static_cast<std::size_t>(i) >> 3] >> (i & 7)) & 1u;
  }
  quiver::bench::validate_or_abort("BM_select", got == want, "count vs independent popcount");

  g_pmu.start();
  for (auto _ : state) {
    benchmark::DoNotOptimize(
        quiver::bitmap_to_selvec(quiver::BitmapView{bits.data()}, n, idx.data()));
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
      benchmark::RegisterBenchmark(
          quiver::bench::bench_name("select", "bitmap_to_selvec", variant, "u32",
                                    "n=" + std::to_string(n) + "/density=" + std::to_string(pct)),
          bm_bitmap_to_selvec)
          ->Args({n, pct});
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
