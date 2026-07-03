// K2 unit tests: order preservation, exact counts, in-place aliasing equivalence (ADR-023),
// selvec-driven form.
// Covers: REQ-K2-001..003, REQ-MEM-005 (PRD 08 §5 K2)
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "quiver/filter.h"
#include "tests/testkit/reference.h"

namespace {

using quiver::BatchView;
using quiver::BitmapView;
using quiver::SelVec;

TEST(Filter, HandComputedAndOrderPreserving) {
  const std::int32_t v[] = {10, 20, 30, 40, 50, 60, 70, 80};
  const std::uint8_t sel[] = {0xA5};  // 0,2,5,7
  std::int32_t out[8];
  EXPECT_EQ(quiver::filter(BatchView<std::int32_t>{v, 8}, BitmapView{sel}, out), 4);
  EXPECT_EQ(out[0], 10);
  EXPECT_EQ(out[1], 30);
  EXPECT_EQ(out[2], 60);
  EXPECT_EQ(out[3], 80);
}

TEST(Filter, InPlaceEqualsOutOfPlace) {  // ADR-023 exact-alias row
  std::vector<std::uint64_t> v(257);
  for (std::size_t i = 0; i < v.size(); ++i) {
    v[i] = i * 3;
  }
  std::vector<std::uint8_t> sel((v.size() + 7) / 8, 0x93);
  std::vector<std::uint64_t> copy = v;
  std::vector<std::uint64_t> out(v.size());
  const auto n = static_cast<std::int64_t>(v.size());
  const std::int64_t c1 =
      quiver::filter(BatchView<std::uint64_t>{copy.data(), n}, BitmapView{sel.data()}, out.data());
  const std::int64_t c2 =
      quiver::filter(BatchView<std::uint64_t>{v.data(), n}, BitmapView{sel.data()}, v.data());
  ASSERT_EQ(c1, c2);
  for (std::int64_t i = 0; i < c1; ++i) {
    ASSERT_EQ(v[static_cast<std::size_t>(i)], out[static_cast<std::size_t>(i)]) << i;
  }
}

TEST(Filter, SelvecDrivenWritesExactly) {
  const double v[] = {0.5, 1.5, 2.5, 3.5};
  const std::uint32_t idx[] = {1, 3};
  double out[2];
  EXPECT_EQ(quiver::filter(BatchView<double>{v, 4}, SelVec{idx, 2}, out), 2);
  EXPECT_EQ(out[0], 1.5);
  EXPECT_EQ(out[1], 3.5);
}

TEST(Filter, EmptySelectionAndEmptyBatch) {
  const std::int16_t v[] = {1, 2, 3};
  const std::uint8_t none[] = {0x00};
  std::int16_t out[3];
  EXPECT_EQ(quiver::filter(BatchView<std::int16_t>{v, 3}, BitmapView{none}, out), 0);
  EXPECT_EQ(quiver::filter(BatchView<std::int16_t>{v, 3}, SelVec{nullptr, 0}, out), 0);
}

}  // namespace
