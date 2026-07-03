// Dispatch-overhead benchmarks (REQ-DISP-003 evidence; ADR-004 reconsideration trigger:
// epoch check costing > 1% on a 4K-element kernel invocation). Hypotheses: (a) dispatched
// vs direct-symbol call difference is amortized to noise at n=4096; (b) it is measurable at
// n=1 (the per-call floor); (c) warmup eliminates first-call latency.
// Module: MOD-BENCH / MOD-DISPATCH | REQs: REQ-BENCH-003, REQ-DISP-003
#include <cstdint>
#include <string>
#include <vector>

#include <benchmark/benchmark.h>

#include "bench/harness/bench_common.h"
#include "bench/harness/distributions.h"
#include "bench/harness/meta.h"
#include "bench/harness/pmu.h"
#include "quiver/quiver.h"

// Direct-symbol declaration of one scalar backend (bypasses dispatch): the comparison
// baseline for the overhead question. Internal symbol — usage documented as bench-only.
QUIVER_BEGIN_NAMESPACE
namespace detail::scalar {
std::int64_t k1_compare_bitmap(CompareOp op, const std::int64_t* in, std::int64_t n,
                               std::int64_t comparand, const std::uint8_t* validity,
                               std::uint8_t* out) noexcept;
}
QUIVER_END_NAMESPACE

namespace {

quiver::bench::PmuGroup g_pmu;

template <bool kDispatched>
void bm_call_path(benchmark::State& state) {
  const auto n = static_cast<std::int64_t>(state.range(0));
  quiver::bench::Rng rng(0xD15BA7C4ull);
  std::vector<std::int64_t> v(static_cast<std::size_t>(n));
  quiver::bench::fill_uniform(rng, v.data(), n);
  std::vector<std::uint8_t> bits(static_cast<std::size_t>((n + 7) / 8));
  quiver::warmup();

  const std::int64_t a = quiver::compare_bitmap(
      quiver::CompareOp::kGt, quiver::BatchView<std::int64_t>{v.data(), n}, std::int64_t{0},
      quiver::BitmapView{nullptr}, bits.data());
  const std::int64_t b = quiver::detail::scalar::k1_compare_bitmap(
      quiver::CompareOp::kGt, v.data(), n, 0, nullptr, bits.data());
  quiver::bench::validate_or_abort("BM_dispatch", a == b, "dispatched vs direct disagree");

  for (auto _ : state) {
    if constexpr (kDispatched) {
      benchmark::DoNotOptimize(quiver::compare_bitmap(
          quiver::CompareOp::kGt, quiver::BatchView<std::int64_t>{v.data(), n}, std::int64_t{0},
          quiver::BitmapView{nullptr}, bits.data()));
    } else {
      benchmark::DoNotOptimize(quiver::detail::scalar::k1_compare_bitmap(
          quiver::CompareOp::kGt, v.data(), n, 0, nullptr, bits.data()));
    }
  }
  state.SetItemsProcessed(state.iterations() * n);
}

void bm_warmup_cost(benchmark::State& state) {
  for (auto _ : state) {
    quiver::warmup();  // idempotent steady-state cost (first-call resolution measured once)
  }
}

void register_benchmarks() {
  for (const std::int64_t n : {1, 64, 4096}) {
    benchmark::RegisterBenchmark(
        quiver::bench::bench_name("dispatch", "compare_gt", "dispatched", "i64",
                                  "n=" + std::to_string(n)),
        bm_call_path<true>)
        ->Args({n});
    benchmark::RegisterBenchmark(
        quiver::bench::bench_name("dispatch", "compare_gt", "direct", "i64",
                                  "n=" + std::to_string(n)),
        bm_call_path<false>)
        ->Args({n});
  }
  benchmark::RegisterBenchmark("BM_dispatch/warmup/steady/none/n=1", bm_warmup_cost);
}

}  // namespace

int main(int argc, char** argv) {
  const bool pmu = g_pmu.open();
  (void)g_pmu;
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
