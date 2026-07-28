// K1 benchmarks. Hypothesis (REQ-BENCH-003): compare cost is flat in selectivity — the
// branch-free core makes cost data-independent (REQ-KERNEL-003; Survey §3.4). Axes:
// selectivity × pattern × batch (QLM-1); variant = active ISA (scalar at M3; ISA variants
// register per cpu_supports as backends land).
// Validation (REQ-BENCH-004): output vs an independent per-element recompute, pre-timing.
// Module: MOD-BENCH / MOD-K1 | REQs: REQ-BENCH-002..004, REQ-KERNEL-003
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
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

// Narrow-width bitmap forms (i8/i16/i32). Hypothesis (REQ-BENCH-003, pre-registered before any
// measurement): unlike the 64-bit pack (which lost 0.69x and delegates per REQ-KERNEL-007), the
// handwritten NEON pack is expected to WIN at narrow widths — the per-group across-lane reduce
// cost falls from ~4 reduces per 8 elements (i64) to ~1 (i16/i32) or ~2 per 16 (i8), while the
// bytes-per-element drop makes the op more compute-bound, exactly where the pack economics
// improve. Decision rule: if the NEON variant loses at any narrow width, apply the same
// evidence-gated delegation as i64. Axes: two selectivities confirm the flat-in-selectivity
// property at these widths, two batch sizes cover L1/L2 residency (the full n x sel grid exists
// for the i64 rows above).
template <class T>
void bm_compare_bitmap_t(benchmark::State& state) {
  const auto n = static_cast<std::int64_t>(state.range(0));
  const int sel_pct = static_cast<int>(state.range(1));
  quiver::bench::Rng rng(kSeed);
  std::vector<T> v(static_cast<std::size_t>(n));
  quiver::bench::fill_uniform(rng, v.data(), n);
  // Comparand at the requested selectivity percentile of T's full uniform range, computed in
  // 64-bit so narrow widths keep percentile precision (span = 2^bits fits u64 for T < 64-bit).
  using U = std::make_unsigned_t<T>;
  const std::uint64_t span = static_cast<std::uint64_t>(std::numeric_limits<U>::max()) + 1u;
  const auto comparand = static_cast<T>(
      static_cast<std::int64_t>(span * (100u - static_cast<unsigned>(sel_pct)) / 100u) +
      static_cast<std::int64_t>(std::numeric_limits<T>::min()));
  std::vector<std::uint8_t> bits(static_cast<std::size_t>((n + 7) / 8));

  const auto run = [&]() {
    return quiver::compare_bitmap(quiver::CompareOp::kGt, quiver::BatchView<T>{v.data(), n},
                                  comparand, quiver::BitmapView{nullptr}, bits.data());
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

// Selvec output form of the same K1 predicate — the other half of the representation study
// (bitmap vs selvec, Charter §6.2): identical input + comparand, output is a selection vector.
// Cost is expected to scale with selectivity (one store per match) where the bitmap is flat.
void bm_compare_selvec(benchmark::State& state) {
  const auto n = static_cast<std::int64_t>(state.range(0));
  const int sel_pct = static_cast<int>(state.range(1));
  quiver::bench::Rng rng(kSeed);
  std::vector<std::int64_t> v(static_cast<std::size_t>(n));
  quiver::bench::fill_uniform(rng, v.data(), n);
  const std::int64_t comparand = static_cast<std::int64_t>(
      static_cast<std::uint64_t>(~0ull) / 100 * (100 - sel_pct) - (~0ull >> 1));
  std::vector<std::uint32_t> sel(static_cast<std::size_t>(n));

  const auto run = [&]() {
    return quiver::compare_selvec(quiver::CompareOp::kGt,
                                  quiver::BatchView<std::int64_t>{v.data(), n}, comparand,
                                  quiver::BitmapView{nullptr}, sel.data());
  };
  const std::int64_t got = run();
  std::int64_t want = 0;  // independent recompute (REQ-BENCH-004)
  for (std::int64_t i = n - 1; i >= 0; --i) {
    want += v[static_cast<std::size_t>(i)] > comparand ? 1 : 0;
  }
  quiver::bench::validate_or_abort("BM_compare_selvec", got == want,
                                   "count vs independent recompute");

  g_pmu.start();
  for (auto _ : state) {
    benchmark::DoNotOptimize(run());
  }
  attach_pmu(state, g_pmu.stop_and_read(), n);
}

template <class T>
void register_narrow(const char* variant, const char* type_name) {
  for (const std::int64_t n : {4096, 65536}) {
    for (const int pct : {10, 90}) {
      benchmark::RegisterBenchmark(
          quiver::bench::bench_name("compare", "bitmap_gt", variant, type_name,
                                    "n=" + std::to_string(n) + "/sel=" + std::to_string(pct)),
          bm_compare_bitmap_t<T>)
          ->Args({n, pct});
    }
  }
}

void register_benchmarks() {
  const char* variant = quiver::bench::variant_name(quiver::active_isa());
  register_narrow<std::int8_t>(variant, "i8");
  register_narrow<std::int16_t>(variant, "i16");
  register_narrow<std::int32_t>(variant, "i32");
  for (const std::int64_t n : {1024, 4096, 65536}) {
    for (const int pct : {1, 10, 50, 90, 99}) {
      benchmark::RegisterBenchmark(
          quiver::bench::bench_name("compare", "selvec_gt", variant, "i64",
                                    "n=" + std::to_string(n) + "/sel=" + std::to_string(pct)),
          bm_compare_selvec)
          ->Args({n, pct});
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
