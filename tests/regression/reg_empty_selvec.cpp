// Regression: an EMPTY SelVec{nullptr, 0} — the natural result of std::vector::data() on an
// empty vector — must mean "empty selection" (zero writes / identity reductions), NOT "no
// selection" (dense). The concrete K5/K6 symbols encode dense as sel == nullptr, and before
// the fix the façades forwarded the caller's null pointer straight through, so an empty
// selection silently processed ALL n elements (heap overflow in fused dict_decode; wrong
// values from selected reductions). Found by differential fuzzing at M4 (REQ-TEST-007);
// fixed by detail::nonnull_sel in the façades. First defect fix — activates the regression
// suite per REQ-TEST-011.
// Covers: REQ-TEST-011, REQ-API-008, API-K5-001/-003, API-K6-001..005
#include <cstdint>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include "quiver/dispatch.h"
#include "quiver/filter.h"
#include "quiver/reduce.h"
#include "quiver/select.h"
#include "quiver/take.h"

namespace {

constexpr quiver::SelVec kEmptySel{nullptr, 0};

template <class Body>
void for_each_backend(Body body) {
  for (const quiver::Isa isa :
       {quiver::Isa::kScalar, quiver::Isa::kNeon, quiver::Isa::kAvx2, quiver::Isa::kAvx512}) {
    if (isa != quiver::Isa::kScalar && !quiver::cpu_supports(isa)) {
      continue;
    }
    ASSERT_TRUE(quiver::set_isa_override(isa));
    body();
  }
  quiver::clear_isa_override();
}

TEST(RegEmptySelvec, TakeAndFusedDecodeWriteNothing) {
  const std::int64_t n = 64;
  std::vector<std::int64_t> dict(31, 7);
  std::vector<std::uint16_t> codes(static_cast<std::size_t>(n), 3);
  for_each_backend([&] {
    const std::int64_t sentinel = 0x5AFE5AFE5AFE5AFEll;
    std::vector<std::int64_t> out(4, sentinel);
    quiver::take(quiver::BatchView<std::int64_t>{dict.data(), 31}, kEmptySel, out.data());
    for (const std::int64_t x : out) {
      ASSERT_EQ(x, sentinel);  // zero elements written
    }
    quiver::dict_decode(quiver::BatchView<std::int64_t>{dict.data(), 31}, codes.data(), n,
                        kEmptySel, out.data());
    for (const std::int64_t x : out) {
      ASSERT_EQ(x, sentinel);  // fused form writes sel.len == 0 elements, not n
    }
  });
}

TEST(RegEmptySelvec, ReductionsYieldIdentities) {
  const std::int64_t n = 64;
  std::vector<std::int32_t> v(static_cast<std::size_t>(n), -5);
  const quiver::BatchView<std::int32_t> in{v.data(), n};
  const quiver::BitmapView val{nullptr};
  for_each_backend([&] {
    EXPECT_EQ(quiver::reduce_min(in, val, kEmptySel), std::numeric_limits<std::int32_t>::max());
    EXPECT_EQ(quiver::reduce_max(in, val, kEmptySel),
              std::numeric_limits<std::int32_t>::lowest());
    EXPECT_EQ(quiver::reduce_sum_wrap(in, val, kEmptySel), 0);
    std::int64_t sum = -1;
    EXPECT_FALSE(quiver::reduce_sum_checked(in, val, kEmptySel, &sum));
    EXPECT_EQ(sum, 0);
    const quiver::Sma<std::int32_t> sma = quiver::compute_sma(in, val, kEmptySel);
    EXPECT_EQ(sma.min, std::numeric_limits<std::int32_t>::max());
    EXPECT_EQ(sma.max, std::numeric_limits<std::int32_t>::lowest());
    EXPECT_EQ(sma.null_count, 0);
    EXPECT_EQ(quiver::reduce_count_valid(val, n, kEmptySel), 0);
  });
}

TEST(RegEmptySelvec, FilterAndConvertHandleEmpty) {
  const std::int64_t n = 64;
  std::vector<std::uint64_t> v(static_cast<std::size_t>(n), 9);
  std::vector<std::uint64_t> out(1, 0xDEADull);
  for_each_backend([&] {
    EXPECT_EQ(quiver::filter(quiver::BatchView<std::uint64_t>{v.data(), n}, kEmptySel,
                             out.data()),
              0);
    EXPECT_EQ(out[0], 0xDEADull);
    std::vector<std::uint8_t> bits(static_cast<std::size_t>((n + 7) / 8), 0xFF);
    quiver::selvec_to_bitmap(kEmptySel, n, bits.data());
    for (const std::uint8_t b : bits) {
      EXPECT_EQ(b, 0u);  // exactly the empty set, tail zeroed
    }
  });
}

}  // namespace
