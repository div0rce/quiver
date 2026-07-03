// K5 properties (PRD 12 §5): take(iota) ≡ copy; take(reverse) ≡ reverse;
// dict_decode ≡ take(dict, codes-as-indices); fused ≡ decode ∘ filter of codes.
// Covers: REQ-TEST-008, REQ-K5-001/-003
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <gtest/gtest.h>

#include "quiver/filter.h"
#include "quiver/take.h"
#include "tests/testkit/generators.h"
#include "tests/testkit/reference.h"

namespace {

using quiver_test::Rng;
namespace ref = quiver_test::ref;

TEST(PropTake, GatherIdentities) {
  Rng rng(0x9E0905);
  for (int iter = 0; iter < (std::getenv("QUIVER_NIGHTLY") ? 200 : 40); ++iter) {
    const std::int64_t n = 1 + static_cast<std::int64_t>(rng.next_below(1000));
    std::vector<double> v(static_cast<std::size_t>(n));
    quiver_test::fill_uniform(rng, v.data(), n);
    std::vector<std::uint32_t> iota(static_cast<std::size_t>(n));
    std::vector<std::uint32_t> rev(static_cast<std::size_t>(n));
    for (std::int64_t i = 0; i < n; ++i) {
      iota[static_cast<std::size_t>(i)] = static_cast<std::uint32_t>(i);
      rev[static_cast<std::size_t>(i)] = static_cast<std::uint32_t>(n - 1 - i);
    }
    std::vector<double> out(static_cast<std::size_t>(n));
    quiver::take(quiver::BatchView<double>{v.data(), n}, quiver::SelVec{iota.data(), n},
                 out.data());
    ASSERT_EQ(std::memcmp(out.data(), v.data(), static_cast<std::size_t>(n) * 8), 0);
    quiver::take(quiver::BatchView<double>{v.data(), n}, quiver::SelVec{rev.data(), n}, out.data());
    for (std::int64_t i = 0; i < n; ++i) {
      ASSERT_EQ(std::memcmp(&out[static_cast<std::size_t>(i)],
                            &v[static_cast<std::size_t>(n - 1 - i)], 8),
                0);
    }

    // dict_decode ≡ take with codes as indices.
    constexpr std::int64_t kDict = 31;
    std::vector<double> dict(kDict);
    quiver_test::fill_uniform(rng, dict.data(), kDict);
    std::vector<std::uint32_t> codes32(static_cast<std::size_t>(n));
    std::vector<std::uint8_t> codes8(static_cast<std::size_t>(n));
    for (std::int64_t i = 0; i < n; ++i) {
      codes32[static_cast<std::size_t>(i)] = static_cast<std::uint32_t>(rng.next_below(kDict));
      codes8[static_cast<std::size_t>(i)] =
          static_cast<std::uint8_t>(codes32[static_cast<std::size_t>(i)]);
    }
    std::vector<double> dec(static_cast<std::size_t>(n));
    std::vector<double> tak(static_cast<std::size_t>(n));
    quiver::dict_decode(quiver::BatchView<double>{dict.data(), kDict}, codes8.data(), n,
                        dec.data());
    quiver::take(quiver::BatchView<double>{dict.data(), kDict}, quiver::SelVec{codes32.data(), n},
                 tak.data());
    ASSERT_EQ(std::memcmp(dec.data(), tak.data(), static_cast<std::size_t>(n) * 8), 0);

    // Fused decode ≡ (filter codes) ∘ decode.
    std::vector<std::uint8_t> selbits((static_cast<std::size_t>(n) + 7) / 8);
    quiver_test::fill_bitmap_uniform(rng, selbits.data(), n, 50);
    const auto selv = ref::selvec_expected(selbits.data(), n);
    const quiver::SelVec sel{selv.data(), static_cast<std::int64_t>(selv.size())};
    std::vector<double> fused(selv.size() + 1);
    quiver::dict_decode(quiver::BatchView<double>{dict.data(), kDict}, codes8.data(), n, sel,
                        fused.data());
    // Bitmap-driven compaction requires an n-element CAPACITY region even though only the
    // first count entries are defined output (REQ-MEM-008).
    std::vector<std::uint8_t> fcodes(static_cast<std::size_t>(n) + 1);
    quiver::filter(quiver::BatchView<std::uint8_t>{codes8.data(), n},
                   quiver::BitmapView{selbits.data()}, fcodes.data());
    std::vector<double> composed(selv.size() + 1);
    quiver::dict_decode(quiver::BatchView<double>{dict.data(), kDict}, fcodes.data(),
                        static_cast<std::int64_t>(selv.size()), composed.data());
    ASSERT_EQ(std::memcmp(fused.data(), composed.data(), selv.size() * 8), 0);
  }
}

}  // namespace
