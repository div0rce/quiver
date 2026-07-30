// K10 benchmarks. Hypotheses (PRD 08 §4, compute-bound framing): (a) the checked-vs-wrap
// tax is the headline number (compare against BM_arith/add names); (b) cost is flat in
// overflow density (branch-free accumulation, ADR-014 — the overflow_density axis is the
// evidence); (c) the 64-bit checked multiply is scalar and the ledger says so (REQ-K10-003).
// Axes: overflow_density × batch (QLM-1 overflow_density axis).
// Module: MOD-BENCH / MOD-K10 | REQs: REQ-BENCH-002..004 | ADR-014
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
constexpr std::uint64_t kSeed = 0xBE5CB00Aull;

// The two operand vectors a guarded op reads, filled together.
struct OperandPair {
  std::vector<std::int64_t>& a;
  std::vector<std::int64_t>& b;
};

// Inputs with a controlled overflow density: overflowing lanes get a = max-1, b = 2.
void make_inputs(quiver::bench::Rng& rng, std::int64_t n, int ovf_permille, OperandPair ops) {
  ops.a.resize(static_cast<std::size_t>(n));
  ops.b.resize(static_cast<std::size_t>(n));
  for (std::size_t i = 0; i < ops.a.size(); ++i) {
    const bool ovf = rng.next_below(1000) < static_cast<std::uint64_t>(ovf_permille);
    if (ovf) {
      ops.a[i] = std::numeric_limits<std::int64_t>::max() - 1;
      ops.b[i] = 2;
    } else {
      ops.a[i] = static_cast<std::int64_t>(rng.next() >> 2);
      ops.b[i] = static_cast<std::int64_t>(rng.next() >> 2);
    }
  }
}

// Independent recompute helpers, deliberately not the kernel's own logic (REQ-BENCH-004).
bool add_overflows(std::int64_t x, std::int64_t y) {
  return (y > 0 && x > std::numeric_limits<std::int64_t>::max() - y) ||
         (y < 0 && x < std::numeric_limits<std::int64_t>::min() - y);
}

std::int64_t wrap_add(std::int64_t x, std::int64_t y) {
  return static_cast<std::int64_t>(static_cast<std::uint64_t>(x) + static_cast<std::uint64_t>(y));
}

struct CheckedExpectation {
  std::int64_t overflow_count;
  bool outputs_match;
};

// Validate all THREE outputs: wrapped values, per-lane overflow bitmap, and the total count —
// a correct count alone does not imply correct out[]/bits[].
CheckedExpectation recompute_checked(OperandPair ops, const std::vector<std::int64_t>& out,
                                     const std::vector<std::uint8_t>& bits) {
  CheckedExpectation e{0, true};
  for (std::size_t i = 0; i < ops.a.size(); ++i) {
    const bool ovf = add_overflows(ops.a[i], ops.b[i]);
    e.overflow_count += ovf ? 1 : 0;
    e.outputs_match = e.outputs_match && out[i] == wrap_add(ops.a[i], ops.b[i]);
    const bool bit = ((bits[i >> 3] >> (i & 7)) & 1u) != 0;
    e.outputs_match = e.outputs_match && bit == ovf;
  }
  return e;
}

bool saturating_outputs_match(OperandPair ops, const std::vector<std::int64_t>& out) {
  for (std::size_t i = 0; i < ops.a.size(); ++i) {
    const std::int64_t x = ops.a[i];
    const std::int64_t y = ops.b[i];
    const std::int64_t want = add_overflows(x, y)
                                  ? (x < 0 ? std::numeric_limits<std::int64_t>::min()
                                           : std::numeric_limits<std::int64_t>::max())
                                  : wrap_add(x, y);
    if (out[i] != want) {
      return false;
    }
  }
  return true;
}

void bm_checked_add_i64(benchmark::State& state) {
  const auto n = static_cast<std::int64_t>(state.range(0));
  const int ovf_permille = static_cast<int>(state.range(1));
  quiver::bench::Rng rng(kSeed);
  std::vector<std::int64_t> a;
  std::vector<std::int64_t> b;
  make_inputs(rng, n, ovf_permille, {a, b});
  std::vector<std::int64_t> out(static_cast<std::size_t>(n));
  std::vector<std::uint8_t> bits((static_cast<std::size_t>(n) + 7) / 8);

  const std::int64_t got =
      quiver::arith_checked(quiver::ArithOp::kAdd, quiver::BatchView<std::int64_t>{a.data(), n},
                            quiver::BatchView<std::int64_t>{b.data(), n}, out.data(), bits.data());
  const CheckedExpectation want = recompute_checked({a, b}, out, bits);
  quiver::bench::validate_or_abort(
      "BM_arith_guarded", got == want.overflow_count && want.outputs_match,
      "checked add: count + wrapped values + overflow bitmap vs recompute");

  g_pmu.start();
  for (auto _ : state) {
    benchmark::DoNotOptimize(quiver::arith_checked(
        quiver::ArithOp::kAdd, quiver::BatchView<std::int64_t>{a.data(), n},
        quiver::BatchView<std::int64_t>{b.data(), n}, out.data(), bits.data()));
  }
  attach_pmu(state, g_pmu.stop_and_read(), n);
}

void bm_saturating_add_i64(benchmark::State& state) {
  const auto n = static_cast<std::int64_t>(state.range(0));
  quiver::bench::Rng rng(kSeed);
  std::vector<std::int64_t> a;
  std::vector<std::int64_t> b;
  make_inputs(rng, n, 100, {a, b});
  std::vector<std::int64_t> out(static_cast<std::size_t>(n));
  quiver::arith_saturating(quiver::ArithOp::kAdd, quiver::BatchView<std::int64_t>{a.data(), n},
                           quiver::BatchView<std::int64_t>{b.data(), n}, out.data());
  quiver::bench::validate_or_abort("BM_arith_saturating", saturating_outputs_match({a, b}, out),
                                   "saturation vs independent recompute");
  g_pmu.start();
  for (auto _ : state) {
    quiver::arith_saturating(quiver::ArithOp::kAdd, quiver::BatchView<std::int64_t>{a.data(), n},
                             quiver::BatchView<std::int64_t>{b.data(), n}, out.data());
    benchmark::DoNotOptimize(out.data());
  }
  attach_pmu(state, g_pmu.stop_and_read(), n);
}

void register_benchmarks() {
  const char* variant = quiver::bench::variant_name(quiver::active_isa());
  for (const std::int64_t n : {4096, 65536}) {
    for (const int permille : {0, 1, 500}) {  // overflow_density axis: 0, 0.1%, 50%
      benchmark::RegisterBenchmark(
          quiver::bench::bench_name({"arith_guarded", "checked_add", variant, "i64"},
                                    "n=" + std::to_string(n) + "/ovf=" + std::to_string(permille)),
          bm_checked_add_i64)
          ->Args({n, permille});
    }
    benchmark::RegisterBenchmark(
        quiver::bench::bench_name({"arith_guarded", "saturating_add", variant, "i64"},
                                  "n=" + std::to_string(n)),
        bm_saturating_add_i64)
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
