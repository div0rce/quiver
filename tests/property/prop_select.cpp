// K3 properties (PRD 12 §5): round-trip identities in both directions; count ≡ popcount.
// Covers: REQ-TEST-008, REQ-K3-001..002
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <gtest/gtest.h>

#include "quiver/mask.h"
#include "quiver/select.h"
#include "tests/testkit/generators.h"
#include "tests/testkit/reference.h"

namespace {

using quiver_test::Rng;
namespace ref = quiver_test::ref;

TEST(PropSelect, RoundTripsAreIdentities) {
  Rng rng(0x9E0903);
  for (int iter = 0; iter < (std::getenv("QUIVER_NIGHTLY") ? 400 : 60); ++iter) {
    const std::int64_t n = 1 + static_cast<std::int64_t>(rng.next_below(2000));
    const std::int64_t bytes = (n + 7) / 8;
    std::vector<std::uint8_t> bits(static_cast<std::size_t>(bytes));
    quiver_test::fill_bitmap_uniform(rng, bits.data(), n, static_cast<int>(rng.next_below(101)));
    std::vector<std::uint32_t> idx(static_cast<std::size_t>(n));
    const std::int64_t count =
        quiver::bitmap_to_selvec(quiver::BitmapView{bits.data()}, n, idx.data());
    ASSERT_EQ(count, quiver::mask_popcount(quiver::BitmapView{bits.data()}, n));
    std::vector<std::uint8_t> round(static_cast<std::size_t>(bytes));
    quiver::selvec_to_bitmap(quiver::SelVec{idx.data(), count}, n, round.data());
    ASSERT_EQ(std::memcmp(bits.data(), round.data(), static_cast<std::size_t>(bytes)), 0)
        << "bitmap -> selvec -> bitmap must be identity (REQ-K3-001)";
    // And the other direction: selvec -> bitmap -> selvec.
    std::vector<std::uint32_t> idx2(static_cast<std::size_t>(n));
    ASSERT_EQ(quiver::bitmap_to_selvec(quiver::BitmapView{round.data()}, n, idx2.data()), count);
    ASSERT_EQ(std::memcmp(idx.data(), idx2.data(), static_cast<std::size_t>(count) * 4), 0);
  }
}

}  // namespace
