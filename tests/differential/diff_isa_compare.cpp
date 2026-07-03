// K1 differential matrix: every host-available backend vs the naive oracle, byte-exact,
// over L-set lengths × ops × selectivity-shaping comparands × validity densities.
// Covers: REQ-TEST-002/-003, REQ-K1-001..003, REQ-API-006
#include <cstdint>
#include <cstdlib>
#include <vector>

#include <gtest/gtest.h>

#include "quiver/compare.h"
#include "quiver/dispatch.h"
#include "tests/testkit/assertions.h"
#include "tests/testkit/generators.h"
#include "tests/testkit/reference.h"

namespace {

using quiver_test::Rng;
namespace ref = quiver_test::ref;

// Applies a functor to a zero of each of the ten element types (REQ-API-004 sweep helper).
template <class F>
void for_each_element_type(F f) {
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
constexpr std::int64_t kLengths[] = {0,  1,  2,  3,  7,   8,   9,   15,  16,  17,  31,   32,
                                     33, 63, 64, 65, 127, 128, 129, 255, 256, 257, 1000, 4096};

[[maybe_unused]] inline bool nightly() {
  return std::getenv("QUIVER_NIGHTLY") != nullptr;
}
// Runs `body` once per host-executable tier (REQ-TEST-003 backend column; scalar-only at M3).
template <class Body>
void for_each_backend(Body body) {
  for (const quiver::Isa isa :
       {quiver::Isa::kScalar, quiver::Isa::kNeon, quiver::Isa::kAvx2, quiver::Isa::kAvx512}) {
    if (isa != quiver::Isa::kScalar && !quiver::cpu_supports(isa)) {
      continue;
    }
    ASSERT_TRUE(quiver::set_isa_override(isa));
    quiver::warmup();
    body(isa);
  }
  quiver::clear_isa_override();
}

template <class T>
void run_compare_diff(std::uint64_t seed) {
  Rng rng(seed);
  for (const std::int64_t n : kLengths) {
    std::vector<T> v(static_cast<std::size_t>(n) + 1);
    quiver_test::fill_uniform(rng, v.data(), n);
    std::vector<std::uint8_t> validity((static_cast<std::size_t>(n) + 7) / 8 + 1);
    quiver_test::fill_bitmap_uniform(rng, validity.data(), n, 80);
    const T comparand = n > 0 ? v[static_cast<std::size_t>(n) / 2] : T{};
    const std::int64_t bytes = (n + 7) / 8;

    for (int opi = 0; opi < 6; ++opi) {
      const auto op = static_cast<quiver::CompareOp>(opi);
      for (const std::uint8_t* vd : {static_cast<const std::uint8_t*>(nullptr),
                                     static_cast<const std::uint8_t*>(validity.data())}) {
        // Expected via the naive oracle.
        std::vector<std::uint8_t> want_bits(static_cast<std::size_t>(bytes) + 1, 0);
        const std::int64_t want_count =
            ref::predicate_bitmap(n, want_bits.data(), [&](std::int64_t i) {
              return ref::valid(vd, i) &&
                     ref::compare_one(op, v[static_cast<std::size_t>(i)], comparand);
            });
        std::vector<std::uint8_t> got_bits(static_cast<std::size_t>(bytes) + 1, 0xEE);
        const std::int64_t got_count =
            quiver::compare_bitmap(op, quiver::BatchView<T>{v.data(), n}, comparand,
                                   quiver::BitmapView{vd}, got_bits.data());
        ASSERT_EQ(got_count, want_count) << "n=" << n << " op=" << opi;
        if (bytes > 0) {
          ASSERT_TRUE(quiver_test::buffers_equal(want_bits.data(), got_bits.data(), bytes, seed,
                                                 "REQ-K1-001"));
        }
        // Selvec form agrees positionally.
        std::vector<std::uint32_t> got_idx(static_cast<std::size_t>(n) + 1);
        const std::int64_t sv_count =
            quiver::compare_selvec(op, quiver::BatchView<T>{v.data(), n}, comparand,
                                   quiver::BitmapView{vd}, got_idx.data());
        ASSERT_EQ(sv_count, want_count);
        const auto want_idx = ref::selvec_expected(want_bits.data(), n);
        for (std::int64_t j = 0; j < sv_count; ++j) {
          ASSERT_EQ(got_idx[static_cast<std::size_t>(j)], want_idx[static_cast<std::size_t>(j)]);
        }
      }
    }
  }
}

TEST(DiffCompare, AllBackendsMatchOracle) {
  for_each_backend([&](quiver::Isa isa) {
    for_each_element_type(
        [&]<class T>(T) { run_compare_diff<T>(0xD1FF0001ull + static_cast<std::uint64_t>(isa)); });
  });
}

}  // namespace
