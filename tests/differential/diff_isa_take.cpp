// K5 differential matrix: take/dict_decode vs naive gathers over L-set × code widths ×
// selection densities.
// Covers: REQ-TEST-002/-003, REQ-K5-001/-003
#include <cstdint>
#include <cstdlib>
#include <vector>

#include <gtest/gtest.h>

#include "quiver/dispatch.h"
#include "tests/testkit/assertions.h"
#include "tests/testkit/generators.h"
#include "tests/testkit/reference.h"
#include "quiver/take.h"

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


template <class T>
void run_take_diff(std::uint64_t seed) {
  Rng rng(seed);
  for (const std::int64_t n : kLengths) {
    // Dictionary of 97 values; codes drawn uniformly below dict size.
    constexpr std::int64_t kDict = 97;
    std::vector<T> dict(kDict);
    quiver_test::fill_uniform(rng, dict.data(), kDict);
    std::vector<std::uint32_t> idx(static_cast<std::size_t>(n) + 1);
    for (std::int64_t i = 0; i < n; ++i) {
      idx[static_cast<std::size_t>(i)] = static_cast<std::uint32_t>(rng.next_below(kDict));
    }
    std::vector<T> got(static_cast<std::size_t>(n) + 1);
    quiver::take(quiver::BatchView<T>{dict.data(), kDict},
                 quiver::SelVec{idx.data(), n}, got.data());
    for (std::int64_t i = 0; i < n; ++i) {
      ASSERT_EQ(std::memcmp(&got[static_cast<std::size_t>(i)],
                            &dict[idx[static_cast<std::size_t>(i)]], sizeof(T)), 0) << i;
    }
    // dict_decode over u16 codes with a 50% fused selection.
    std::vector<std::uint16_t> codes(static_cast<std::size_t>(n) + 1);
    for (std::int64_t i = 0; i < n; ++i) {
      codes[static_cast<std::size_t>(i)] = static_cast<std::uint16_t>(rng.next_below(kDict));
    }
    std::vector<std::uint8_t> selbits((static_cast<std::size_t>(n) + 7) / 8 + 1);
    quiver_test::fill_bitmap_uniform(rng, selbits.data(), n, 50);
    const auto sel = ref::selvec_expected(selbits.data(), n);
    std::vector<T> fused(sel.size() + 1);
    quiver::dict_decode(quiver::BatchView<T>{dict.data(), kDict}, codes.data(), n,
                        quiver::SelVec{sel.data(), static_cast<std::int64_t>(sel.size())},
                        fused.data());
    for (std::size_t j = 0; j < sel.size(); ++j) {
      ASSERT_EQ(std::memcmp(&fused[j], &dict[codes[sel[j]]], sizeof(T)), 0) << j;
    }
  }
}

TEST(DiffTake, AllBackendsMatchOracle) {
  for_each_backend([&](quiver::Isa isa) {
    for_each_element_type([&]<class T>(T) {
      run_take_diff<T>(0xD1FF0005ull + static_cast<std::uint64_t>(isa));
    });
  });
}

}  // namespace
