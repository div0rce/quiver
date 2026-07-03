// K4 properties (PRD 12 §5): De Morgan, idempotence, involution, popcount conservation
// popcount(and) + popcount(andnot) ≡ popcount(a).
// Covers: REQ-TEST-008, REQ-K4-001
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <gtest/gtest.h>

#include "quiver/mask.h"
#include "tests/testkit/generators.h"
#include "tests/testkit/reference.h"

namespace {

using quiver_test::Rng;
namespace ref = quiver_test::ref;

TEST(PropMask, AlgebraicIdentities) {
  Rng rng(0x9E0904);
  for (int iter = 0; iter < (std::getenv("QUIVER_NIGHTLY") ? 400 : 60); ++iter) {
    const std::int64_t n = 1 + static_cast<std::int64_t>(rng.next_below(2000));
    const std::size_t bytes = (static_cast<std::size_t>(n) + 7) / 8;
    std::vector<std::uint8_t> a(bytes);
    std::vector<std::uint8_t> b(bytes);
    quiver_test::fill_bitmap_uniform(rng, a.data(), n, 45);
    quiver_test::fill_bitmap_uniform(rng, b.data(), n, 55);
    const quiver::BitmapView va{a.data()};
    const quiver::BitmapView vb{b.data()};
    std::vector<std::uint8_t> t1(bytes);
    std::vector<std::uint8_t> t2(bytes);
    std::vector<std::uint8_t> t3(bytes);

    // De Morgan: NOT(a AND b) == NOT a OR NOT b.
    quiver::mask_combine(quiver::MaskOp::kAnd, va, vb, n, t1.data());
    quiver::mask_not(quiver::BitmapView{t1.data()}, n, t1.data());
    quiver::mask_not(va, n, t2.data());
    quiver::mask_not(vb, n, t3.data());
    quiver::mask_combine(quiver::MaskOp::kOr, quiver::BitmapView{t2.data()},
                         quiver::BitmapView{t3.data()}, n, t2.data());
    ASSERT_EQ(std::memcmp(t1.data(), t2.data(), bytes), 0);

    // Idempotence: a AND a == a.
    quiver::mask_combine(quiver::MaskOp::kAnd, va, va, n, t1.data());
    ASSERT_EQ(std::memcmp(t1.data(), a.data(), bytes), 0);

    // Involution: NOT NOT a == a.
    quiver::mask_not(va, n, t1.data());
    quiver::mask_not(quiver::BitmapView{t1.data()}, n, t1.data());
    ASSERT_EQ(std::memcmp(t1.data(), a.data(), bytes), 0);

    // Popcount conservation: |a∧b| + |a∧¬b| == |a|.
    quiver::mask_combine(quiver::MaskOp::kAnd, va, vb, n, t1.data());
    quiver::mask_combine(quiver::MaskOp::kAndNot, va, vb, n, t2.data());
    ASSERT_EQ(quiver::mask_popcount(quiver::BitmapView{t1.data()}, n) +
                  quiver::mask_popcount(quiver::BitmapView{t2.data()}, n),
              quiver::mask_popcount(va, n));
  }
}

}  // namespace
