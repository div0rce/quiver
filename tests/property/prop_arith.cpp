// K9/K10 properties (PRD 12 §5): the K9-002 composition identity, checked-count ==
// popcount(position bitmap), saturating ≡ clamp(checked), and add/sub inverse round trips.
// Covers: REQ-K9-001..002, REQ-K10-001..002, REQ-TEST-008
#include <cstdint>
#include <cstring>
#include <vector>

#include <gtest/gtest.h>

#include "quiver/arith.h"
#include "quiver/mask.h"
#include "tests/testkit/generators.h"
#include "tests/testkit/reference.h"

namespace {

using quiver::ArithOp;
using quiver_test::Rng;
namespace ref = quiver_test::ref;

TEST(PropArith, ValidityOverloadEqualsComposition) {
  Rng rng(0xA91701ull);
  for (int iter = 0; iter < 20; ++iter) {
    const std::int64_t n = 1 + static_cast<std::int64_t>(rng.next_below(500));
    std::vector<std::int32_t> a(static_cast<std::size_t>(n));
    std::vector<std::int32_t> b(static_cast<std::size_t>(n));
    quiver_test::fill_uniform(rng, a.data(), n);
    quiver_test::fill_uniform(rng, b.data(), n);
    std::vector<std::uint8_t> av((static_cast<std::size_t>(n) + 7) / 8);
    std::vector<std::uint8_t> bv((static_cast<std::size_t>(n) + 7) / 8);
    quiver_test::fill_bitmap_uniform(rng, av.data(), n, 70);
    quiver_test::fill_bitmap_uniform(rng, bv.data(), n, 70);

    std::vector<std::int32_t> out(static_cast<std::size_t>(n));
    std::vector<std::uint8_t> out_v(av.size());
    quiver::arith(ArithOp::kMul, quiver::BatchView<std::int32_t>{a.data(), n},
                  quiver::BatchView<std::int32_t>{b.data(), n}, quiver::BitmapView{av.data()},
                  quiver::BitmapView{bv.data()}, out.data(), out_v.data());

    std::vector<std::int32_t> values(static_cast<std::size_t>(n));
    quiver::arith(ArithOp::kMul, quiver::BatchView<std::int32_t>{a.data(), n},
                  quiver::BatchView<std::int32_t>{b.data(), n}, values.data());
    std::vector<std::uint8_t> masks(av.size());
    quiver::mask_combine(quiver::MaskOp::kAnd, quiver::BitmapView{av.data()},
                         quiver::BitmapView{bv.data()}, n, masks.data());
    ASSERT_EQ(std::memcmp(out.data(), values.data(), values.size() * 4), 0);
    ASSERT_EQ(std::memcmp(out_v.data(), masks.data(), masks.size()), 0);
  }
}

TEST(PropArith, CheckedCountEqualsBitmapPopcount) {
  Rng rng(0xA91702ull);
  for (const ArithOp op : {ArithOp::kAdd, ArithOp::kSub, ArithOp::kMul}) {
    const std::int64_t n = 777;
    std::vector<std::int8_t> a(static_cast<std::size_t>(n));
    std::vector<std::int8_t> b(static_cast<std::size_t>(n));
    quiver_test::fill_uniform(rng, a.data(), n);  // i8 saturates the space -> many overflows
    quiver_test::fill_uniform(rng, b.data(), n);
    std::vector<std::int8_t> out(static_cast<std::size_t>(n));
    std::vector<std::uint8_t> bits((static_cast<std::size_t>(n) + 7) / 8);
    const std::int64_t count =
        quiver::arith_checked(op, quiver::BatchView<std::int8_t>{a.data(), n},
                              quiver::BatchView<std::int8_t>{b.data(), n}, out.data(), bits.data());
    ASSERT_EQ(count, ref::popcount_bits(bits.data(), n));
    ASSERT_TRUE(ref::tail_bits_zero(bits.data(), n));
    // and the nullable form returns the same count
    std::vector<std::int8_t> out2(static_cast<std::size_t>(n));
    ASSERT_EQ(quiver::arith_checked(op, quiver::BatchView<std::int8_t>{a.data(), n},
                                    quiver::BatchView<std::int8_t>{b.data(), n}, out2.data(),
                                    nullptr),
              count);
    ASSERT_EQ(std::memcmp(out.data(), out2.data(), out.size()), 0);
  }
}

TEST(PropArith, SaturatingEqualsClampOfChecked) {
  Rng rng(0xA91703ull);
  for (const ArithOp op : {ArithOp::kAdd, ArithOp::kSub, ArithOp::kMul}) {
    const std::int64_t n = 555;
    std::vector<std::int16_t> a(static_cast<std::size_t>(n));
    std::vector<std::int16_t> b(static_cast<std::size_t>(n));
    quiver_test::fill_uniform(rng, a.data(), n);
    quiver_test::fill_uniform(rng, b.data(), n);
    std::vector<std::int16_t> sat(static_cast<std::size_t>(n));
    quiver::arith_saturating(op, quiver::BatchView<std::int16_t>{a.data(), n},
                             quiver::BatchView<std::int16_t>{b.data(), n}, sat.data());
    for (std::int64_t i = 0; i < n; ++i) {
      ASSERT_EQ(sat[static_cast<std::size_t>(i)],
                ref::arith_saturate_expected(op, a[static_cast<std::size_t>(i)],
                                             b[static_cast<std::size_t>(i)]))
          << "op=" << static_cast<int>(op) << " i=" << i;
    }
  }
}

TEST(PropArith, AddSubRoundTrip) {
  Rng rng(0xA91704ull);
  const std::int64_t n = 1024;
  std::vector<std::uint64_t> a(static_cast<std::size_t>(n));
  std::vector<std::uint64_t> b(static_cast<std::size_t>(n));
  quiver_test::fill_uniform(rng, a.data(), n);
  quiver_test::fill_uniform(rng, b.data(), n);
  std::vector<std::uint64_t> t(static_cast<std::size_t>(n));
  quiver::arith(ArithOp::kAdd, quiver::BatchView<std::uint64_t>{a.data(), n},
                quiver::BatchView<std::uint64_t>{b.data(), n}, t.data());
  quiver::arith(ArithOp::kSub, quiver::BatchView<std::uint64_t>{t.data(), n},
                quiver::BatchView<std::uint64_t>{b.data(), n}, t.data());
  ASSERT_EQ(std::memcmp(t.data(), a.data(), t.size() * 8), 0);  // wrap is a group
}

}  // namespace
