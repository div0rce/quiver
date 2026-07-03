// K4 benchmarks. Hypothesis (a designated T7 candidate): the auto-vectorized word loop is
// already at bandwidth — explicit SIMD is expected to show little or no gain here; the
// verdict is published either way (REQ-LEDGER-011; Survey §4.4).
// Module: MOD-BENCH / MOD-K4 | REQs: REQ-BENCH-002..004
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


void bm_mask_and(benchmark::State& state) {
  const auto n = static_cast<std::int64_t>(state.range(0));
  quiver::bench::Rng rng(kSeed);
  const std::size_t bytes = (static_cast<std::size_t>(n) + 7) / 8;
  std::vector<std::uint8_t> a(bytes);
  std::vector<std::uint8_t> b(bytes);
  quiver::bench::fill_bitmap_uniform(rng, a.data(), n, 50);
  quiver::bench::fill_bitmap_uniform(rng, b.data(), n, 50);
  std::vector<std::uint8_t> out(bytes);

  quiver::mask_combine(quiver::MaskOp::kAnd, quiver::BitmapView{a.data()},
                       quiver::BitmapView{b.data()}, n, out.data());
  bool ok = true;  // independent per-bit recompute (REQ-BENCH-004)
  for (std::int64_t i = 0; i < n; ++i) {
    const bool av = (a[static_cast<std::size_t>(i) >> 3] >> (i & 7)) & 1u;
    const bool bv = (b[static_cast<std::size_t>(i) >> 3] >> (i & 7)) & 1u;
    const bool ov = (out[static_cast<std::size_t>(i) >> 3] >> (i & 7)) & 1u;
    ok = ok && (ov == (av && bv));
  }
  quiver::bench::validate_or_abort("BM_mask", ok, "AND vs per-bit recompute");

  g_pmu.start();
  for (auto _ : state) {
    quiver::mask_combine(quiver::MaskOp::kAnd, quiver::BitmapView{a.data()},
                         quiver::BitmapView{b.data()}, n, out.data());
    benchmark::DoNotOptimize(out.data());
  }
  attach_pmu(state, g_pmu.stop_and_read(), n);
}

void register_benchmarks() {
  const char* variant = quiver::active_isa() == quiver::Isa::kScalar ? "scalar"
                        : quiver::active_isa() == quiver::Isa::kNeon ? "neon"
                        : quiver::active_isa() == quiver::Isa::kAvx2 ? "avx2"
                                                                     : "avx512";
  for (const std::int64_t n : {4096, 65536, 1 << 20}) {
    benchmark::RegisterBenchmark(quiver::bench::bench_name("mask", "and", variant, "bitmap",
                                                           "n=" + std::to_string(n)),
                                 bm_mask_and)
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
