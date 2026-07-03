// K4 differential matrix: word-loop implementation vs the per-bit naive oracle.
// Covers: REQ-TEST-002/-003, REQ-K4-001..002
#include <cstdint>
#include <cstdlib>
#include <vector>

#include <gtest/gtest.h>

#include "quiver/dispatch.h"
#include "tests/testkit/assertions.h"
#include "tests/testkit/generators.h"
#include "tests/testkit/reference.h"
#include "quiver/mask.h"

namespace {

using quiver_test::Rng;
namespace ref = quiver_test::ref;


// Applies a functor to a zero of each of the ten element types (REQ-API-004 sweep helper).
template <class F>
void for_each_element_type_unused(F f) {
  f(std::int8_t{});
  f(std::int16_t{});
  f(std::int32_t{});
  f(std::int64_t{});
  f(std::uint8_t{});
  f(std::uint16_t{});
  f(std::uint32_t{});
  f(std::uint64_t{});
  f(float{});
  f(double{});
}

// PR-tier length set: every tail residue for widths <= 64 B plus representative large sizes
// (REQ-TEST-003 L-set); QUIVER_NIGHTLY widens the sampled axes to the full cross product.
constexpr std::int64_t kLengths[] = {0,  1,  2,  3,   7,   8,   9,   15,  16,   17,   31, 32,
                                     33, 63, 64, 65,  127, 128, 129, 255, 256,  257,  1000, 4096};

inline bool nightly() { return std::getenv("QUIVER_NIGHTLY") != nullptr; }
// Runs `body` once per host-executable tier (REQ-TEST-003 backend column; scalar-only at M3).
template <class Body>
void for_each_backend(Body body) {
  for (const quiver::Isa isa : {quiver::Isa::kScalar, quiver::Isa::kNeon, quiver::Isa::kAvx2,
                                quiver::Isa::kAvx512}) {
    if (isa != quiver::Isa::kScalar && !quiver::cpu_supports(isa)) {
      continue;
    }
    ASSERT_TRUE(quiver::set_isa_override(isa));
    quiver::warmup();
    body(isa);
  }
  quiver::clear_isa_override();
}


TEST(DiffMask, AllOpsMatchOracle) {
  for_each_backend([&](quiver::Isa isa) {
    Rng rng(0xD1FF0004ull + static_cast<std::uint64_t>(isa));
    for (const std::int64_t n : kLengths) {
      const std::int64_t bytes = (n + 7) / 8;
      std::vector<std::uint8_t> a(static_cast<std::size_t>(bytes) + 1);
      std::vector<std::uint8_t> b(static_cast<std::size_t>(bytes) + 1);
      quiver_test::fill_bitmap_uniform(rng, a.data(), n, 37);
      quiver_test::fill_bitmap_uniform(rng, b.data(), n, 73);
      std::vector<std::uint8_t> out(static_cast<std::size_t>(bytes) + 1);
      for (int opi = 0; opi < 4; ++opi) {
        const auto op = static_cast<quiver::MaskOp>(opi);
        quiver::mask_combine(op, quiver::BitmapView{a.data()}, quiver::BitmapView{b.data()}, n,
                             out.data());
        for (std::int64_t i = 0; i < n; ++i) {
          const bool av = ref::bit_get(a.data(), i);
          const bool bv = ref::bit_get(b.data(), i);
          const bool want = opi == 0 ? (av && bv)
                            : opi == 1 ? (av || bv)
                            : opi == 2 ? (av && !bv)
                                       : (av != bv);
          ASSERT_EQ(ref::bit_get(out.data(), i), want) << "op=" << opi << " n=" << n;
        }
        ASSERT_TRUE(ref::tail_bits_zero(out.data(), n));
      }
      ASSERT_EQ(quiver::mask_popcount(quiver::BitmapView{a.data()}, n),
                ref::popcount_bits(a.data(), n));
    }
  });
}

}  // namespace
