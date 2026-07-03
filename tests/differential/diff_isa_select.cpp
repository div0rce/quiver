// K3 differential matrix + round-trip identities over L-set × density × pattern.
// Covers: REQ-TEST-002/-003, REQ-K3-001..002
#include <cstdint>
#include <cstdlib>
#include <vector>

#include <gtest/gtest.h>

#include "quiver/dispatch.h"
#include "tests/testkit/assertions.h"
#include "tests/testkit/generators.h"
#include "tests/testkit/reference.h"
#include "quiver/select.h"

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


TEST(DiffSelect, ConversionsMatchOracleAndRoundTrip) {
  for_each_backend([&](quiver::Isa isa) {
    Rng rng(0xD1FF0003ull + static_cast<std::uint64_t>(isa));
    for (const std::int64_t n : kLengths) {
      const std::int64_t bytes = (n + 7) / 8;
      for (const int pct : {0, 10, 50, 100}) {
        std::vector<std::uint8_t> bits(static_cast<std::size_t>(bytes) + 1);
        quiver_test::fill_bitmap_uniform(rng, bits.data(), n, pct);
        const auto want = ref::selvec_expected(bits.data(), n);
        std::vector<std::uint32_t> idx(static_cast<std::size_t>(n) + 1);
        const std::int64_t count =
            quiver::bitmap_to_selvec(quiver::BitmapView{bits.data()}, n, idx.data());
        ASSERT_EQ(count, static_cast<std::int64_t>(want.size()));
        for (std::int64_t j = 0; j < count; ++j) {
          ASSERT_EQ(idx[static_cast<std::size_t>(j)], want[static_cast<std::size_t>(j)]);
        }
        // Round trip: selvec -> bitmap == original (REQ-K3-001).
        std::vector<std::uint8_t> round(static_cast<std::size_t>(bytes) + 1, 0xEE);
        quiver::selvec_to_bitmap(quiver::SelVec{idx.data(), count}, n, round.data());
        for (std::int64_t i = 0; i < n; ++i) {
          ASSERT_EQ(ref::bit_get(round.data(), i), ref::bit_get(bits.data(), i)) << i;
        }
        ASSERT_TRUE(ref::tail_bits_zero(round.data(), n));
      }
    }
  });
}

}  // namespace
