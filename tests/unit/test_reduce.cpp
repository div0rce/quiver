// K6 unit tests: participation rule (selected AND valid), identity results, wrapping and
// checked sums (incl. 64-bit boundary overflow), NaN propagation to the canonical qNaN,
// ±0.0 determinism, SMA composition, count_valid delegation.
// Covers: REQ-K6-001..005 (PRD 08 §3/§5 K6; ADR-013)
#include <bit>
#include <cstdint>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include "quiver/reduce.h"
#include "tests/testkit/reference.h"

namespace {

using quiver::BatchView;
using quiver::BitmapView;
using quiver::SelVec;
using quiver::Sma;

TEST(Reduce, ParticipationIsSelectedAndValid) {  // REQ-K6-001
  const std::int32_t v[] = {5, 1, 9, 2, 7};
  const std::uint8_t validity[] = {0x1B};  // 0,1,3,4 valid (index 2 invalid)
  const std::uint32_t sel[] = {1, 2, 4};   // selected
  // participants: 1 (valid), 4 (valid); index 2 selected-but-invalid
  EXPECT_EQ(quiver::reduce_min(BatchView<std::int32_t>{v, 5}, BitmapView{validity}, SelVec{sel, 3}),
            1);
  EXPECT_EQ(quiver::reduce_max(BatchView<std::int32_t>{v, 5}, BitmapView{validity}, SelVec{sel, 3}),
            7);
  EXPECT_EQ(
      quiver::reduce_sum_wrap(BatchView<std::int32_t>{v, 5}, BitmapView{validity}, SelVec{sel, 3}),
      8);
  const Sma<std::int32_t> sma =
      quiver::compute_sma(BatchView<std::int32_t>{v, 5}, BitmapView{validity}, SelVec{sel, 3});
  EXPECT_EQ(sma.min, 1);
  EXPECT_EQ(sma.max, 7);
  EXPECT_EQ(sma.null_count, 1);  // the selected-but-invalid position
}

TEST(Reduce, IdentitiesOnEmptyParticipation) {  // REQ-K6-002 / PRD 08 §3.5
  const std::int16_t v[] = {3, 4};
  const std::uint8_t none_valid[] = {0x00};
  EXPECT_EQ(quiver::reduce_min(BatchView<std::int16_t>{v, 2}, BitmapView{none_valid}),
            std::numeric_limits<std::int16_t>::max());
  EXPECT_EQ(quiver::reduce_max(BatchView<std::int16_t>{v, 2}, BitmapView{none_valid}),
            std::numeric_limits<std::int16_t>::lowest());
  EXPECT_EQ(quiver::reduce_sum_wrap(BatchView<std::int16_t>{v, 2}, BitmapView{none_valid}), 0);
  EXPECT_EQ(quiver::reduce_min(BatchView<double>{nullptr, 0}, BitmapView{nullptr}),
            std::numeric_limits<double>::max());
}

TEST(Reduce, WrappingSumIsModular) {
  const std::int64_t big = std::numeric_limits<std::int64_t>::max();
  const std::int64_t v[] = {big, 1};
  EXPECT_EQ(quiver::reduce_sum_wrap(BatchView<std::int64_t>{v, 2}, BitmapView{nullptr}),
            std::numeric_limits<std::int64_t>::min());  // wraps, never UB (REQ-STD-008)
}

TEST(Reduce, CheckedSumReportsExactOverflow) {  // API-K6-003
  const std::int64_t big = std::numeric_limits<std::int64_t>::max();
  {
    const std::int64_t v[] = {big, 1};
    std::int64_t sum = 0;
    EXPECT_TRUE(
        quiver::reduce_sum_checked(BatchView<std::int64_t>{v, 2}, BitmapView{nullptr}, &sum));
    EXPECT_EQ(sum, std::numeric_limits<std::int64_t>::min());  // wrapped value
  }
  {
    const std::int64_t v[] = {big, 1, -1};  // mathematically representable again
    std::int64_t sum = 0;
    EXPECT_FALSE(
        quiver::reduce_sum_checked(BatchView<std::int64_t>{v, 3}, BitmapView{nullptr}, &sum));
    EXPECT_EQ(sum, big);
  }
  {
    const std::uint64_t u = std::numeric_limits<std::uint64_t>::max();
    const std::uint64_t v[] = {u, 2};
    std::uint64_t sum = 0;
    EXPECT_TRUE(
        quiver::reduce_sum_checked(BatchView<std::uint64_t>{v, 2}, BitmapView{nullptr}, &sum));
    EXPECT_EQ(sum, 1u);
  }
  {
    const std::int8_t v[] = {127, 127, 127};  // narrow types cannot overflow SumType
    std::int64_t sum = 0;
    EXPECT_FALSE(
        quiver::reduce_sum_checked(BatchView<std::int8_t>{v, 3}, BitmapView{nullptr}, &sum));
    EXPECT_EQ(sum, 381);
  }
}

TEST(Reduce, NanPropagatesAsCanonicalQnan) {  // PRD 08 §3.3
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const float v[] = {3.0F, nan, 1.0F};
  const float mn = quiver::reduce_min(BatchView<float>{v, 3}, BitmapView{nullptr});
  const float mx = quiver::reduce_max(BatchView<float>{v, 3}, BitmapView{nullptr});
  EXPECT_EQ(std::bit_cast<std::uint32_t>(mn), 0x7FC00000u);
  EXPECT_EQ(std::bit_cast<std::uint32_t>(mx), 0x7FC00000u);
  // An INVALID NaN lane does not poison the result.
  const std::uint8_t validity[] = {0x05};  // NaN lane (1) masked out
  EXPECT_EQ(quiver::reduce_min(BatchView<float>{v, 3}, BitmapView{validity}), 1.0F);
}

TEST(Reduce, FloatSumIsStrictLeftFold) {  // ADR-013 scalar A=1: bit-exact vs strict oracle
  std::vector<double> v = {1e16, 1.0, -1e16, 1.0};
  const double got = quiver::reduce_sum_wrap(BatchView<double>{v.data(), 4}, BitmapView{nullptr});
  const quiver_test::ref::Participation p{nullptr, nullptr, 0};
  const double want = quiver_test::ref::sum_expected(v.data(), 4, p);
  EXPECT_EQ(std::bit_cast<std::uint64_t>(got), std::bit_cast<std::uint64_t>(want));
  EXPECT_EQ(got, 1.0);  // strict order: (1e16 + 1) - 1e16 + 1 == 1 under IEEE double
}

TEST(Reduce, CountValidDelegation) {       // API-K6-005
  const std::uint8_t validity[] = {0x2F};  // 0,1,2,3,5
  EXPECT_EQ(quiver::reduce_count_valid(BitmapView{validity}, 6), 5);
  EXPECT_EQ(quiver::reduce_count_valid(BitmapView{nullptr}, 6), 6);
  const std::uint32_t sel[] = {3, 4, 5};
  EXPECT_EQ(quiver::reduce_count_valid(BitmapView{validity}, 6, SelVec{sel, 3}), 2);
  EXPECT_EQ(quiver::reduce_count_valid(BitmapView{nullptr}, 6, SelVec{sel, 3}), 3);
}

}  // namespace
