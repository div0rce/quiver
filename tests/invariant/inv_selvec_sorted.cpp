// Invariant: every produced selection vector is strictly increasing within its defined
// output (ADR-025) — checked across producers and lengths.
// Covers: REQ-MEM-007, REQ-TEST-005
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "quiver/compare.h"
#include "quiver/select.h"
#include "tests/testkit/generators.h"

namespace {

TEST(InvSelvecSorted, ProducersEmitStrictlyIncreasingIndices) {
  quiver_test::Rng rng(0x50D7ED);
  for (const std::int64_t n : {1, 7, 64, 129, 1000, 4096}) {
    std::vector<std::int16_t> v(static_cast<std::size_t>(n));
    quiver_test::fill_uniform(rng, v.data(), n);
    std::vector<std::uint8_t> bits((static_cast<std::size_t>(n) + 7) / 8);
    quiver_test::fill_bitmap_uniform(rng, bits.data(), n, 30);
    std::vector<std::uint32_t> idx(static_cast<std::size_t>(n));

    const std::int64_t c1 =
        quiver::compare_selvec(quiver::CompareOp::kGt, quiver::BatchView<std::int16_t>{v.data(), n},
                               std::int16_t{0}, quiver::BitmapView{nullptr}, idx.data());
    for (std::int64_t j = 1; j < c1; ++j) {
      ASSERT_LT(idx[static_cast<std::size_t>(j - 1)], idx[static_cast<std::size_t>(j)]);
    }
    const std::int64_t c2 =
        quiver::bitmap_to_selvec(quiver::BitmapView{bits.data()}, n, idx.data());
    for (std::int64_t j = 1; j < c2; ++j) {
      ASSERT_LT(idx[static_cast<std::size_t>(j - 1)], idx[static_cast<std::size_t>(j)]);
    }
  }
}

}  // namespace
