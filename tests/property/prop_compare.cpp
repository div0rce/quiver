// K1 properties (PRD 12 §5): bitmap ≡ selvec (via K3), popcount return ≡ K4 popcount,
// kNe ≡ NOT(kEq) for integers, between ≡ (ge lo) ∧ (le hi).
// Covers: REQ-TEST-008, REQ-K1-001/-003
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <gtest/gtest.h>

#include "quiver/compare.h"
#include "quiver/mask.h"
#include "quiver/select.h"
#include "tests/testkit/generators.h"
#include "tests/testkit/reference.h"

namespace {

using quiver_test::Rng;
namespace ref = quiver_test::ref;

TEST(PropCompare, RepresentationsAgreeAndComposeWithK4) {
  Rng rng(0x9E0901);
  for (int iter = 0; iter < (std::getenv("QUIVER_NIGHTLY") ? 200 : 40); ++iter) {
    const std::int64_t n = 1 + static_cast<std::int64_t>(rng.next_below(1500));
    std::vector<std::int32_t> v(static_cast<std::size_t>(n));
    quiver_test::fill_uniform(rng, v.data(), n);
    const std::int32_t c = v[rng.next_below(static_cast<std::uint64_t>(n))];
    const std::int64_t bytes = (n + 7) / 8;
    std::vector<std::uint8_t> bits(static_cast<std::size_t>(bytes));
    std::vector<std::uint32_t> idx_direct(static_cast<std::size_t>(n));
    std::vector<std::uint32_t> idx_via(static_cast<std::size_t>(n));
    const quiver::BatchView<std::int32_t> in{v.data(), n};

    const std::int64_t cnt_bm = quiver::compare_bitmap(quiver::CompareOp::kGt, in, c,
                                                       quiver::BitmapView{nullptr}, bits.data());
    const std::int64_t cnt_sv = quiver::compare_selvec(
        quiver::CompareOp::kGt, in, c, quiver::BitmapView{nullptr}, idx_direct.data());
    ASSERT_EQ(cnt_bm, cnt_sv);
    ASSERT_EQ(cnt_bm, quiver::mask_popcount(quiver::BitmapView{bits.data()}, n));
    ASSERT_EQ(quiver::bitmap_to_selvec(quiver::BitmapView{bits.data()}, n, idx_via.data()), cnt_bm);
    ASSERT_EQ(std::memcmp(idx_direct.data(), idx_via.data(), static_cast<std::size_t>(cnt_bm) * 4),
              0);

    // kNe ≡ NOT(kEq) for integers.
    std::vector<std::uint8_t> eq(static_cast<std::size_t>(bytes));
    std::vector<std::uint8_t> ne(static_cast<std::size_t>(bytes));
    std::vector<std::uint8_t> noteq(static_cast<std::size_t>(bytes));
    quiver::compare_bitmap(quiver::CompareOp::kEq, in, c, quiver::BitmapView{nullptr}, eq.data());
    quiver::compare_bitmap(quiver::CompareOp::kNe, in, c, quiver::BitmapView{nullptr}, ne.data());
    quiver::mask_not(quiver::BitmapView{eq.data()}, n, noteq.data());
    ASSERT_EQ(std::memcmp(ne.data(), noteq.data(), static_cast<std::size_t>(bytes)), 0);

    // between ≡ (ge lo) ∧ (le hi).
    const std::int32_t lo = std::min(c, v[0]);
    const std::int32_t hi = std::max(c, v[0]);
    std::vector<std::uint8_t> btw(static_cast<std::size_t>(bytes));
    std::vector<std::uint8_t> ge(static_cast<std::size_t>(bytes));
    std::vector<std::uint8_t> le(static_cast<std::size_t>(bytes));
    std::vector<std::uint8_t> composed(static_cast<std::size_t>(bytes));
    quiver::compare_between_bitmap(in, lo, hi, quiver::BitmapView{nullptr}, btw.data());
    quiver::compare_bitmap(quiver::CompareOp::kGe, in, lo, quiver::BitmapView{nullptr}, ge.data());
    quiver::compare_bitmap(quiver::CompareOp::kLe, in, hi, quiver::BitmapView{nullptr}, le.data());
    quiver::mask_combine(quiver::MaskOp::kAnd, quiver::BitmapView{ge.data()},
                         quiver::BitmapView{le.data()}, n, composed.data());
    ASSERT_EQ(std::memcmp(btw.data(), composed.data(), static_cast<std::size_t>(bytes)), 0);
  }
}

}  // namespace
