// K3 unit tests: hand-computed conversions, tail zeroing, popcount agreement.
// Covers: REQ-K3-001..002 (PRD 08 §5 K3)
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "quiver/mask.h"
#include "quiver/select.h"
#include "tests/testkit/reference.h"

namespace {

using quiver::BitmapView;
using quiver::SelVec;

TEST(Select, BitmapToSelvecHandComputed) {
  const std::uint8_t bits[] = {0x8C, 0x01};  // 2,3,7,8
  std::uint32_t idx[9];
  ASSERT_EQ(quiver::bitmap_to_selvec(BitmapView{bits}, 9, idx), 4);
  EXPECT_EQ(idx[0], 2u);
  EXPECT_EQ(idx[1], 3u);
  EXPECT_EQ(idx[2], 7u);
  EXPECT_EQ(idx[3], 8u);
}

TEST(Select, CountAgreesWithMaskPopcount) {  // REQ-K3-002
  const std::uint8_t bits[] = {0xF0, 0x0F, 0xAA};
  std::uint32_t idx[24];
  EXPECT_EQ(quiver::bitmap_to_selvec(BitmapView{bits}, 20, idx),
            quiver::mask_popcount(BitmapView{bits}, 20));
}

TEST(Select, SelvecToBitmapZeroesTailAndNonSelected) {
  const std::uint32_t sel[] = {0, 9};
  std::uint8_t bits[2] = {0xFF, 0xFF};
  quiver::selvec_to_bitmap(SelVec{sel, 2}, 11, bits);
  EXPECT_EQ(bits[0], 0x01);
  EXPECT_EQ(bits[1], 0x02);  // bit 9 set; bits 11..15 zero (ADR-016)
  EXPECT_TRUE(quiver_test::ref::tail_bits_zero(bits, 11));
}

TEST(Select, EmptyInputs) {
  std::uint32_t idx[1];
  const std::uint8_t bits[1] = {0x00};
  EXPECT_EQ(quiver::bitmap_to_selvec(BitmapView{bits}, 0, idx), 0);
  std::uint8_t out[1] = {0xFF};
  quiver::selvec_to_bitmap(SelVec{nullptr, 0}, 3, out);
  EXPECT_EQ(out[0], 0x00);
}

}  // namespace
