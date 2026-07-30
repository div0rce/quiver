// K6 properties (PRD 12 §5): sum(selvec) ≡ sum(filter → no-selection); min ≤ participants;
// SMA ≡ (min, max, null-count) composition; checked-sum flag ≡ big-accumulator reference.
// Covers: REQ-TEST-008, REQ-K6-001..005
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <gtest/gtest.h>

#include "quiver/filter.h"
#include "quiver/reduce.h"
#include "quiver/select.h"
#include "tests/testkit/generators.h"
#include "tests/testkit/reference.h"

namespace {

using quiver_test::Rng;
namespace ref = quiver_test::ref;

// sum over a selection === filter by that selection, then sum without one.
void check_sum_over_selection(const quiver::BatchView<std::int64_t>& in,
                              const std::uint8_t* selbits, const quiver::SelVec& sel,
                              std::int64_t count) {
  std::vector<std::int64_t> filtered(static_cast<std::size_t>(in.len));
  quiver::filter(in, quiver::BitmapView{selbits}, filtered.data());
  EXPECT_EQ(quiver::reduce_sum_wrap(in, quiver::BitmapView{nullptr}, sel),
            quiver::reduce_sum_wrap(quiver::BatchView<std::int64_t>{filtered.data(), count},
                                    quiver::BitmapView{nullptr}));
}

// min and max bound every selected element, and the one-pass SMA agrees with both.
void check_minmax_and_sma(const quiver::BatchView<std::int64_t>& in, const quiver::SelVec& sel,
                          const std::vector<std::uint32_t>& idx, std::int64_t count) {
  if (count == 0) {
    return;
  }
  const std::int64_t mn = quiver::reduce_min(in, quiver::BitmapView{nullptr}, sel);
  const std::int64_t mx = quiver::reduce_max(in, quiver::BitmapView{nullptr}, sel);
  for (std::int64_t j = 0; j < count; ++j) {
    ASSERT_LE(mn, in.data[idx[static_cast<std::size_t>(j)]]);
    ASSERT_GE(mx, in.data[idx[static_cast<std::size_t>(j)]]);
  }
  const quiver::MinMaxSummary<std::int64_t> sma =
      quiver::compute_min_max(in, quiver::BitmapView{nullptr}, sel);
  EXPECT_EQ(sma.min, mn);
  EXPECT_EQ(sma.max, mx);
  EXPECT_EQ(sma.null_count, 0);
}

struct ExactSum {
  std::int64_t wrapped;
  bool overflowed;
};

// The EXACT sum and whether the final value is representable — never a per-step overflow flag,
// which would wrongly report [INT64_MAX, 1, -1] as overflowing (API-K6-003).
ExactSum exact_sum(const std::int64_t* v, std::int64_t n) {
  std::uint64_t lo = 0;
  std::int64_t hi = 0;
  for (std::int64_t i = 0; i < n; ++i) {
    const std::int64_t x = v[static_cast<std::size_t>(i)];
    const std::uint64_t nlo = lo + static_cast<std::uint64_t>(x);
    hi += (x < 0 ? -1 : 0) + (nlo < lo ? 1 : 0);
    lo = nlo;
  }
  return {static_cast<std::int64_t>(lo), hi != ((static_cast<std::int64_t>(lo) < 0) ? -1 : 0)};
}

// Checked-sum flag matches the exact reference above, computed here independently of the kernel.
//
// CAVEAT on oracle independence: __int128 is a GNU extension MSVC does not provide on any
// architecture (error C4235), so this reference uses an explicit (hi, lo) limb pair. On
// GCC/Clang that is a genuine differential — the kernel takes the __int128 branch while this
// takes limbs. On MSVC both sides are limb-based, so a shared misconception about the carry
// expression would not be caught HERE; the tier-1 legs of the CI matrix are what provide the
// independent cross-check for that path (reduce_scalar_impl.h, REQ-TEST-002).
void check_checked_sum(const quiver::BatchView<std::int64_t>& in) {
  std::int64_t wrapped = 0;
  const bool flag = quiver::reduce_sum_checked(in, quiver::BitmapView{nullptr}, &wrapped);
  const ExactSum want = exact_sum(in.data, in.len);
  EXPECT_EQ(flag, want.overflowed);
  EXPECT_EQ(wrapped, want.wrapped);
}

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

    check_sum_over_selection(in, selbits.data(), sel, count);
    check_minmax_and_sma(in, sel, idx, count);
    check_checked_sum(in);
  }
}

}  // namespace
