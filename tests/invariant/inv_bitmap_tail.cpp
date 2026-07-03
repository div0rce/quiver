// Invariant: every bitmap-producing kernel zeroes tail bits (ADR-016) — the property that
// makes outputs memcmp-comparable across backends and runs.
// Covers: REQ-MEM-006/-008, REQ-TEST-005
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "quiver/compare.h"
#include "quiver/mask.h"
#include "quiver/select.h"
#include "tests/testkit/generators.h"
#include "tests/testkit/reference.h"

namespace {

TEST(InvBitmapTail, AllProducersZeroTails) {
  quiver_test::Rng rng(0x7A11);
  for (std::int64_t n = 1; n <= 130; ++n) {
    const std::size_t bytes = (static_cast<std::size_t>(n) + 7) / 8;
    std::vector<std::int32_t> v(static_cast<std::size_t>(n));
    quiver_test::fill_uniform(rng, v.data(), n);
    std::vector<std::uint8_t> a(bytes);
    std::vector<std::uint8_t> b(bytes);
    quiver_test::fill_bitmap_uniform(rng, a.data(), n, 50);
    quiver_test::fill_bitmap_uniform(rng, b.data(), n, 50);
    std::vector<std::uint8_t> out(bytes, 0xFF);

    quiver::compare_bitmap(quiver::CompareOp::kNe, quiver::BatchView<std::int32_t>{v.data(), n},
                           0, quiver::BitmapView{nullptr}, out.data());
    ASSERT_TRUE(quiver_test::ref::tail_bits_zero(out.data(), n)) << "K1 n=" << n;

    quiver::mask_not(quiver::BitmapView{a.data()}, n, out.data());  // NOT sets high bits
    ASSERT_TRUE(quiver_test::ref::tail_bits_zero(out.data(), n)) << "K4 not n=" << n;
    quiver::mask_combine(quiver::MaskOp::kXor, quiver::BitmapView{a.data()},
                         quiver::BitmapView{b.data()}, n, out.data());
    ASSERT_TRUE(quiver_test::ref::tail_bits_zero(out.data(), n)) << "K4 xor n=" << n;

    std::vector<std::uint32_t> idx(static_cast<std::size_t>(n));
    const std::int64_t cnt =
        quiver::bitmap_to_selvec(quiver::BitmapView{a.data()}, n, idx.data());
    std::fill(out.begin(), out.end(), 0xFF);
    quiver::selvec_to_bitmap(quiver::SelVec{idx.data(), cnt}, n, out.data());
    ASSERT_TRUE(quiver_test::ref::tail_bits_zero(out.data(), n)) << "K3 n=" << n;
  }
}

}  // namespace
