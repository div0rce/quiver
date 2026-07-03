// K1 unit tests: hand-computed cases, boundary/float-special blocks (IEEE semantics:
// NaN false except kNe; -0.0 == +0.0), validity fast path, two-batch AND rule.
// Covers: REQ-K1-001..003, REQ-API-006/-008 (PRD 08 §5 K1, PRD 12 §5)
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "quiver/compare.h"
#include "tests/testkit/reference.h"

namespace {

using quiver::BatchView;
using quiver::BitmapView;
using quiver::CompareOp;

TEST(Compare, HandComputedBitmap) {
  const std::int32_t v[] = {5, -1, 7, 7, 0, 100, -100, 7};
  std::uint8_t bits[1] = {0xFF};
  // v > 6 at indices 2,3,5,7 -> byte 0b10101100 = 0xAC
  EXPECT_EQ(quiver::compare_bitmap(CompareOp::kGt, BatchView<std::int32_t>{v, 8}, 6,
                                   BitmapView{nullptr}, bits),
            4);
  EXPECT_EQ(bits[0], 0xAC);
}

TEST(Compare, ValidityMasksOutMatches) {
  const std::int32_t v[] = {7, 7, 7, 7};
  const std::uint8_t validity[] = {0x05};  // indices 0, 2 valid
  std::uint8_t bits[1];
  EXPECT_EQ(quiver::compare_bitmap(CompareOp::kEq, BatchView<std::int32_t>{v, 4}, 7,
                                   BitmapView{validity}, bits),
            2);
  EXPECT_EQ(bits[0], 0x05);
}

TEST(Compare, TwoBatchValidityIsAnd) {
  const std::int64_t a[] = {1, 2, 3, 4};
  const std::int64_t b[] = {1, 2, 0, 4};
  const std::uint8_t av[] = {0x0B};  // 0,1,3
  const std::uint8_t bv[] = {0x0E};  // 1,2,3
  std::uint8_t bits[1];
  // equal at 0,1,3; a_valid AND b_valid = {1,3} -> both equal -> count 2, byte 0x0A
  EXPECT_EQ(quiver::compare_bitmap(CompareOp::kEq, BatchView<std::int64_t>{a, 4},
                                   BatchView<std::int64_t>{b, 4}, BitmapView{av}, BitmapView{bv},
                                   bits),
            2);
  EXPECT_EQ(bits[0], 0x0A);
}

TEST(Compare, FloatIeeeSemantics) {
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double v[] = {1.0, nan, -0.0, 3.0};
  std::uint8_t bits[1];
  // NaN: all ordered comparisons false...
  EXPECT_EQ(quiver::compare_bitmap(CompareOp::kEq, BatchView<double>{v, 4}, nan,
                                   BitmapView{nullptr}, bits),
            0);
  // ...except kNe, which is true for every lane when comparand is NaN.
  EXPECT_EQ(quiver::compare_bitmap(CompareOp::kNe, BatchView<double>{v, 4}, nan,
                                   BitmapView{nullptr}, bits),
            4);
  // -0.0 == +0.0 (PRD 08 §3.2).
  EXPECT_EQ(quiver::compare_bitmap(CompareOp::kEq, BatchView<double>{v, 4}, 0.0,
                                   BitmapView{nullptr}, bits),
            1);
  EXPECT_EQ(bits[0], 0x04);
}

TEST(Compare, BetweenIsInclusiveBothEnds) {
  const std::uint16_t v[] = {9, 10, 15, 20, 21};
  std::uint8_t bits[1];
  EXPECT_EQ(quiver::compare_between_bitmap(BatchView<std::uint16_t>{v, 5}, std::uint16_t{10},
                                           std::uint16_t{20}, BitmapView{nullptr}, bits),
            3);
  EXPECT_EQ(bits[0], 0x0E);
}

TEST(Compare, SelvecMatchesBitmapPositions) {
  const std::int8_t v[] = {1, 5, 1, 5, 5};
  std::uint32_t idx[5];
  const std::int64_t cnt = quiver::compare_selvec(CompareOp::kEq, BatchView<std::int8_t>{v, 5},
                                                  std::int8_t{5}, BitmapView{nullptr}, idx);
  ASSERT_EQ(cnt, 3);
  EXPECT_EQ(idx[0], 1u);
  EXPECT_EQ(idx[1], 3u);
  EXPECT_EQ(idx[2], 4u);
}

TEST(Compare, EmptyBatchIsDefined) {
  std::uint8_t bits[1] = {0xAA};
  std::uint32_t idx[1];
  EXPECT_EQ(quiver::compare_bitmap(CompareOp::kLt, BatchView<std::int32_t>{nullptr, 0}, 1,
                                   BitmapView{nullptr}, bits),
            0);
  EXPECT_EQ(quiver::compare_selvec(CompareOp::kLt, BatchView<std::int32_t>{nullptr, 0}, 1,
                                   BitmapView{nullptr}, idx),
            0);
}

}  // namespace
