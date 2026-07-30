// K2 differential matrix: backends vs naive compaction over L-set × selectivity × pattern.
// Covers: REQ-TEST-002/-003, REQ-K2-001..002
#include <cstdint>
#include <cstdlib>
#include <vector>

#include <gtest/gtest.h>

#include "quiver/dispatch.h"
#include "quiver/filter.h"
#include "tests/testkit/assertions.h"
#include "tests/testkit/generators.h"
#include "tests/testkit/reference.h"

namespace {

using quiver_test::Rng;
namespace ref = quiver_test::ref;

// Applies a functor to a zero of each of the ten element types (REQ-API-004 sweep helper).
template <class F>
void for_each_element_type(F f) {
  f(std::int8_t{});
  f(std::int16_t{});
  f(std::int32_t{});
  f(std::int64_t{});
  f(std::uint8_t{});
  f(std::uint16_t{});
  f(std::uint32_t{});
  f(std::uint64_t{});
  f(float{});
  f(double{});
}

// PR-tier length set: every tail residue for widths <= 64 B plus representative large sizes
// (REQ-TEST-003 L-set); QUIVER_NIGHTLY widens the sampled axes to the full cross product.
constexpr std::int64_t kLengths[] = {0,  1,  2,  3,  7,   8,   9,   15,  16,  17,  31,   32,
                                     33, 63, 64, 65, 127, 128, 129, 255, 256, 257, 1000, 4096};

[[maybe_unused]] inline bool nightly() {
  return std::getenv("QUIVER_NIGHTLY") != nullptr;
}
// Runs `body` once per host-executable tier (REQ-TEST-003 backend column; scalar-only at M3).
template <class Body>
void for_each_backend(Body body) {
  for (const quiver::Isa isa :
       {quiver::Isa::kScalar, quiver::Isa::kNeon, quiver::Isa::kAvx2, quiver::Isa::kAvx512}) {
    if (isa != quiver::Isa::kScalar && !quiver::cpu_supports(isa)) {
      continue;
    }
    ASSERT_TRUE(quiver::set_isa_override(isa));
    quiver::warmup();
    body(isa);
  }
  quiver::clear_isa_override();
}

template <class T>
void run_filter_diff(std::uint64_t seed) {
  Rng rng(seed);
  const int sel_pcts_pr[] = {1, 50, 99};
  const int sel_pcts_full[] = {1, 10, 50, 90, 99};
  for (const std::int64_t n : kLengths) {
    std::vector<T> v(static_cast<std::size_t>(n) + 1);
    quiver_test::fill_uniform(rng, v.data(), n);
    const std::int64_t bytes = (n + 7) / 8;
    for (const bool clustered : {false, true}) {
      for (const int pct :
           nightly() ? std::vector<int>(std::begin(sel_pcts_full), std::end(sel_pcts_full))
                     : std::vector<int>(std::begin(sel_pcts_pr), std::end(sel_pcts_pr))) {
        std::vector<std::uint8_t> sel(static_cast<std::size_t>(bytes) + 1);
        if (clustered) {
          quiver_test::fill_bitmap_clustered(rng, sel.data(), n, pct);
        } else {
          quiver_test::fill_bitmap_uniform(rng, sel.data(), n, pct);
        }
        const auto want = ref::filter_expected(v.data(), n, sel.data());
        std::vector<T> got(static_cast<std::size_t>(n) + 1);
        const std::int64_t got_count = quiver::filter(quiver::BatchView<T>{v.data(), n},
                                                      quiver::BitmapView{sel.data()}, got.data());
        ASSERT_EQ(got_count, static_cast<std::int64_t>(want.size())) << "n=" << n;
        if (!want.empty()) {
          ASSERT_TRUE(quiver_test::buffers_equal(want.data(), got.data(),
                                                 static_cast<std::int64_t>(want.size()),
                                                 {seed, "REQ-K2-001"}));
        }
      }
    }
  }
}

TEST(DiffFilter, AllBackendsMatchOracle) {
  for_each_backend([&](quiver::Isa isa) {
    for_each_element_type(
        [&]<class T>(T) { run_filter_diff<T>(0xD1FF0002ull + static_cast<std::uint64_t>(isa)); });
  });
}

}  // namespace
