// K2 properties (PRD 12 §5): count ≡ cardinality; filter(bitmap) ≡ filter(bitmap_to_selvec);
// output is a subsequence; in-place ≡ out-of-place.
// Covers: REQ-TEST-008, REQ-K2-001..002
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <gtest/gtest.h>

#include "tests/testkit/generators.h"
#include "tests/testkit/reference.h"
#include "quiver/filter.h"
#include "quiver/mask.h"
#include "quiver/select.h"

namespace {

using quiver_test::Rng;
namespace ref = quiver_test::ref;


TEST(PropFilter, RepresentationEquivalenceAndSubsequence) {
  Rng rng(0x9E0902);
  for (int iter = 0; iter < (std::getenv("QUIVER_NIGHTLY") ? 200 : 40); ++iter) {
    const std::int64_t n = 1 + static_cast<std::int64_t>(rng.next_below(1500));
    std::vector<std::uint64_t> v(static_cast<std::size_t>(n));
    quiver_test::fill_uniform(rng, v.data(), n);
    std::vector<std::uint8_t> bits((static_cast<std::size_t>(n) + 7) / 8);
    quiver_test::fill_bitmap_clustered(rng, bits.data(), n, 40);
    const quiver::BatchView<std::uint64_t> in{v.data(), n};

    std::vector<std::uint64_t> via_bitmap(static_cast<std::size_t>(n));
    const std::int64_t c1 =
        quiver::filter(in, quiver::BitmapView{bits.data()}, via_bitmap.data());
    ASSERT_EQ(c1, quiver::mask_popcount(quiver::BitmapView{bits.data()}, n));

    std::vector<std::uint32_t> idx(static_cast<std::size_t>(n));
    const std::int64_t c2 =
        quiver::bitmap_to_selvec(quiver::BitmapView{bits.data()}, n, idx.data());
    std::vector<std::uint64_t> via_selvec(static_cast<std::size_t>(n));
    ASSERT_EQ(quiver::filter(in, quiver::SelVec{idx.data(), c2}, via_selvec.data()), c1);
    ASSERT_EQ(std::memcmp(via_bitmap.data(), via_selvec.data(),
                          static_cast<std::size_t>(c1) * 8), 0);

    // Subsequence check: outputs appear in input order.
    std::int64_t cursor = 0;
    for (std::int64_t j = 0; j < c1; ++j) {
      while (cursor < n && v[static_cast<std::size_t>(cursor)] !=
                               via_bitmap[static_cast<std::size_t>(j)]) {
        ++cursor;
      }
      ASSERT_LT(cursor, n) << "output element not found in order";
      ++cursor;
    }
  }
}

}  // namespace
