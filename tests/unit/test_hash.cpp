// K7 unit tests: the COMMITTED golden vectors (frozen at M6, ADR-012 — this suite failing
// after a code change means a broken hash, never tuning latitude), float canonicalization,
// zero-extension typing rule, and the combine formula.
// Covers: REQ-K7-001..002, REQ-TEST-015 (golden-vector infrastructure)
#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "quiver/hash.h"
#include "tests/testkit/reference.h"

namespace {

struct GoldenRow {
  std::string type;
  std::uint64_t key_bits;
  std::uint64_t seed;
  std::uint64_t hash;
};

std::vector<GoldenRow> load_goldens() {
  // CTest runs suites from the build tree; the golden file resolves via the source-root
  // macro the CMake wiring passes.
  std::ifstream f(QUIVER_GOLDEN_DIR "/qhash64_vectors.txt");
  EXPECT_TRUE(f.is_open());
  std::vector<GoldenRow> rows;
  std::string line;
  while (std::getline(f, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    std::istringstream ss(line);
    GoldenRow r;
    ss >> r.type >> std::hex >> r.key_bits >> r.seed >> r.hash;
    rows.push_back(r);
  }
  return rows;
}

// Reconstruct the typed input from the recorded key bits and hash it via the façade.
template <class T>
std::uint64_t hash_from_bits(std::uint64_t key_bits, std::uint64_t seed) {
  T v;
  if constexpr (sizeof(T) == 4 && std::is_floating_point_v<T>) {
    const auto b = static_cast<std::uint32_t>(key_bits);
    std::memcpy(&v, &b, 4);
  } else if constexpr (sizeof(T) == 8 && std::is_floating_point_v<T>) {
    std::memcpy(&v, &key_bits, 8);
  } else {
    v = static_cast<T>(key_bits);
  }
  std::uint64_t h = 0;
  quiver::hash64(quiver::BatchView<T>{&v, 1}, seed, &h);
  return h;
}

// The golden file's type column names the hash instantiation that produced the row. A table
// keeps the mapping in one place instead of an if/else ladder that has to stay in sync with it.
using HashFromBits = std::uint64_t (*)(std::uint64_t, std::uint64_t);

struct GoldenType {
  const char* name;
  HashFromBits fn;
};

constexpr GoldenType kGoldenTypes[] = {
    {"i8", &hash_from_bits<std::int8_t>},    {"i16", &hash_from_bits<std::int16_t>},
    {"i32", &hash_from_bits<std::int32_t>},  {"i64", &hash_from_bits<std::int64_t>},
    {"u8", &hash_from_bits<std::uint8_t>},   {"u16", &hash_from_bits<std::uint16_t>},
    {"u32", &hash_from_bits<std::uint32_t>}, {"u64", &hash_from_bits<std::uint64_t>},
    {"f32", &hash_from_bits<float>},         {"f64", &hash_from_bits<double>},
};

// False when the golden file names a type this test does not know how to reproduce.
bool golden_hash(const GoldenRow& r, std::uint64_t* out) {
  if (r.type == "combine") {  // combine rows: key_bits = a, seed column = b
    quiver::hash64_combine(&r.key_bits, &r.seed, 1, out);
    return true;
  }
  for (const GoldenType& t : kGoldenTypes) {
    if (r.type == t.name) {
      *out = t.fn(r.key_bits, r.seed);
      return true;
    }
  }
  return false;
}

TEST(Hash, GoldenVectorsReproduce) {
  const auto rows = load_goldens();
  ASSERT_GT(rows.size(), 200u);
  std::int64_t checked = 0;
  for (const auto& r : rows) {
    std::uint64_t h = 0;
    ASSERT_TRUE(golden_hash(r, &h)) << "unknown golden type " << r.type;
    ASSERT_EQ(h, r.hash) << r.type << " key=" << std::hex << r.key_bits << " seed=" << r.seed;
    ++checked;
  }
  ASSERT_EQ(checked, static_cast<std::int64_t>(rows.size()));
}

TEST(Hash, NegativeZeroCanonicalizes) {
  float pz = 0.0f;
  float nz = -0.0f;
  std::uint64_t hp = 0;
  std::uint64_t hn = 0;
  quiver::hash64(quiver::BatchView<float>{&pz, 1}, 7, &hp);
  quiver::hash64(quiver::BatchView<float>{&nz, 1}, 7, &hn);
  EXPECT_EQ(hp, hn);
  double pzd = 0.0;
  double nzd = -0.0;
  quiver::hash64(quiver::BatchView<double>{&pzd, 1}, 7, &hp);
  quiver::hash64(quiver::BatchView<double>{&nzd, 1}, 7, &hn);
  EXPECT_EQ(hp, hn);
}

TEST(Hash, ZeroExtensionMakesEqualKeysHashEqually) {
  // key64 zero-extends the bit pattern: u8{0xFF}, u16{0xFF}, u32{0xFF} share key64 0xFF.
  std::uint8_t a = 0xFF;
  std::uint16_t b = 0xFF;
  std::uint32_t c = 0xFF;
  std::uint64_t ha = 0;
  std::uint64_t hb = 0;
  std::uint64_t hc = 0;
  quiver::hash64(quiver::BatchView<std::uint8_t>{&a, 1}, 3, &ha);
  quiver::hash64(quiver::BatchView<std::uint16_t>{&b, 1}, 3, &hb);
  quiver::hash64(quiver::BatchView<std::uint32_t>{&c, 1}, 3, &hc);
  EXPECT_EQ(ha, hb);
  EXPECT_EQ(hb, hc);
  // and i8{-1} zero-extends to 0xFF (sign NOT extended, documented) != u64{~0}.
  std::int8_t neg = -1;
  std::uint64_t hneg = 0;
  quiver::hash64(quiver::BatchView<std::int8_t>{&neg, 1}, 3, &hneg);
  EXPECT_EQ(hneg, ha);
}

TEST(Hash, CombineAliasingPermitted) {
  std::uint64_t a[8];
  std::uint64_t b[8];
  std::uint64_t expect[8];
  for (int i = 0; i < 8; ++i) {
    a[i] = 0x1111 * (i + 1);
    b[i] = 0x2222 * (i + 1);
    expect[i] = quiver_test::ref::qhash64_combine_expected(a[i], b[i]);
  }
  quiver::hash64_combine(a, b, 8, a);  // out == a
  EXPECT_EQ(std::memcmp(a, expect, sizeof expect), 0);
}

}  // namespace
