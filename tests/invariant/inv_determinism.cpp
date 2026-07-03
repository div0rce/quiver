// Invariant: identical inputs produce bit-identical outputs — run twice, memcmp everything
// (REQ-API-006; per-ISA sections activate as backends land, REQ-DISP-001 prerequisite).
// Covers: REQ-API-006, REQ-TEST-005
#include <cstdint>
#include <cstring>
#include <vector>

#include <gtest/gtest.h>

#include "quiver/quiver.h"
#include "tests/testkit/generators.h"

namespace {

TEST(InvDeterminism, TwoRunsAreByteIdentical) {
  for (const quiver::Isa isa : {quiver::Isa::kScalar, quiver::Isa::kNeon, quiver::Isa::kAvx2,
                                quiver::Isa::kAvx512}) {
    if (isa != quiver::Isa::kScalar && !quiver::cpu_supports(isa)) {
      continue;
    }
    ASSERT_TRUE(quiver::set_isa_override(isa));
    quiver::warmup();
    quiver_test::Rng rng(0xDE7E12);
    constexpr std::int64_t n = 4097;
    std::vector<double> v(n);
    quiver_test::fill_uniform(rng, v.data(), n);
    std::vector<std::uint8_t> validity((n + 7) / 8);
    quiver_test::fill_bitmap_uniform(rng, validity.data(), n, 90);

    auto run = [&](std::vector<std::uint8_t>& bits, std::vector<std::uint32_t>& idx,
                   double& sum) {
      quiver::compare_bitmap(quiver::CompareOp::kGt, quiver::BatchView<double>{v.data(), n},
                             5.0e5, quiver::BitmapView{validity.data()}, bits.data());
      quiver::bitmap_to_selvec(quiver::BitmapView{bits.data()}, n, idx.data());
      sum = quiver::reduce_sum_wrap(quiver::BatchView<double>{v.data(), n},
                                    quiver::BitmapView{validity.data()});
    };
    std::vector<std::uint8_t> bits1((n + 7) / 8);
    std::vector<std::uint8_t> bits2((n + 7) / 8);
    std::vector<std::uint32_t> idx1(n);
    std::vector<std::uint32_t> idx2(n);
    double s1 = 0;
    double s2 = 0;
    run(bits1, idx1, s1);
    run(bits2, idx2, s2);
    EXPECT_EQ(std::memcmp(bits1.data(), bits2.data(), bits1.size()), 0);
    EXPECT_EQ(std::memcmp(&s1, &s2, sizeof(double)), 0);
  }
  quiver::clear_isa_override();
}

}  // namespace
