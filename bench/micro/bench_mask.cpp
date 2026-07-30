// K4 benchmarks. Hypothesis (a designated T7 candidate): the auto-vectorized word loop is
// already at bandwidth — explicit SIMD is expected to show little or no gain here; the
// verdict is published either way (REQ-LEDGER-011; Survey §4.4).
// Module: MOD-BENCH / MOD-K4 | REQs: REQ-BENCH-002..004
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

template <bool kAutovec>
void bm_mask_and(benchmark::State& state) {
  const auto n = static_cast<std::int64_t>(state.range(0));
  quiver::bench::Rng rng(kSeed);
  const std::size_t bytes = (static_cast<std::size_t>(n) + 7) / 8;
  std::vector<std::uint8_t> a(bytes);
  std::vector<std::uint8_t> b(bytes);
  quiver::bench::fill_bitmap_uniform(rng, a.data(), n, 50);
  quiver::bench::fill_bitmap_uniform(rng, b.data(), n, 50);
  std::vector<std::uint8_t> out(bytes);

  const auto run = [&]() {
#if defined(QUIVER_BENCH_HAVE_AUTOVEC_AVX2)
    if constexpr (kAutovec) {
      quiver::bench::autovec_avx2::mask_combine(quiver::MaskOp::kAnd, a.data(), b.data(), n,
                                                out.data());
      return;
    }
#endif
    quiver::mask_combine(quiver::MaskOp::kAnd, quiver::BitmapView{a.data()},
                         quiver::BitmapView{b.data()}, n, out.data());
  };
  run();
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
    run();
    benchmark::DoNotOptimize(out.data());
  }
  attach_pmu(state, g_pmu.stop_and_read(), n);
}

// Query form (mask_all). Hypothesis (REQ-BENCH-003, pre-registered before any measurement): the
// shipped query is a byte-at-a-time scalar loop with a per-byte early exit, which the
// autovectorizer does NOT vectorize (early-exit loops resist it, Survey §4.4), so both ledger
// variants run identical code today (parity) at roughly one byte per iteration. A NEON rework
// (vandq accumulator + a block-granular horizontal check that preserves the early exit at coarse
// granularity) should approach the read bandwidth on the no-early-exit input class -- `exit=none`,
// an all-valid validity bitmap, the ubiquitous "are there any nulls?" fast path -- while the
// `exit=first` class (a zero in byte 0) bounds the regression a block-granular exit can cost.
// Decision rule: promote the NEON query only if exit=none improves materially AND exit=first
// stays within a few nanoseconds absolute; otherwise keep the scalar delegation and record why.
void bm_mask_all(benchmark::State& state) {
  const auto n = static_cast<std::int64_t>(state.range(0));
  const bool exit_first = state.range(1) != 0;
  const std::size_t bytes = (static_cast<std::size_t>(n) + 7) / 8;
  std::vector<std::uint8_t> a(bytes, 0xFF);
  if (n & 7) {  // keep tail bits zero per the bitmap invariant
    a[bytes - 1] = static_cast<std::uint8_t>((1u << (n & 7)) - 1u);
  }
  if (exit_first) {
    a[0] = 0xFE;  // bit 0 invalid: the query can exit on the first byte
  }

  const auto run = [&]() { return quiver::mask_all(quiver::BitmapView{a.data()}, n); };
  bool want = true;  // independent per-bit recompute (REQ-BENCH-004)
  for (std::int64_t i = 0; i < n; ++i) {
    want = want && (((a[static_cast<std::size_t>(i) >> 3] >> (i & 7)) & 1u) != 0);
  }
  quiver::bench::validate_or_abort("BM_mask_all", run() == want, "all() vs per-bit recompute");

  g_pmu.start();
  for (auto _ : state) {
    benchmark::DoNotOptimize(run());
  }
  attach_pmu(state, g_pmu.stop_and_read(), n);
}

void register_benchmarks() {
  const char* variant = quiver::bench::variant_name(quiver::active_isa());
  for (const std::int64_t n : {4096, 65536, 1 << 20}) {
    benchmark::RegisterBenchmark(
        quiver::bench::bench_name({"mask", "and", variant, "bitmap"}, "n=" + std::to_string(n)),
        bm_mask_and<false>)
        ->Args({n});
  }
  for (const std::int64_t n : {65536, 1 << 20}) {
    for (const int exit_first : {0, 1}) {
      benchmark::RegisterBenchmark(quiver::bench::bench_name({"mask", "all", variant, "bitmap"},
                                                             "n=" + std::to_string(n) + "/exit=" +
                                                                 (exit_first ? "first" : "none")),
                                   bm_mask_all)
          ->Args({n, exit_first});
    }
  }
#if defined(QUIVER_BENCH_HAVE_AUTOVEC_AVX2)
  // Equal-ISA autovec baseline variants (ADR-011; verdict pair for `avx2`, REQ-BENCH-002).
  if (quiver::cpu_supports(quiver::Isa::kAvx2)) {
    for (const std::int64_t n : {4096, 65536, 1 << 20}) {
      benchmark::RegisterBenchmark(
          quiver::bench::bench_name({"mask", "and", "autovec-avx2", "bitmap"},
                                    "n=" + std::to_string(n)),
          bm_mask_and<true>)
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
