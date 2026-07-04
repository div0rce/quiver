// K8 benchmarks. Hypothesis (PRD 08 §4): bandwidth-bound at large n — throughput tracks the
// packed byte stream, so narrower widths yield more values/s; the specialized byte-aligned
// widths beat the generic gather path. Axes: bit_width × batch (QLM-1 bit_width axis).
// Validation (REQ-BENCH-004): per-bit gather recompute straight from the ADR-026 sentence.
// Module: MOD-BENCH / MOD-K8 | REQs: REQ-BENCH-002..004 | ADR-026
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
constexpr std::uint64_t kSeed = 0xBE5CB008ull;

void attach_pmu(benchmark::State& state, const quiver::bench::PmuCounters& c, std::int64_t values,
                std::int64_t bytes) {
  state.SetItemsProcessed(state.iterations() * values);
  state.SetBytesProcessed(state.iterations() * bytes);
  if (c.valid) {
    const double total = static_cast<double>(state.iterations()) * static_cast<double>(values);
    state.counters["cycles_per_value"] = static_cast<double>(c.cycles) / total;
  }
}

template <bool kAutovec>
void bm_unpack_u32(benchmark::State& state) {
  const auto n = static_cast<std::int64_t>(state.range(0));
  const int w = static_cast<int>(state.range(1));
  quiver::bench::Rng rng(kSeed);
  const std::int64_t bytes = (n * w + 7) / 8;
  std::vector<std::uint8_t> packed(static_cast<std::size_t>(bytes) + 1);
  for (auto& byte : packed) {
    byte = static_cast<std::uint8_t>(rng.next());
  }
  std::vector<std::uint32_t> out(static_cast<std::size_t>(n));
  const std::uint32_t base = 7;

  const auto run = [&]() {
#if defined(QUIVER_BENCH_HAVE_AUTOVEC_AVX2)
    if constexpr (kAutovec) {
      quiver::bench::autovec_avx2::unpack_for_u32(packed.data(), n, w, base, out.data());
      return;
    }
#endif
    quiver::unpack_for(packed.data(), n, w, base, out.data());
  };
  run();
  bool ok = true;
  for (std::int64_t i = 0; i < n; ++i) {  // per-bit recompute (ADR-026 definition)
    std::uint64_t v = 0;
    for (int k = 0; k < w; ++k) {
      const std::int64_t j = i * w + k;
      v |= static_cast<std::uint64_t>((packed[static_cast<std::size_t>(j / 8)] >> (j % 8)) & 1u)
           << k;
    }
    ok = ok && out[static_cast<std::size_t>(i)] == base + static_cast<std::uint32_t>(v);
  }
  quiver::bench::validate_or_abort("BM_unpack", ok, "values vs per-bit recompute");

  g_pmu.start();
  for (auto _ : state) {
    run();
    benchmark::DoNotOptimize(out.data());
  }
  attach_pmu(state, g_pmu.stop_and_read(), n, bytes);
}

void register_benchmarks() {
  const char* variant = quiver::bench::variant_name(quiver::active_isa());
  for (const std::int64_t n : {4096, 65536}) {
    for (const int w : {1, 4, 7, 8, 16, 24, 32}) {  // bit_width axis (QLM-1)
      benchmark::RegisterBenchmark(
          quiver::bench::bench_name("unpack", "unpack_for", variant, "u32",
                                    "n=" + std::to_string(n) + "/w=" + std::to_string(w)),
          bm_unpack_u32<false>)
          ->Args({n, w});
    }
  }
#if defined(QUIVER_BENCH_HAVE_AUTOVEC_AVX2)
  // Equal-ISA autovec baseline variants (ADR-011; verdict pair for `avx2`, REQ-BENCH-002).
  if (quiver::cpu_supports(quiver::Isa::kAvx2)) {
    for (const std::int64_t n : {4096, 65536}) {
      for (const int w : {1, 4, 7, 8, 16, 24, 32}) {
        benchmark::RegisterBenchmark(
            quiver::bench::bench_name("unpack", "unpack_for", "autovec-avx2", "u32",
                                      "n=" + std::to_string(n) + "/w=" + std::to_string(w)),
            bm_unpack_u32<true>)
            ->Args({n, w});
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
