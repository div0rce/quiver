// End-to-end pipeline benchmark — the Charter §6.6 demo layer (REQ-BENCH-012). Composes the
// canonical query chain K1 -> K3 -> K5 -> K6 over synthetic data: compare into a selection
// bitmap (K1), materialize the matching indices as a selection vector (K3 select — the
// index-producing sibling of K2 filter's value compaction), gather the matched values (K5
// take), and reduce them (K6 sum). This measures composition effects (LUT/cache interplay
// across kernels, risk R-08) as a sanity benchmark — NOT a marketing number: there are no
// cross-engine comparisons here (DBTest 2018 apples-vs-oranges rule, Survey §7.5).
// Validation (REQ-BENCH-004): the pipeline's sum vs an independent single-pass recompute,
// pre-timing. Module: MOD-BENCH | REQs: REQ-BENCH-002..004, REQ-BENCH-012
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
constexpr std::uint64_t kSeed = 0x91DE501C0DE5ull;  // distinct stream from the micro-benches

using Sum = quiver::SumType<std::int32_t>;

// Run the full chain over `v`, returning the reduced sum of the elements that pass `> comparand`.
// The intermediate buffers the chain writes through. They are hoisted out of the timing loop
// and reused, so the benchmark measures the kernels rather than allocation.
struct PipelineScratch {
  std::vector<std::uint8_t>& bits;
  std::vector<std::uint32_t>& sel;
  std::vector<std::int32_t>& gathered;
};

Sum run_pipeline(const std::vector<std::int32_t>& v, std::int32_t comparand,
                 PipelineScratch scratch) {
  std::vector<std::uint8_t>& bits = scratch.bits;
  std::vector<std::uint32_t>& sel = scratch.sel;
  std::vector<std::int32_t>& gathered = scratch.gathered;
  const auto n = static_cast<std::int64_t>(v.size());
  quiver::compare_bitmap(quiver::CompareOp::kGt, quiver::BatchView<std::int32_t>{v.data(), n},
                         comparand, quiver::BitmapView{nullptr}, bits.data());  // K1
  const std::int64_t k = quiver::bitmap_to_selvec(quiver::BitmapView{bits.data()}, n, sel.data());
  quiver::take(quiver::BatchView<std::int32_t>{v.data(), n}, quiver::SelVec{sel.data(), k},
               gathered.data());  // K5
  return quiver::reduce_sum_wrap(quiver::BatchView<std::int32_t>{gathered.data(), k},
                                 quiver::BitmapView{nullptr});  // K6
}

void bm_pipeline_sum_gt(benchmark::State& state) {
  const auto n = static_cast<std::int64_t>(state.range(0));
  const int sel_pct = static_cast<int>(state.range(1));
  quiver::bench::Rng rng(kSeed);
  std::vector<std::int32_t> v(static_cast<std::size_t>(n));
  quiver::bench::fill_uniform(rng, v.data(), n);
  // Comparand at the requested selectivity percentile of the uniform int32 range.
  constexpr std::int64_t kSpan = 1LL << 32;
  const auto comparand = static_cast<std::int32_t>(static_cast<std::int64_t>(-(1LL << 31)) +
                                                   kSpan * (100 - sel_pct) / 100);

  std::vector<std::uint8_t> bits(static_cast<std::size_t>((n + 7) / 8));
  std::vector<std::uint32_t> sel(static_cast<std::size_t>(n));
  std::vector<std::int32_t> gathered(static_cast<std::size_t>(n));

  const Sum got = run_pipeline(v, comparand, {bits, sel, gathered});
  Sum want = 0;  // independent single-pass recompute (REQ-BENCH-004)
  for (std::int64_t i = 0; i < n; ++i) {
    if (v[static_cast<std::size_t>(i)] > comparand) {
      want += v[static_cast<std::size_t>(i)];
    }
  }
  quiver::bench::validate_or_abort("BM_pipeline", got == want,
                                   "reduced sum vs independent recompute");

  g_pmu.start();
  for (auto _ : state) {
    benchmark::DoNotOptimize(run_pipeline(v, comparand, {bits, sel, gathered}));
  }
  attach_pmu(state, g_pmu.stop_and_read(), n);
}

void register_benchmarks() {
  const char* variant = quiver::bench::variant_name(quiver::active_isa());
  for (const std::int64_t n : {1024, 4096, 65536}) {
    for (const int pct : {1, 10, 50, 90, 99}) {
      benchmark::RegisterBenchmark(
          quiver::bench::bench_name({"pipeline", "cmp_select_take_sum", variant, "i32"},
                                    "n=" + std::to_string(n) + "/sel=" + std::to_string(pct)),
          bm_pipeline_sum_gt)
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
