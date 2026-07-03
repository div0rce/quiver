// K5 benchmarks. Hypothesis: gather cost is MLP/latency-bound, not bandwidth-bound — the
// dict-size sweep crosses cache levels and cost per value should step with each level
// (roofline class, PRD 08 §4; Survey §3.9/§4.2). The gather-vs-scalar evidence gate
// (REQ-K5-004) consumes this benchmark's grid at M4/M7.
// Module: MOD-BENCH / MOD-K5 | REQs: REQ-BENCH-002..004, REQ-K5-004 (evidence source)
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

void attach_pmu(benchmark::State& state, const quiver::bench::PmuCounters& c, std::int64_t values) {
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

void bm_dict_decode(benchmark::State& state) {
  const auto n = static_cast<std::int64_t>(state.range(0));
  const auto dict_bytes = static_cast<std::size_t>(state.range(1));
  const std::int64_t dict_len = static_cast<std::int64_t>(dict_bytes / sizeof(std::int64_t));
  quiver::bench::Rng rng(kSeed);
  std::vector<std::int64_t> dict(static_cast<std::size_t>(dict_len));
  quiver::bench::fill_uniform(rng, dict.data(), dict_len);
  std::vector<std::uint32_t> codes(static_cast<std::size_t>(n));
  for (std::int64_t i = 0; i < n; ++i) {
    codes[static_cast<std::size_t>(i)] =
        static_cast<std::uint32_t>(rng.next_below(static_cast<std::uint64_t>(dict_len)));
  }
  std::vector<std::int64_t> out(static_cast<std::size_t>(n));

  quiver::dict_decode(quiver::BatchView<std::int64_t>{dict.data(), dict_len}, codes.data(), n,
                      out.data());
  std::int64_t checksum_got = 0;
  std::int64_t checksum_want = 0;
  for (std::int64_t i = 0; i < n; ++i) {
    checksum_got += out[static_cast<std::size_t>(i)];
    checksum_want += dict[codes[static_cast<std::size_t>(i)]];
  }
  quiver::bench::validate_or_abort("BM_take", checksum_got == checksum_want,
                                   "decode checksum vs independent recompute");

  g_pmu.start();
  for (auto _ : state) {
    quiver::dict_decode(quiver::BatchView<std::int64_t>{dict.data(), dict_len}, codes.data(), n,
                        out.data());
    benchmark::DoNotOptimize(out.data());
  }
  attach_pmu(state, g_pmu.stop_and_read(), n);
}

void register_benchmarks() {
  const char* variant = quiver::active_isa() == quiver::Isa::kScalar ? "scalar"
                        : quiver::active_isa() == quiver::Isa::kNeon ? "neon"
                        : quiver::active_isa() == quiver::Isa::kAvx2 ? "avx2"
                                                                     : "avx512";
  // Dict-size sweep across cache levels (QLM-1 dict_size axis; mandatory per PRD 08 §4).
  for (const std::int64_t dict_bytes : {4 << 10, 32 << 10, 256 << 10, 8 << 20, 64 << 20}) {
    benchmark::RegisterBenchmark(
        quiver::bench::bench_name("take", "dict_decode", variant, "i64_u32",
                                  "n=65536/dict=" + std::to_string(dict_bytes >> 10) + "KiB"),
        bm_dict_decode)
        ->Args({65536, dict_bytes});
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
