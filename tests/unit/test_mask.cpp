// K4 unit tests: exhaustive small-n truth tables vs the naive oracle, vacuous truths,
// tail behavior, in-place aliasing.
// Covers: REQ-K4-001..003 (PRD 08 §5 K4, PRD 12 §5)
#include <cstdint>
#include <cstdlib>
#include <vector>

#include <gtest/gtest.h>

#include "quiver/mask.h"
#include "tests/testkit/generators.h"
#include "tests/testkit/reference.h"

namespace {

using quiver::BitmapView;
using quiver::MaskOp;
namespace ref = quiver_test::ref;

TEST(Mask, ExhaustiveSmallLengthsVsOracle) {  // n = 0..129 (REQ-TEST rows)
  quiver_test::Rng rng(0xC0FFEE);
  for (std::int64_t n = 0; n <= 129; ++n) {
    const std::int64_t bytes = (n + 7) / 8;
    std::vector<std::uint8_t> a(static_cast<std::size_t>(bytes) + 1);
    std::vector<std::uint8_t> b(static_cast<std::size_t>(bytes) + 1);
    quiver_test::fill_bitmap_uniform(rng, a.data(), n, 50);
    quiver_test::fill_bitmap_uniform(rng, b.data(), n, 50);
    std::vector<std::uint8_t> out(static_cast<std::size_t>(bytes) + 1);
    for (const MaskOp op : {MaskOp::kAnd, MaskOp::kOr, MaskOp::kAndNot, MaskOp::kXor}) {
      quiver::mask_combine(op, BitmapView{a.data()}, BitmapView{b.data()}, n, out.data());
      for (std::int64_t i = 0; i < n; ++i) {
        const bool av = ref::bit_get(a.data(), i);
        const bool bv = ref::bit_get(b.data(), i);
        bool want = false;
        switch (op) {
        case MaskOp::kAnd:
          want = av && bv;
          break;
        case MaskOp::kOr:
          want = av || bv;
          break;
        case MaskOp::kAndNot:
          want = av && !bv;
          break;
        case MaskOp::kXor:
          want = av != bv;
          break;
        }
        ASSERT_EQ(ref::bit_get(out.data(), i), want)
            << "op=" << int(op) << " n=" << n << " i=" << i;
      }
      ASSERT_TRUE(ref::tail_bits_zero(out.data(), n)) << "n=" << n;
    }
    quiver::mask_not(BitmapView{a.data()}, n, out.data());
    for (std::int64_t i = 0; i < n; ++i) {
      ASSERT_EQ(ref::bit_get(out.data(), i), !ref::bit_get(a.data(), i));
    }
    ASSERT_TRUE(ref::tail_bits_zero(out.data(), n));
    ASSERT_EQ(quiver::mask_popcount(BitmapView{a.data()}, n), ref::popcount_bits(a.data(), n));
  }
}

TEST(Mask, VacuousTruthsAtZeroLength) {  // PRD 04 K4
  const std::uint8_t bits[1] = {0x00};
  EXPECT_TRUE(quiver::mask_all(BitmapView{bits}, 0));
  EXPECT_FALSE(quiver::mask_any(BitmapView{bits}, 0));
  EXPECT_TRUE(quiver::mask_none(BitmapView{bits}, 0));
}

TEST(Mask, AllAnyNoneRespectTailBits) {
  const std::uint8_t full_low[] = {0x0F};  // bits 0..3 set
  EXPECT_TRUE(quiver::mask_all(BitmapView{full_low}, 4));
  EXPECT_FALSE(quiver::mask_all(BitmapView{full_low}, 5));
  const std::uint8_t high_only[] = {0xF0};
  EXPECT_FALSE(quiver::mask_any(BitmapView{high_only}, 4));  // set bits are beyond n
  EXPECT_TRUE(quiver::mask_none(BitmapView{high_only}, 4));
}

TEST(Mask, InPlaceAliasing) {  // ADR-023
  std::uint8_t a[] = {0xCC, 0x33};
  const std::uint8_t b[] = {0xAA, 0x55};
  std::uint8_t expect[2];
  quiver::mask_combine(MaskOp::kXor, BitmapView{a}, BitmapView{b}, 16, expect);
  quiver::mask_combine(MaskOp::kXor, BitmapView{a}, BitmapView{b}, 16, a);  // out == a
  EXPECT_EQ(a[0], expect[0]);
  EXPECT_EQ(a[1], expect[1]);
}

}  // namespace
