// K6 benchmarks. Hypotheses: (a) integer sums are bandwidth-bound at large n; (b) the
// scalar float sum is latency-bound BY DESIGN (strict left fold — the charter's strict-order
// recourse, ADR-013 scalar A=1); SIMD variants with A=4 blocked accumulators quantify the
// reassociation win at M4+. Axes: null density × selection × batch.
// Module: MOD-BENCH / MOD-K6 | REQs: REQ-BENCH-002..004; ADR-013 evidence
#include <cstdint>
#include <limits>
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

template <bool kAutovec>
void bm_sum_i64(benchmark::State& state) {
  const auto n = static_cast<std::int64_t>(state.range(0));
  const int null_pct = static_cast<int>(state.range(1));
  quiver::bench::Rng rng(kSeed);
  std::vector<std::int64_t> v(static_cast<std::size_t>(n));
  quiver::bench::fill_uniform(rng, v.data(), n);
  std::vector<std::uint8_t> validity(static_cast<std::size_t>((n + 7) / 8));
  quiver::bench::fill_bitmap_uniform(rng, validity.data(), n, 100 - null_pct);
  const std::uint8_t* vd = null_pct == 0 ? nullptr : validity.data();

  const auto run = [&]() {
#if defined(QUIVER_BENCH_HAVE_AUTOVEC_AVX2)
    if constexpr (kAutovec) {
      return quiver::bench::autovec_avx2::sum_wrap_i64(v.data(), n, vd);
    }
#endif
    return quiver::reduce_sum_wrap(quiver::BatchView<std::int64_t>{v.data(), n},
                                   quiver::BitmapView{vd});
  };
  const std::int64_t got = run();
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
    benchmark::DoNotOptimize(run());
  }
  attach_pmu(state, g_pmu.stop_and_read(), n);
}

// Dense min (K6 reduce_min). Hypothesis (REQ-BENCH-003, pre-registered before any measurement):
// the handwritten NEON dense min/max keeps a SINGLE vector accumulator (a latency-bound serial
// chain: i64 = cmgt+bif per block, f64 = fmax), unlike dense_sum's four. Min/max is associative
// and exact under any association, so the autovectorizer CAN reassociate integer min reductions
// without fast-math — the explicit path may therefore TIE or LOSE at i64 despite vectorizing;
// f64 depends on whether the compiler can prove the fmin NaN semantics. Decision rule: a loss
// routes to the scalar reference per REQ-KERNEL-007 (as compare-i64/arith-8-byte did), and
// separately motivates the recorded multi-accumulator rework; a win/tie keeps the current path.
// Result is exact either way, so validation is a plain independent recompute.
template <class T>
void bm_min_t(benchmark::State& state) {
  const auto n = static_cast<std::int64_t>(state.range(0));
  const int null_pct = static_cast<int>(state.range(1));
  quiver::bench::Rng rng(kSeed);
  std::vector<T> v(static_cast<std::size_t>(n));
  quiver::bench::fill_uniform(rng, v.data(), n);
  std::vector<std::uint8_t> validity(static_cast<std::size_t>((n + 7) / 8));
  quiver::bench::fill_bitmap_uniform(rng, validity.data(), n, 100 - null_pct);
  const std::uint8_t* vd = null_pct == 0 ? nullptr : validity.data();

  const auto run = [&]() {
    return quiver::reduce_min(quiver::BatchView<T>{v.data(), n}, quiver::BitmapView{vd});
  };
  const T got = run();
  T want = std::numeric_limits<T>::max();
  for (std::int64_t i = 0; i < n; ++i) {
    const bool ok = vd == nullptr || ((vd[static_cast<std::size_t>(i) >> 3] >> (i & 7)) & 1u);
    if (ok && v[static_cast<std::size_t>(i)] < want) {
      want = v[static_cast<std::size_t>(i)];
    }
  }
  quiver::bench::validate_or_abort("BM_reduce_min", got == want, "min vs independent recompute");

  g_pmu.start();
  for (auto _ : state) {
    benchmark::DoNotOptimize(run());
  }
  attach_pmu(state, g_pmu.stop_and_read(), n);
}

// Same-order recompute of the dense float-sum policy the measured path is DOCUMENTED to use
// (ADR-013): the strict fold for the scalar spec and the autovec baseline; the blocked
// {w, a=4} shape when the dispatched path resolves to AVX2 (w=4 for f64) or NEON (w=2).
// Bench-local on purpose — bench code never links test oracles (REQ-TEST-018).
inline double expected_dense_sum_f64(const double* v, std::int64_t n, int w) {
  if (w == 0) {  // strict fold
    double s = 0.0;
    for (std::int64_t i = 0; i < n; ++i) {
      s += v[i];
    }
    return s;
  }
  constexpr int kA = 4;
  double acc[8 * kA] = {};
  const std::int64_t block = static_cast<std::int64_t>(w) * kA;
  std::int64_t i = 0;
  // acc index k*w+lane and source index i+k*w+lane both run contiguously over [0, block), so
  // the accumulator-major/lane-minor nest is one flat pass — same additions, same order.
  for (; i + block <= n; i += block) {
    for (std::int64_t m = 0; m < block; ++m) {
      acc[m] += v[i + m];
    }
  }
  double s = 0.0;
  for (int lane = 0; lane < w; ++lane) {  // ADR-013 frozen combine: (0+2),(1+3), then +
    s += (acc[lane] + acc[2 * w + lane]) + (acc[w + lane] + acc[3 * w + lane]);
  }
  for (; i < n; ++i) {
    s += v[i];
  }
  return s;
}

template <bool kAutovec>
void bm_sum_f64(benchmark::State& state) {
  const auto n = static_cast<std::int64_t>(state.range(0));
  quiver::bench::Rng rng(kSeed);
  std::vector<double> v(static_cast<std::size_t>(n));
  quiver::bench::fill_uniform(rng, v.data(), n);
  const auto run = [&]() {
#if defined(QUIVER_BENCH_HAVE_AUTOVEC_AVX2)
    if constexpr (kAutovec) {
      return quiver::bench::autovec_avx2::sum_wrap_f64(v.data(), n, nullptr);
    }
#endif
    return quiver::reduce_sum_wrap(quiver::BatchView<double>{v.data(), n},
                                   quiver::BitmapView{nullptr});
  };
  // Effective backend, not the cap: empty dispatch rows fall through, so an avx512-capable
  // host still executes the AVX2 backend until the AVX-512 rows land (M7).
  int policy_w = 0;  // strict fold (scalar spec; also the autovec baseline)
  if (!kAutovec) {
    if (quiver::active_isa() >= quiver::Isa::kAvx2 && quiver::cpu_supports(quiver::Isa::kAvx2)) {
      policy_w = 4;  // AVX2 f64
    } else if (quiver::active_isa() >= quiver::Isa::kNeon &&
               quiver::cpu_supports(quiver::Isa::kNeon)) {
      policy_w = 2;  // NEON f64
    }
  }
  const double want = expected_dense_sum_f64(v.data(), n, policy_w);
  const double got = run();
  quiver::bench::validate_or_abort("BM_reduce_f64", got == want,
                                   "policy sum vs same-order recompute");
  g_pmu.start();
  for (auto _ : state) {
    benchmark::DoNotOptimize(run());
  }
  attach_pmu(state, g_pmu.stop_and_read(), n);
}

void register_benchmarks() {
  const char* variant = quiver::bench::variant_name(quiver::active_isa());
  for (const std::int64_t n : {4096, 65536}) {
    for (const int null_pct : {0, 10, 50}) {
      benchmark::RegisterBenchmark(quiver::bench::bench_name("reduce", "sum_wrap", variant, "i64",
                                                             "n=" + std::to_string(n) + "/nulls=" +
                                                                 std::to_string(null_pct)),
                                   bm_sum_i64<false>)
          ->Args({n, null_pct});
    }
    benchmark::RegisterBenchmark(quiver::bench::bench_name("reduce", "sum_wrap", variant, "f64",
                                                           "n=" + std::to_string(n) + "/nulls=0"),
                                 bm_sum_f64<false>)
        ->Args({n});
    for (const int null_pct : {0, 50}) {
      benchmark::RegisterBenchmark(quiver::bench::bench_name("reduce", "min", variant, "i64",
                                                             "n=" + std::to_string(n) + "/nulls=" +
                                                                 std::to_string(null_pct)),
                                   bm_min_t<std::int64_t>)
          ->Args({n, null_pct});
    }
    // i32 probes whether the single-accumulator latency chain loses at narrow widths too (the
    // chain is 1 native vmin per vector there vs 2 ops for 64-bit lanes, but still A=1 serial).
    benchmark::RegisterBenchmark(quiver::bench::bench_name("reduce", "min", variant, "i32",
                                                           "n=" + std::to_string(n) + "/nulls=0"),
                                 bm_min_t<std::int32_t>)
        ->Args({n, 0});
    benchmark::RegisterBenchmark(quiver::bench::bench_name("reduce", "min", variant, "f64",
                                                           "n=" + std::to_string(n) + "/nulls=0"),
                                 bm_min_t<double>)
        ->Args({n, 0});
  }
#if defined(QUIVER_BENCH_HAVE_AUTOVEC_AVX2)
  // Equal-ISA autovec baseline variants (ADR-011): registered only where the explicit AVX2
  // path also runs, so every verdict pair exists on the same machine (REQ-BENCH-002).
  if (quiver::cpu_supports(quiver::Isa::kAvx2)) {
    for (const std::int64_t n : {4096, 65536}) {
      for (const int null_pct : {0, 10, 50}) {
        benchmark::RegisterBenchmark(
            quiver::bench::bench_name("reduce", "sum_wrap", "autovec-avx2", "i64",
                                      "n=" + std::to_string(n) +
                                          "/nulls=" + std::to_string(null_pct)),
            bm_sum_i64<true>)
            ->Args({n, null_pct});
      }
      benchmark::RegisterBenchmark(quiver::bench::bench_name("reduce", "sum_wrap", "autovec-avx2",
                                                             "f64",
                                                             "n=" + std::to_string(n) + "/nulls=0"),
                                   bm_sum_f64<true>)
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
