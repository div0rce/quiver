// K5 unit tests: identity/reverse/duplicate takes, dict decode, fused packed decode that
// must not touch unselected code positions (guard-page validated in the invariant suite).
// Covers: REQ-K5-001..003 (PRD 08 §5 K5)
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "quiver/take.h"

namespace {

using quiver::BatchView;
using quiver::SelVec;

TEST(Take, IdentityReverseAndDuplicates) {  // REQ-K5-001: arbitrary order + duplicates legal
  const std::int64_t v[] = {10, 11, 12, 13, 14};
  {
    const std::uint32_t idx[] = {0, 1, 2, 3, 4};
    std::int64_t out[5];
    quiver::take(BatchView<std::int64_t>{v, 5}, SelVec{idx, 5}, out);
    for (int i = 0; i < 5; ++i) {
      ASSERT_EQ(out[i], v[i]);
    }
  }
  {
    const std::uint32_t idx[] = {4, 3, 2, 1, 0};
    std::int64_t out[5];
    quiver::take(BatchView<std::int64_t>{v, 5}, SelVec{idx, 5}, out);
    for (int i = 0; i < 5; ++i) {
      ASSERT_EQ(out[i], v[4 - i]);
    }
  }
  {
    const std::uint32_t idx[] = {2, 2, 2, 0, 4, 4, 1};
    std::int64_t out[7];
    quiver::take(BatchView<std::int64_t>{v, 5}, SelVec{idx, 7}, out);
    const std::int64_t want[] = {12, 12, 12, 10, 14, 14, 11};
    for (int i = 0; i < 7; ++i) {
      ASSERT_EQ(out[i], want[i]);
    }
  }
}

TEST(Take, DictDecodeAllCodeWidths) {
  const float dict[] = {1.5F, 2.5F, 3.5F, 4.5F};
  {
    const std::uint8_t codes[] = {3, 0, 2, 2, 1};
    float out[5];
    quiver::dict_decode(BatchView<float>{dict, 4}, codes, 5, out);
    EXPECT_EQ(out[0], 4.5F);
    EXPECT_EQ(out[4], 2.5F);
  }
  {
    const std::uint16_t codes[] = {1, 1, 0};
    float out[3];
    quiver::dict_decode(BatchView<float>{dict, 4}, codes, 3, out);
    EXPECT_EQ(out[0], 2.5F);
    EXPECT_EQ(out[2], 1.5F);
  }
  {
    const std::uint32_t codes[] = {2};
    float out[1];
    quiver::dict_decode(BatchView<float>{dict, 4}, codes, 1, out);
    EXPECT_EQ(out[0], 3.5F);
  }
}

TEST(Take, FusedDecodeIsPacked) {  // REQ-K5-003 packing semantics
  const std::uint32_t dict[] = {100, 200, 300};
  const std::uint8_t codes[] = {0, 1, 2, 1, 0, 2};
  const std::uint32_t sel[] = {1, 4, 5};
  std::uint32_t out[3];
  quiver::dict_decode(BatchView<std::uint32_t>{dict, 3}, codes, 6, SelVec{sel, 3}, out);
  EXPECT_EQ(out[0], 200u);
  EXPECT_EQ(out[1], 100u);
  EXPECT_EQ(out[2], 300u);
}

TEST(Take, EmptyIndexList) {
  const std::int8_t v[] = {1};
  std::int8_t out[1] = {42};
  quiver::take(BatchView<std::int8_t>{v, 1}, SelVec{nullptr, 0}, out);
  EXPECT_EQ(out[0], 42);  // nothing written
}

}  // namespace
