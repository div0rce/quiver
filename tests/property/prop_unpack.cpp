// K8 properties (PRD 12 §5): pack∘unpack round trip over every width (the pack side is the
// testkit's independent bit-setter), and unpack_for ≡ unpack + elementwise wrapping add.
// Covers: REQ-K8-001..004, REQ-TEST-008
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <gtest/gtest.h>

#include "quiver/arith.h"
#include "quiver/unpack.h"
#include "tests/testkit/generators.h"
#include "tests/testkit/reference.h"

namespace {

using quiver_test::Rng;
namespace ref = quiver_test::ref;

template <class Out>
void roundtrip_all_widths(std::uint64_t seed) {
  Rng rng(seed);
  const int max_w = 8 * static_cast<int>(sizeof(Out));
  for (int w = 0; w <= max_w; ++w) {  // exhaustive width sweep (REQ-K8-004)
    const std::int64_t n = 200 + static_cast<std::int64_t>(rng.next_below(56));
    std::vector<std::uint64_t> values(static_cast<std::size_t>(n));
    std::vector<std::uint8_t> packed((static_cast<std::size_t>(n) * w + 7) / 8 + 1, 0);
    const std::uint64_t mask = (w == 64) ? ~0ull : ((1ull << w) - 1ull);
    for (std::int64_t i = 0; i < n; ++i) {
      values[static_cast<std::size_t>(i)] = rng.next() & mask;
      if (w > 0) {
        ref::pack_value(packed.data(), i, w, values[static_cast<std::size_t>(i)]);
      }
    }
    std::vector<Out> out(static_cast<std::size_t>(n));
    quiver::unpack(w == 0 ? nullptr : packed.data(), n, w, out.data());
    for (std::int64_t i = 0; i < n; ++i) {
      ASSERT_EQ(out[static_cast<std::size_t>(i)],
                static_cast<Out>(w == 0 ? 0 : values[static_cast<std::size_t>(i)]))
          << "Out=" << sizeof(Out) << " w=" << w << " i=" << i;
    }
  }
}

TEST(PropUnpack, RoundTripAllWidthsU8) {
  roundtrip_all_widths<std::uint8_t>(0x08A11ull);
}
TEST(PropUnpack, RoundTripAllWidthsU16) {
  roundtrip_all_widths<std::uint16_t>(0x08A12ull);
}
TEST(PropUnpack, RoundTripAllWidthsU32) {
  roundtrip_all_widths<std::uint32_t>(0x08A13ull);
}
TEST(PropUnpack, RoundTripAllWidthsU64) {
  roundtrip_all_widths<std::uint64_t>(0x08A14ull);
}

TEST(PropUnpack, ForFusionEqualsUnpackPlusAdd) {
  Rng rng(0x08A15ull);
  for (const int w : {1, 5, 8, 13, 16, 27, 32}) {
    const std::int64_t n = 300;
    std::vector<std::uint8_t> packed((static_cast<std::size_t>(n) * w + 7) / 8 + 1, 0);
    for (std::int64_t i = 0; i < n; ++i) {
      ref::pack_value(packed.data(), i, w, rng.next() & ((1ull << w) - 1ull));
    }
    const auto base = static_cast<std::uint32_t>(rng.next());
    std::vector<std::uint32_t> fused(static_cast<std::size_t>(n));
    quiver::unpack_for(packed.data(), n, w, base, fused.data());
    std::vector<std::uint32_t> plain(static_cast<std::size_t>(n));
    quiver::unpack(packed.data(), n, w, plain.data());
    std::vector<std::uint32_t> composed(static_cast<std::size_t>(n));
    quiver::arith(quiver::ArithOp::kAdd, quiver::BatchView<std::uint32_t>{plain.data(), n}, base,
                  composed.data());
    ASSERT_EQ(std::memcmp(fused.data(), composed.data(), fused.size() * 4), 0) << "w=" << w;
  }
}

}  // namespace
