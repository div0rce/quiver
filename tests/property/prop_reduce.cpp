// K6 properties (PRD 12 §5): sum(selvec) ≡ sum(filter → no-selection); min ≤ participants;
// SMA ≡ (min, max, null-count) composition; checked-sum flag ≡ big-accumulator reference.
// Covers: REQ-TEST-008, REQ-K6-001..005
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <gtest/gtest.h>

#include "tests/testkit/generators.h"
#include "tests/testkit/reference.h"
#include "quiver/filter.h"
#include "quiver/reduce.h"
#include "quiver/select.h"

namespace {

using quiver_test::Rng;
namespace ref = quiver_test::ref;


TEST(PropReduce, CompositionIdentities) {
  Rng rng(0x9E0906);
  for (int iter = 0; iter < (std::getenv("QUIVER_NIGHTLY") ? 200 : 40); ++iter) {
    const std::int64_t n = 1 + static_cast<std::int64_t>(rng.next_below(1200));
    std::vector<std::int64_t> v(static_cast<std::size_t>(n));
    quiver_test::fill_uniform(rng, v.data(), n);
    std::vector<std::uint8_t> selbits((static_cast<std::size_t>(n) + 7) / 8);
    quiver_test::fill_bitmap_uniform(rng, selbits.data(), n, 50);
    std::vector<std::uint32_t> idx(static_cast<std::size_t>(n));
    const std::int64_t count =
        quiver::bitmap_to_selvec(quiver::BitmapView{selbits.data()}, n, idx.data());
    const quiver::BatchView<std::int64_t> in{v.data(), n};
    const quiver::SelVec sel{idx.data(), count};

    // sum over selection ≡ filter then sum without selection.
    std::vector<std::int64_t> filtered(static_cast<std::size_t>(n));
    quiver::filter(in, quiver::BitmapView{selbits.data()}, filtered.data());
    EXPECT_EQ(quiver::reduce_sum_wrap(in, quiver::BitmapView{nullptr}, sel),
              quiver::reduce_sum_wrap(quiver::BatchView<std::int64_t>{filtered.data(), count},
                                      quiver::BitmapView{nullptr}));

    // min bound and SMA composition.
    if (count > 0) {
      const std::int64_t mn = quiver::reduce_min(in, quiver::BitmapView{nullptr}, sel);
      const std::int64_t mx = quiver::reduce_max(in, quiver::BitmapView{nullptr}, sel);
      for (std::int64_t j = 0; j < count; ++j) {
        ASSERT_LE(mn, v[idx[static_cast<std::size_t>(j)]]);
        ASSERT_GE(mx, v[idx[static_cast<std::size_t>(j)]]);
      }
      const quiver::Sma<std::int64_t> sma =
          quiver::compute_sma(in, quiver::BitmapView{nullptr}, sel);
      EXPECT_EQ(sma.min, mn);
      EXPECT_EQ(sma.max, mx);
      EXPECT_EQ(sma.null_count, 0);
    }

    // Checked-sum flag matches an independent 128-bit reference.
    std::int64_t wrapped = 0;
    const bool flag = quiver::reduce_sum_checked(in, quiver::BitmapView{nullptr}, &wrapped);
    __int128 big = 0;
    for (std::int64_t i = 0; i < n; ++i) {
      big += v[static_cast<std::size_t>(i)];
    }
    const bool want_flag = big > std::numeric_limits<std::int64_t>::max() ||
                           big < std::numeric_limits<std::int64_t>::min();
    EXPECT_EQ(flag, want_flag);
    EXPECT_EQ(wrapped, static_cast<std::int64_t>(big));
  }
}

}  // namespace
