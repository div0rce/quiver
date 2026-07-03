// K8 unit tests: hand-computed ADR-026 layout cases, the w = 0 contract, full-width
// pass-through, FOR fusion wrapping, and the exact-read-bound on a guarded page.
// Covers: REQ-K8-001..003, REQ-SEC-004 (bound check), REQ-TEST-014-adjacent contracts
#include <cstdint>
#include <cstring>
#include <vector>

#include <gtest/gtest.h>

#include "quiver/unpack.h"
#include "tests/testkit/generators.h"
#include "tests/testkit/reference.h"

namespace {

namespace ref = quiver_test::ref;

TEST(Unpack, HandComputedLayout) {
  // ADR-026: value i occupies bits [i*w, (i+1)*w), LSB-first. For w = 3 and the byte
  // stream {0b11'010'001, 0b0000'10'11}: v0 = 001b = 1, v1 = 010b = 2, v2 = 11|1b -> bits
  // 6,7 of byte0 (11) + bit 0 of byte1 (1) = 111b = 7, v3 = bits 9..11 = 101b = 5.
  const std::uint8_t packed[2] = {0b11010001, 0b00001011};
  std::uint32_t out[4] = {};
  quiver::unpack(packed, 4, 3, out);
  EXPECT_EQ(out[0], 1u);
  EXPECT_EQ(out[1], 2u);
  EXPECT_EQ(out[2], 7u);
  EXPECT_EQ(out[3], 5u);
}

TEST(Unpack, WidthZeroMeansBaseAndNullPacked) {
  std::uint16_t out[5] = {9, 9, 9, 9, 9};
  quiver::unpack<std::uint16_t>(nullptr, 5, 0, out);  // REQ-K8-003
  for (auto v : out) {
    EXPECT_EQ(v, 0u);
  }
  quiver::unpack_for<std::uint16_t>(nullptr, 5, 0, std::uint16_t{77}, out);
  for (auto v : out) {
    EXPECT_EQ(v, 77u);
  }
}

TEST(Unpack, FullWidthIsCopyPlusBase) {
  std::uint64_t values[3] = {~0ull, 1ull, 0x0123456789ABCDEFull};
  std::uint8_t packed[24];
  std::memcpy(packed, values, 24);
  std::uint64_t out[3];
  quiver::unpack_for(packed, 3, 64, std::uint64_t{1}, out);
  EXPECT_EQ(out[0], 0ull);  // wrapping FOR add
  EXPECT_EQ(out[1], 2ull);
  EXPECT_EQ(out[2], 0x0123456789ABCDF0ull);
}

TEST(Unpack, ForFusionWraps) {
  // w = 4, base near the top of u8: wrap is the documented semantics.
  std::uint8_t packed[1] = {0xF1};  // v0 = 1, v1 = 15
  std::uint8_t out[2];
  quiver::unpack_for(packed, 2, 4, std::uint8_t{250}, out);
  EXPECT_EQ(out[0], static_cast<std::uint8_t>(251));
  EXPECT_EQ(out[1], static_cast<std::uint8_t>(9));  // 250 + 15 wraps
}

TEST(Unpack, OddWidthsMatchNaiveOracle) {
  quiver_test::Rng rng(0x0801u);
  for (const int w : {1, 3, 5, 7, 11, 13, 17, 23, 29, 31}) {
    const std::int64_t n = 100;
    std::vector<std::uint8_t> packed((static_cast<std::size_t>(n) * w + 7) / 8, 0);
    for (std::int64_t i = 0; i < n; ++i) {
      ref::pack_value(packed.data(), i, w, rng.next() & ((1ull << w) - 1));
    }
    std::vector<std::uint32_t> out(static_cast<std::size_t>(n));
    quiver::unpack(packed.data(), n, w, out.data());
    for (std::int64_t i = 0; i < n; ++i) {
      ASSERT_EQ(out[i], ref::unpack_value_expected<std::uint32_t>(packed.data(), i, w))
          << "w=" << w << " i=" << i;
    }
  }
}

// REQ-K8-002 / REQ-SEC-004: reads exactly ceil(n*w/8) bytes — verified with a guard page
// immediately after the packed stream (any over-read faults the process).
TEST(Unpack, ExactReadBoundOnGuardPage) {
  for (const int w : {1, 3, 7, 8, 12, 16, 31, 32, 33, 64}) {
    for (const std::int64_t n : {1, 7, 8, 9, 63, 64, 65, 1000}) {
      if (w > 32 && n > 65) {
        continue;  // keep the page budget small; coverage is the boundary itself
      }
      const std::int64_t bytes = (n * w + 7) / 8;
      quiver_test::GuardedBuffer<std::uint8_t> packed(bytes, quiver_test::Guard::kEnd);
      for (std::int64_t b = 0; b < bytes; ++b) {
        packed.data()[b] = static_cast<std::uint8_t>(b * 131 + 7);
      }
      if (w <= 32) {
        std::vector<std::uint32_t> out32(static_cast<std::size_t>(n));
        quiver::unpack(packed.data(), n, w, out32.data());  // fault == over-read
      } else {
        std::vector<std::uint64_t> out(static_cast<std::size_t>(n));
        quiver::unpack(packed.data(), n, w, out.data());
      }
    }
  }
}

}  // namespace
