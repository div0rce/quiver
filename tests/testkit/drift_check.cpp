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

int main(int argc, char** argv) {
  constexpr std::uint64_t kSeed = 0x51E5EEDBA5E0001ull;
  constexpr std::int64_t kN = 4096;
  const bool print_golden = argc > 1 && std::strcmp(argv[1], "--print-golden") == 0;

  {
    quiver_test::Rng tr(kSeed);
    quiver::bench::Rng br(kSeed);
    std::vector<std::int32_t> tv(kN);
    std::vector<std::int32_t> bv(kN);
    quiver_test::fill_uniform(tr, tv.data(), kN);
    quiver::bench::fill_uniform(br, bv.data(), kN);
    check("uniform<i32>", same(tv, bv));
    if (print_golden) {
      std::printf("QUIVER_GOLDEN_UNIFORM_I32=0x%llx\n",
                  (unsigned long long)quiver_test::fnv1a64(tv.data(), tv.size() * 4));
    }
  }
  {
    quiver_test::Rng tr(kSeed);
    quiver::bench::Rng br(kSeed);
    std::vector<double> tv(kN);
    std::vector<double> bv(kN);
    quiver_test::fill_uniform(tr, tv.data(), kN);
    quiver::bench::fill_uniform(br, bv.data(), kN);
    check("uniform<f64>", same(tv, bv));
    if (print_golden) {
      std::printf("QUIVER_GOLDEN_UNIFORM_F64=0x%llx\n",
                  (unsigned long long)quiver_test::fnv1a64(tv.data(), tv.size() * 8));
    }
  }
  {
    quiver_test::Rng tr(kSeed);
    quiver::bench::Rng br(kSeed);
    std::vector<std::uint32_t> tv(kN);
    std::vector<std::uint32_t> bv(kN);
    quiver_test::fill_zipf_codes(tr, tv.data(), kN);
    quiver::bench::fill_zipf_codes(br, bv.data(), kN);
    check("zipf codes", same(tv, bv));
    if (print_golden) {
      std::printf("QUIVER_GOLDEN_ZIPF=0x%llx\n",
                  (unsigned long long)quiver_test::fnv1a64(tv.data(), tv.size() * 4));
    }
  }
  {
    const std::int64_t bytes = (kN + 7) / 8;
    for (const bool clustered : {false, true}) {
      const int pct = clustered ? 50 : 10;
      quiver_test::Rng tr(kSeed);
      quiver::bench::Rng br(kSeed);
      std::vector<std::uint8_t> tv(bytes);
      std::vector<std::uint8_t> bv(bytes);
      if (clustered) {
        quiver_test::fill_bitmap_clustered(tr, tv.data(), kN, pct);
        quiver::bench::fill_bitmap_clustered(br, bv.data(), kN, pct);
      } else {
        quiver_test::fill_bitmap_uniform(tr, tv.data(), kN, pct);
        quiver::bench::fill_bitmap_uniform(br, bv.data(), kN, pct);
      }
      check(clustered ? "bitmap clustered" : "bitmap uniform", same(tv, bv));
      if (print_golden) {
        std::printf("QUIVER_GOLDEN_BITMAP_%s=0x%llx\n", clustered ? "C50" : "U10",
                    (unsigned long long)quiver_test::fnv1a64(tv.data(), tv.size()));
      }
    }
  }

  if (g_failures == 0) {
    std::printf("drift_check: OK — testkit and bench distributions are byte-identical\n");
  }
  return g_failures == 0 ? 0 : 1;
}
