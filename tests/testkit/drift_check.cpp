// Drift-alarm conformance runner (REQ-BENCH-015): the testkit generators and the bench-local
// distributions are duplicated by design; this standalone program (deliberately free of BOTH
// GoogleTest and Google Benchmark, honoring REQ-TEST-018) asserts byte-identical output for
// identical seeds across every QLM-1 axis implementation, and prints golden hashes with
// --print-golden for deliberate spec updates.
// Module: MOD-TESTKIT | REQs: REQ-BENCH-015, REQ-TEST-018 | PRD 10 §2
#include <cstdio>
#include <cstring>
#include <vector>

#include "bench/harness/distributions.h"
#include "tests/testkit/generators.h"

namespace {

int g_failures = 0;

void check(const char* what, bool ok) {
  if (!ok) {
    ++g_failures;
    std::fprintf(stderr,
                 "drift_check: MISMATCH in %s — testkit and bench distributions have "
                 "diverged (REQ-BENCH-015)\n",
                 what);
  }
}

template <class T>
bool same(const std::vector<T>& a, const std::vector<T>& b) {
  return a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * sizeof(T)) == 0;
}

}  // namespace

constexpr std::uint64_t kSeed = 0x51E5EEDBA5E0001ull;
constexpr std::int64_t kN = 4096;
bool g_print_golden = false;

// One generator under comparison: what to call it, which golden line it prints, and how many
// elements the two buffers hold (bitmaps are sized in bytes, not values).
struct DriftCase {
  const char* label;
  const char* golden_name;
  std::int64_t count;
};

// Fill from the testkit and from the bench harness with the SAME seed, then require the two
// buffers byte-identical — that identity is the whole point of this check.
template <class T, class TestFill, class BenchFill>
void check_pair(DriftCase c, TestFill test_fill, BenchFill bench_fill) {
  quiver_test::Rng tr(kSeed);
  quiver::bench::Rng br(kSeed);
  std::vector<T> tv(static_cast<std::size_t>(c.count));
  std::vector<T> bv(static_cast<std::size_t>(c.count));
  test_fill(tr, tv.data());
  bench_fill(br, bv.data());
  check(c.label, same(tv, bv));
  if (g_print_golden) {
    std::printf("%s=0x%llx\n", c.golden_name,
                (unsigned long long)quiver_test::fnv1a64(tv.data(), tv.size() * sizeof(T)));
  }
}

int main(int argc, char** argv) {
  g_print_golden = argc > 1 && std::strcmp(argv[1], "--print-golden") == 0;
  constexpr std::int64_t kBytes = (kN + 7) / 8;

  check_pair<std::int32_t>(
      {"uniform<i32>", "QUIVER_GOLDEN_UNIFORM_I32", kN},
      [](quiver_test::Rng& r, std::int32_t* p) { quiver_test::fill_uniform(r, p, kN); },
      [](quiver::bench::Rng& r, std::int32_t* p) { quiver::bench::fill_uniform(r, p, kN); });
  check_pair<double>(
      {"uniform<f64>", "QUIVER_GOLDEN_UNIFORM_F64", kN},
      [](quiver_test::Rng& r, double* p) { quiver_test::fill_uniform(r, p, kN); },
      [](quiver::bench::Rng& r, double* p) { quiver::bench::fill_uniform(r, p, kN); });
  check_pair<std::uint32_t>(
      {"zipf codes", "QUIVER_GOLDEN_ZIPF", kN},
      [](quiver_test::Rng& r, std::uint32_t* p) { quiver_test::fill_zipf_codes(r, p, kN); },
      [](quiver::bench::Rng& r, std::uint32_t* p) { quiver::bench::fill_zipf_codes(r, p, kN); });
  check_pair<std::uint8_t>(
      {"bitmap uniform", "QUIVER_GOLDEN_BITMAP_U10", kBytes},
      [](quiver_test::Rng& r, std::uint8_t* p) { quiver_test::fill_bitmap_uniform(r, p, kN, 10); },
      [](quiver::bench::Rng& r, std::uint8_t* p) {
        quiver::bench::fill_bitmap_uniform(r, p, kN, 10);
      });
  check_pair<std::uint8_t>(
      {"bitmap clustered", "QUIVER_GOLDEN_BITMAP_C50", kBytes},
      [](quiver_test::Rng& r, std::uint8_t* p) {
        quiver_test::fill_bitmap_clustered(r, p, kN, 50);
      },
      [](quiver::bench::Rng& r, std::uint8_t* p) {
        quiver::bench::fill_bitmap_clustered(r, p, kN, 50);
      });

  if (g_failures == 0) {
    std::printf("drift_check: OK — testkit and bench distributions are byte-identical\n");
  }
  return g_failures == 0 ? 0 : 1;
}
