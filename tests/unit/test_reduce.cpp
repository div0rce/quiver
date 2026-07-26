// K6 unit tests: participation rule (selected AND valid), identity results, wrapping and
// checked sums (incl. 64-bit boundary overflow), NaN propagation to the canonical qNaN,
// ±0.0 determinism, SMA composition, count_valid delegation.
// Covers: REQ-K6-001..005 (PRD 08 §3/§5 K6; ADR-013)
#include <bit>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "quiver/compare.h"
#include "quiver/reduce.h"
#include "quiver/take.h"
#include "tests/testkit/reference.h"

namespace {

using quiver::BatchView;
using quiver::BitmapView;
using quiver::MinMaxSummary;
using quiver::SelVec;

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
  const MinMaxSummary<std::int32_t> sma =
      quiver::compute_min_max(BatchView<std::int32_t>{v, 5}, BitmapView{validity}, SelVec{sel, 3});
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

// --- Convenience layer (PRD 04 §3.6, ADR-027): forwards must agree exactly with the
// --- buffer-oriented primitives, and the review's target pipeline must compile as written.

TEST(Reduce, ConvenienceFormsAgreeWithPrimitives) {
  const std::vector<std::int32_t> v = {5, 1, 9, 3, 7, 2};
  const auto bv = quiver::batch_view(v);

  // Validity defaults + range forms == explicit all-valid primitives.
  EXPECT_EQ(quiver::reduce_min(bv), quiver::reduce_min(bv, quiver::BitmapView{nullptr}));
  EXPECT_EQ(quiver::reduce_max(v), quiver::reduce_max(bv, quiver::BitmapView{nullptr}));
  EXPECT_EQ(quiver::reduce_sum_wrap(v), quiver::reduce_sum_wrap(bv, quiver::BitmapView{nullptr}));
  const auto mm = quiver::compute_min_max(v);
  EXPECT_EQ(mm.min, 1);
  EXPECT_EQ(mm.max, 9);
  EXPECT_EQ(mm.null_count, 0);

  // CheckedSum: value + named flag agree with the pointer-out primitive.
  quiver::SumType<std::int32_t> raw = 0;
  const bool overflowed = quiver::reduce_sum_checked(bv, quiver::all_valid, &raw);
  const auto cs = quiver::sum_checked(v);
  EXPECT_EQ(cs.overflowed, overflowed);
  EXPECT_EQ(cs.value, raw);
  EXPECT_FALSE(cs.overflowed);
  EXPECT_EQ(cs.value, 27);
}

TEST(Reduce, TargetPipelineCompilesAsReviewed) {
  // The ergonomics-review target syntax, verbatim shape: storage is explicit, results carry
  // their counts, and no BitmapView{nullptr} or manual size bookkeeping appears anywhere.
  const std::vector<std::int32_t> values = {5, 1, 9, 3, 7, 2, 8, 4, 6, 0};

  std::vector<std::uint32_t> selection_storage(values.size());
  const auto selected =
      quiver::compare_selvec(quiver::CompareOp::kGe, values, 5, std::span{selection_storage});
  EXPECT_EQ(selected.size(), 5u);  // 5, 9, 7, 8, 6

  std::vector<std::int32_t> picked_storage(selected.size());
  const auto picked = quiver::take(values, selected, std::span{picked_storage});
  EXPECT_EQ(picked.size(), selected.size());

  const auto total = quiver::reduce_sum_wrap(picked);
  EXPECT_EQ(total, 5 + 9 + 7 + 8 + 6);

  // Deprecated names still compile and forward (removed at v1.0).
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)  // calling the deprecated alias IS the point of this test
#endif
  const quiver::Sma<std::int32_t> old_name =
      quiver::compute_sma(quiver::batch_view(values), quiver::all_valid);
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif
  EXPECT_EQ(old_name.min, 0);
  EXPECT_EQ(old_name.max, 9);
}

}  // namespace
