// K9 benchmarks. Hypothesis (PRD 08 §4): bandwidth-bound at large n; the auto-vectorizer is
// competitive here — expected T7 verdict territory (parity is the honest prediction).
// Axes: op × type × batch.
// Module: MOD-BENCH / MOD-K9 | REQs: REQ-BENCH-002..004
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
constexpr std::uint64_t kSeed = 0xBE5CB009ull;

template <bool kAutovec>
void bm_add_i64(benchmark::State& state) {
  const auto n = static_cast<std::int64_t>(state.range(0));
  quiver::bench::Rng rng(kSeed);
  std::vector<std::int64_t> a(static_cast<std::size_t>(n));
  std::vector<std::int64_t> b(static_cast<std::size_t>(n));
  quiver::bench::fill_uniform(rng, a.data(), n);
  quiver::bench::fill_uniform(rng, b.data(), n);
  std::vector<std::int64_t> out(static_cast<std::size_t>(n));

  const auto run = [&]() {
#if defined(QUIVER_BENCH_HAVE_AUTOVEC_AVX2)
    if constexpr (kAutovec) {
      quiver::bench::autovec_avx2::arith_i64(quiver::ArithOp::kAdd, a.data(), b.data(), n,
                                             out.data());
      return;
    }
#endif
    quiver::arith(quiver::ArithOp::kAdd, quiver::BatchView<std::int64_t>{a.data(), n},
                  quiver::BatchView<std::int64_t>{b.data(), n}, out.data());
  };
  run();
  bool ok = true;
  for (std::int64_t i = 0; i < n; ++i) {  // wrapping recompute
    const auto want =
        static_cast<std::int64_t>(static_cast<std::uint64_t>(a[static_cast<std::size_t>(i)]) +
                                  static_cast<std::uint64_t>(b[static_cast<std::size_t>(i)]));
    ok = ok && out[static_cast<std::size_t>(i)] == want;
  }
  quiver::bench::validate_or_abort("BM_arith", ok, "wrap-add vs independent recompute");

  g_pmu.start();
  for (auto _ : state) {
    run();
    benchmark::DoNotOptimize(out.data());
  }
  attach_pmu(state, g_pmu.stop_and_read(), n);
}

void bm_mul_f64(benchmark::State& state) {
  const auto n = static_cast<std::int64_t>(state.range(0));
  quiver::bench::Rng rng(kSeed);
  std::vector<double> a(static_cast<std::size_t>(n));
  std::vector<double> b(static_cast<std::size_t>(n));
  quiver::bench::fill_uniform(rng, a.data(), n);
  quiver::bench::fill_uniform(rng, b.data(), n);
  std::vector<double> out(static_cast<std::size_t>(n));
  quiver::arith(quiver::ArithOp::kMul, quiver::BatchView<double>{a.data(), n},
                quiver::BatchView<double>{b.data(), n}, out.data());
  bool ok = true;
  for (std::int64_t i = 0; i < n; ++i) {
    ok = ok && out[static_cast<std::size_t>(i)] ==
                   a[static_cast<std::size_t>(i)] * b[static_cast<std::size_t>(i)];
  }
  quiver::bench::validate_or_abort("BM_arith_f64", ok, "IEEE mul vs independent recompute");
  g_pmu.start();
  for (auto _ : state) {
    quiver::arith(quiver::ArithOp::kMul, quiver::BatchView<double>{a.data(), n},
                  quiver::BatchView<double>{b.data(), n}, out.data());
    benchmark::DoNotOptimize(out.data());
  }
  attach_pmu(state, g_pmu.stop_and_read(), n);
}

void register_benchmarks() {
  const char* variant = quiver::bench::variant_name(quiver::active_isa());
  for (const std::int64_t n : {4096, 65536}) {
    benchmark::RegisterBenchmark(
        quiver::bench::bench_name("arith", "add", variant, "i64", "n=" + std::to_string(n)),
        bm_add_i64<false>)
        ->Args({n});
    benchmark::RegisterBenchmark(
        quiver::bench::bench_name("arith", "mul", variant, "f64", "n=" + std::to_string(n)),
        bm_mul_f64)
        ->Args({n});
  }
#if defined(QUIVER_BENCH_HAVE_AUTOVEC_AVX2)
  // Equal-ISA autovec baseline variants (ADR-011; verdict pair for `avx2`, REQ-BENCH-002).
  if (quiver::cpu_supports(quiver::Isa::kAvx2)) {
    for (const std::int64_t n : {4096, 65536}) {
      benchmark::RegisterBenchmark(quiver::bench::bench_name("arith", "add", "autovec-avx2", "i64",
                                                             "n=" + std::to_string(n)),
                                   bm_add_i64<true>)
          ->Args({n});
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
