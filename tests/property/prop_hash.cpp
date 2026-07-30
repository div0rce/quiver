// K7 properties (PRD 12 §5): seed sensitivity, determinism, and the REQ-TEST-016
// avalanche/bias quality gate — over >= 100k seeded samples per type under QUIVER_NIGHTLY
// (PR tier runs a reduced sample count as a smoke of the same machinery), every input-bit
// flip must change each output bit with probability 0.5 +- 0.02, and no output bit may show
// overall bias > 0.02. The constants are ADR-012-FROZEN: a gate failure means an
// implementation bug, never tuning latitude (PRD 18 M6 risk note).
// Covers: REQ-K7-002..003, REQ-TEST-016
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "quiver/hash.h"
#include "tests/testkit/generators.h"

namespace {

using quiver_test::Rng;

inline bool nightly() {
  return std::getenv("QUIVER_NIGHTLY") != nullptr;
}

TEST(PropHash, SeedChangesEverything) {
  Rng rng(0x707A5401ull);
  const std::int64_t n = 512;
  std::vector<std::uint64_t> v(static_cast<std::size_t>(n));
  for (auto& x : v) {
    x = rng.next();
  }
  std::vector<std::uint64_t> h1(v.size());
  std::vector<std::uint64_t> h2(v.size());
  quiver::hash64(quiver::BatchView<std::uint64_t>{v.data(), n}, 1, h1.data());
  quiver::hash64(quiver::BatchView<std::uint64_t>{v.data(), n}, 2, h2.data());
  std::int64_t same = 0;
  for (std::size_t i = 0; i < v.size(); ++i) {
    same += h1[i] == h2[i] ? 1 : 0;
  }
  EXPECT_EQ(same, 0);  // 512 collisions across seeds would be astronomically unlikely
}

TEST(PropHash, BatchEqualsElementwise) {
  Rng rng(0x707A5402ull);
  const std::int64_t n = 1000;
  std::vector<std::int32_t> v(static_cast<std::size_t>(n));
  quiver_test::fill_uniform(rng, v.data(), n);
  std::vector<std::uint64_t> batch(v.size());
  quiver::hash64(quiver::BatchView<std::int32_t>{v.data(), n}, 99, batch.data());
  for (std::int64_t i = 0; i < n; i += 97) {
    std::uint64_t one = 0;
    quiver::hash64(quiver::BatchView<std::int32_t>{v.data() + i, 1}, 99, &one);
    ASSERT_EQ(one, batch[static_cast<std::size_t>(i)]);
  }
}

// REQ-TEST-016 avalanche: for input bit j, P(output bit k flips) within 0.5 +- 0.02.
// Aggregated per (input-bit, output-bit) pair over the sample count.
// Same-size unsigned carrier (make_unsigned rejects floats).
template <class T>
using Carrier = std::conditional_t<
    sizeof(T) == 1, std::uint8_t,
    std::conditional_t<sizeof(T) == 2, std::uint16_t,
                       std::conditional_t<sizeof(T) == 4, std::uint32_t, std::uint64_t>>>;

// How often each input bit flipped each output bit, and how often each output bit was set.
struct AvalancheCounts {
  std::vector<std::int64_t> flips;
  std::vector<std::int64_t> bits_set;
};

// One sample: hash a random value, then hash every single-bit flip of it.
template <class T>
void tally_sample(AvalancheCounts& c, Rng& rng) {
  constexpr int kInBits = 8 * static_cast<int>(sizeof(T));
  using U = Carrier<T>;
  const U raw = static_cast<U>(rng.next());
  T v;
  std::memcpy(&v, &raw, sizeof(T));
  std::uint64_t h0 = 0;
  quiver::hash64(quiver::BatchView<T>{&v, 1}, 0x1234, &h0);
  for (int out = 0; out < 64; ++out) {
    c.bits_set[static_cast<std::size_t>(out)] += (h0 >> out) & 1u;
  }
  for (int in = 0; in < kInBits; ++in) {
    const U flipped_raw = static_cast<U>(raw ^ (U{1} << in));
    T fv;
    std::memcpy(&fv, &flipped_raw, sizeof(T));
    std::uint64_t h1 = 0;
    quiver::hash64(quiver::BatchView<T>{&fv, 1}, 0x1234, &h1);
    const std::uint64_t diff = h0 ^ h1;
    for (int out = 0; out < 64; ++out) {
      c.flips[static_cast<std::size_t>(in) * 64 + out] += (diff >> out) & 1u;
    }
  }
}

// Statistical resolution is bounded by DISTINCT flip pairs, not raw samples: a T-bit domain has
// at most 2^(T-1) distinct (x, x^bit) pairs, so for 8-bit keys the flip probability is a fixed
// rational with ~1/128 granularity and the literal 0.5 +- 0.02 criterion is unsatisfiable for ANY
// function (gate M6 records the REQ-TEST-016 amendment). Band = max(0.02, 6 sigma of the
// distinct-pair binomial SE); nightly at >= 100k samples enforces exactly the normative 0.02 for
// every type of 16+ bits.
template <class T>
double avalanche_band(std::int64_t samples) {
  constexpr int kInBits = 8 * static_cast<int>(sizeof(T));
  const double distinct_pairs = std::min(static_cast<double>(samples),
                                         (kInBits >= 63) ? 9.22e18 : std::ldexp(1.0, kInBits - 1));
  return std::max(0.02, 6.0 * std::sqrt(0.25 / distinct_pairs));
}

template <class T>
void expect_within_band(const AvalancheCounts& c, std::int64_t samples, double band) {
  constexpr int kInBits = 8 * static_cast<int>(sizeof(T));
  const double lo = 0.5 - band;
  const double hi = 0.5 + band;
  for (int in = 0; in < kInBits; ++in) {
    for (int out = 0; out < 64; ++out) {
      const double p = static_cast<double>(c.flips[static_cast<std::size_t>(in) * 64 + out]) /
                       static_cast<double>(samples);
      ASSERT_GE(p, lo) << "avalanche: in-bit " << in << " -> out-bit " << out;
      ASSERT_LE(p, hi) << "avalanche: in-bit " << in << " -> out-bit " << out;
    }
  }
  for (int out = 0; out < 64; ++out) {
    const double bias = static_cast<double>(c.bits_set[static_cast<std::size_t>(out)]) /
                            static_cast<double>(samples) -
                        0.5;
    ASSERT_LE(bias, band) << "bias: out-bit " << out;
    ASSERT_GE(bias, -band) << "bias: out-bit " << out;
  }
}

template <class T>
void avalanche_gate(std::uint64_t seed_base, std::int64_t samples) {
  constexpr int kInBits = 8 * static_cast<int>(sizeof(T));
  Rng rng(seed_base);
  AvalancheCounts counts{std::vector<std::int64_t>(static_cast<std::size_t>(kInBits) * 64, 0),
                         std::vector<std::int64_t>(64, 0)};
  for (std::int64_t s = 0; s < samples; ++s) {
    tally_sample<T>(counts, rng);
  }
  expect_within_band<T>(counts, samples, avalanche_band<T>(samples));
}

// Integer-key avalanche across representative widths; the full >= 100k/type sweep is
// nightly (REQ-TEST-016); the PR tier proves the machinery on a smaller sample. Note the
// sample floor: 0.5 +- 0.02 needs enough trials for the binomial CI to fit inside the band
// (~10k is comfortably sufficient at 3 sigma; nightly uses 100k).
TEST(PropHash, AvalancheAndBiasGate) {
  const std::int64_t samples = nightly() ? 100000 : 10000;
  avalanche_gate<std::uint64_t>(0xA7A1A001ull, samples);
  avalanche_gate<std::uint32_t>(0xA7A1A002ull, samples);
  if (nightly()) {
    avalanche_gate<std::uint16_t>(0xA7A1A003ull, samples);
    avalanche_gate<std::uint8_t>(0xA7A1A004ull, samples);
    avalanche_gate<std::int64_t>(0xA7A1A005ull, samples);
    avalanche_gate<std::int32_t>(0xA7A1A006ull, samples);
    avalanche_gate<std::int16_t>(0xA7A1A007ull, samples);
    avalanche_gate<std::int8_t>(0xA7A1A008ull, samples);
    avalanche_gate<float>(0xA7A1A009ull, samples);
    avalanche_gate<double>(0xA7A1A00Aull, samples);
  }
}

}  // namespace
