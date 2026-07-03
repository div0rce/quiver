// MOD-TESTKIT self-tests: cross-platform determinism (golden FNV-1a hashes asserted
// identical on x86-64, ARM64, and macOS CI — M2 acceptance), distribution sanity
// (empirical selectivity within ±0.5% at n=1e6), and diagnostic-format checks.
// Covers: REQ-INT-002, REQ-TEST-012, REQ-ERR-008 (PRD 05 §7, PRD 18 M2)
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "tests/testkit/assertions.h"
#include "tests/testkit/generators.h"
#include "tests/testkit/reference.h"

namespace {

using quiver_test::AlignedBuffer;
using quiver_test::fnv1a64;
using quiver_test::Rng;

constexpr std::uint64_t kSeed = 0x51E5EEDBA5E0001ull;  // fixed suite seed (REQ-TEST-012)

// Golden hashes computed once and committed; any platform producing different bytes for the
// same seed fails here. Values are validated to be identical across all tier-1 CI platforms.
// If a deliberate spec change alters generator output, update these values AND the bench-local
// implementation in the same PR (drift_check enforces the pairing).
std::uint64_t hash_uniform_i32(std::uint64_t seed, std::int64_t n) {
  Rng rng(seed);
  std::vector<std::int32_t> v(static_cast<std::size_t>(n));
  quiver_test::fill_uniform(rng, v.data(), n);
  return fnv1a64(v.data(), v.size() * sizeof(std::int32_t));
}

std::uint64_t hash_uniform_f64(std::uint64_t seed, std::int64_t n) {
  Rng rng(seed);
  std::vector<double> v(static_cast<std::size_t>(n));
  quiver_test::fill_uniform(rng, v.data(), n);
  return fnv1a64(v.data(), v.size() * sizeof(double));
}

std::uint64_t hash_zipf(std::uint64_t seed, std::int64_t n) {
  Rng rng(seed);
  std::vector<std::uint32_t> v(static_cast<std::size_t>(n));
  quiver_test::fill_zipf_codes(rng, v.data(), n);
  return fnv1a64(v.data(), v.size() * sizeof(std::uint32_t));
}

std::uint64_t hash_bitmap(std::uint64_t seed, std::int64_t n, int pct, bool clustered) {
  Rng rng(seed);
  std::vector<std::uint8_t> bits(static_cast<std::size_t>((n + 7) / 8));
  if (clustered) {
    quiver_test::fill_bitmap_clustered(rng, bits.data(), n, pct);
  } else {
    quiver_test::fill_bitmap_uniform(rng, bits.data(), n, pct);
  }
  return fnv1a64(bits.data(), bits.size());
}

// GOLDEN VALUES — regenerate with tests/testkit/drift_check.cpp `--print-golden` (see
// docs/testing/testkit.md) only on a deliberate, documented spec change.
TEST(Testkit, GoldenHashesAreCrossPlatformStable) {
  EXPECT_EQ(hash_uniform_i32(kSeed, 4096), UINT64_C(0xba831572317c5bc3));
  EXPECT_EQ(hash_uniform_f64(kSeed, 4096), UINT64_C(0x5fdf4db31403880f));
  EXPECT_EQ(hash_zipf(kSeed, 4096), UINT64_C(0x2000b3d2dc06be5a));
  EXPECT_EQ(hash_bitmap(kSeed, 4096, 10, false), UINT64_C(0x83475152c478ee1d));
  EXPECT_EQ(hash_bitmap(kSeed, 4096, 50, true), UINT64_C(0x1de127a44a2dadb2));
}

TEST(Testkit, DeterminismAcrossInstances) {
  EXPECT_EQ(hash_uniform_i32(123, 1000), hash_uniform_i32(123, 1000));
  EXPECT_NE(hash_uniform_i32(123, 1000), hash_uniform_i32(124, 1000));
}

TEST(Testkit, UniformBitmapSelectivityWithinHalfPercent) {
  constexpr std::int64_t n = 1'000'000;
  for (const int pct : {1, 10, 50, 90, 99}) {
    Rng rng(kSeed + static_cast<std::uint64_t>(pct));
    std::vector<std::uint8_t> bits(static_cast<std::size_t>((n + 7) / 8));
    quiver_test::fill_bitmap_uniform(rng, bits.data(), n, pct);
    const double got = 100.0 *
                       static_cast<double>(quiver_test::ref::popcount_bits(bits.data(), n)) /
                       static_cast<double>(n);
    EXPECT_NEAR(got, pct, 0.5) << "selectivity " << pct << "% (REQ-INT-002 sanity)";
    EXPECT_TRUE(quiver_test::ref::tail_bits_zero(bits.data(), n));
  }
}

TEST(Testkit, ClusteredBitmapDensityTracksSelectivity) {
  constexpr std::int64_t n = 1'000'000;
  Rng rng(kSeed);
  std::vector<std::uint8_t> bits(static_cast<std::size_t>((n + 7) / 8));
  quiver_test::fill_bitmap_clustered(rng, bits.data(), n, 50);
  const double got = 100.0 * static_cast<double>(quiver_test::ref::popcount_bits(bits.data(), n)) /
                     static_cast<double>(n);
  EXPECT_NEAR(got, 50.0, 3.0);  // run-granular: wider tolerance than per-bit uniform
}

TEST(Testkit, ZipfIsSkewedTowardLowCodes) {
  constexpr std::int64_t n = 100'000;
  Rng rng(kSeed);
  std::vector<std::uint32_t> codes(static_cast<std::size_t>(n));
  quiver_test::fill_zipf_codes(rng, codes.data(), n);
  std::int64_t code0 = 0;
  for (const std::uint32_t c : codes) {
    ASSERT_LT(c, 1000u);
    code0 += (c == 0) ? 1 : 0;
  }
  // P(code 0) = (1/1)/H_1000 ≈ 1/7.485 ≈ 13.4%.
  EXPECT_NEAR(static_cast<double>(code0) / static_cast<double>(n), 0.134, 0.02);
}

TEST(Testkit, SelvecFromBitmapIsStrictlyIncreasing) {
  constexpr std::int64_t n = 4096;
  Rng rng(kSeed);
  std::vector<std::uint8_t> bits(static_cast<std::size_t>((n + 7) / 8));
  quiver_test::fill_bitmap_uniform(rng, bits.data(), n, 30);
  std::vector<std::uint32_t> idx(static_cast<std::size_t>(n));
  const std::int64_t count = quiver_test::selvec_from_bitmap(bits.data(), n, idx.data());
  EXPECT_EQ(count, quiver_test::ref::popcount_bits(bits.data(), n));
  for (std::int64_t i = 1; i < count; ++i) {
    ASSERT_LT(idx[static_cast<std::size_t>(i - 1)], idx[static_cast<std::size_t>(i)]);
  }
}

TEST(Testkit, AlignedBufferHonorsOffsets) {
  AlignedBuffer<std::int32_t> aligned(128, 0);
  AlignedBuffer<std::int32_t> offset(128, 1);
  EXPECT_EQ(reinterpret_cast<std::uintptr_t>(aligned.data()) % 64, 0u);
  EXPECT_EQ(reinterpret_cast<std::uintptr_t>(offset.data()) % 64, sizeof(std::int32_t));
}

TEST(Testkit, DivergenceDiagnosticNamesIndexSeedAndReq) {
  const std::int32_t a[8] = {0, 1, 2, 3, 4, 5, 6, 7};
  const std::int32_t b[8] = {0, 1, 2, 99, 4, 5, 6, 7};
  const auto result = quiver_test::buffers_equal(a, b, 8, kSeed, "REQ-TEST-012");
  ASSERT_FALSE(result);
  const std::string msg = result.message();
  EXPECT_NE(msg.find("index 3"), std::string::npos);
  EXPECT_NE(msg.find("REQ-TEST-012"), std::string::npos);
  EXPECT_NE(msg.find("seed=0x"), std::string::npos);
}

}  // namespace
