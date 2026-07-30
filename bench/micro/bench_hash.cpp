// K7 benchmarks. Hypothesis (PRD 08 §4): hashing is compute-bound — ILP/port pressure
// dominate, so explicit SIMD is most likely to pay here; on Apple Firestorm the GPR
// multiplier pipes make the vector-vs-GPR question evidence-gated (REQ-KERNEL-007 — this
// bench is the deciding instrument). Axes: type × batch.
// Validation (REQ-BENCH-004): output vs a bench-local transcription of the FROZEN ADR-012
// formula (bench code never links test oracles, REQ-TEST-018).
// Module: MOD-BENCH / MOD-K7 | REQs: REQ-BENCH-002..004 | ADR-012
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
constexpr std::uint64_t kSeed = 0xBE5CB007ull;

// Bench-local ADR-012 transcription for validation.
std::uint64_t qhash64_local(std::uint64_t key, std::uint64_t seed) {
  std::uint64_t x = key ^ (seed + 0x9E3779B97F4A7C15ull);
  x ^= x >> 33;
  x *= 0xFF51AFD7ED558CCDull;
  x ^= x >> 33;
  x *= 0xC4CEB9FE1A85EC53ull;
  x ^= x >> 33;
  return x;
}

template <bool kAutovec>
void bm_hash_i64(benchmark::State& state) {
  const auto n = static_cast<std::int64_t>(state.range(0));
  quiver::bench::Rng rng(kSeed);
  std::vector<std::int64_t> v(static_cast<std::size_t>(n));
  quiver::bench::fill_uniform(rng, v.data(), n);
  std::vector<std::uint64_t> out(static_cast<std::size_t>(n));
  const std::uint64_t seed = 0x1234;

  const auto run = [&]() {
#if defined(QUIVER_BENCH_HAVE_AUTOVEC_AVX2)
    if constexpr (kAutovec) {
      quiver::bench::autovec_avx2::hash64_i64(v.data(), n, seed, out.data());
      return;
    }
#endif
    quiver::hash64(quiver::BatchView<std::int64_t>{v.data(), n}, seed, out.data());
  };
  run();
  bool ok = true;
  for (std::int64_t i = 0; i < n; ++i) {
    ok = ok && out[static_cast<std::size_t>(i)] ==
                   qhash64_local(static_cast<std::uint64_t>(v[static_cast<std::size_t>(i)]), seed);
  }
  quiver::bench::validate_or_abort("BM_hash", ok, "hashes vs frozen-formula recompute");

  g_pmu.start();
  for (auto _ : state) {
    run();
    benchmark::DoNotOptimize(out.data());
  }
  attach_pmu(state, g_pmu.stop_and_read(), n);
}

void bm_hash_combine(benchmark::State& state) {
  const auto n = static_cast<std::int64_t>(state.range(0));
  quiver::bench::Rng rng(kSeed);
  std::vector<std::uint64_t> a(static_cast<std::size_t>(n));
  std::vector<std::uint64_t> b(static_cast<std::size_t>(n));
  for (std::int64_t i = 0; i < n; ++i) {
    a[static_cast<std::size_t>(i)] = rng.next();
    b[static_cast<std::size_t>(i)] = rng.next();
  }
  std::vector<std::uint64_t> out(static_cast<std::size_t>(n));
  quiver::hash64_combine(a.data(), b.data(), n, out.data());
  bool ok = true;
  for (std::int64_t i = 0; i < n; ++i) {
    const std::uint64_t x = a[static_cast<std::size_t>(i)];
    const std::uint64_t y = b[static_cast<std::size_t>(i)];
    std::uint64_t t = x ^ (y + 0x9E3779B97F4A7C15ull + (x << 6) + (x >> 2));
    t ^= t >> 33;
    t *= 0xFF51AFD7ED558CCDull;
    t ^= t >> 33;
    t *= 0xC4CEB9FE1A85EC53ull;
    t ^= t >> 33;
    ok = ok && out[static_cast<std::size_t>(i)] == t;
  }
  quiver::bench::validate_or_abort("BM_hash_combine", ok, "combine vs frozen-formula recompute");
  g_pmu.start();
  for (auto _ : state) {
    quiver::hash64_combine(a.data(), b.data(), n, out.data());
    benchmark::DoNotOptimize(out.data());
  }
  attach_pmu(state, g_pmu.stop_and_read(), n);
}

void register_benchmarks() {
  const char* variant = quiver::bench::variant_name(quiver::active_isa());
  for (const std::int64_t n : {4096, 65536}) {
    benchmark::RegisterBenchmark(
        quiver::bench::bench_name({"hash", "hash64", variant, "i64"}, "n=" + std::to_string(n)),
        bm_hash_i64<false>)
        ->Args({n});
    benchmark::RegisterBenchmark(
        quiver::bench::bench_name({"hash", "combine", variant, "u64"}, "n=" + std::to_string(n)),
        bm_hash_combine)
        ->Args({n});
  }
#if defined(QUIVER_BENCH_HAVE_AUTOVEC_AVX2)
  // Equal-ISA autovec baseline variants (ADR-011; verdict pair for `avx2`, REQ-BENCH-002).
  if (quiver::cpu_supports(quiver::Isa::kAvx2)) {
    for (const std::int64_t n : {4096, 65536}) {
      benchmark::RegisterBenchmark(
          quiver::bench::bench_name({"hash", "hash64", "autovec-avx2", "i64"},
                                    "n=" + std::to_string(n)),
          bm_hash_i64<true>)
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
