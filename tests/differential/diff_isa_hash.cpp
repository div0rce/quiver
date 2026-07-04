// K7 differential matrix: every host backend vs the independent ADR-012 oracle, byte-exact
// (cross-ISA bit identity is this family's core promise, REQ-K7-002).
// Covers: REQ-TEST-002/-003, REQ-K7-001..002
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "quiver/dispatch.h"
#include "quiver/hash.h"
#include "tests/testkit/generators.h"
#include "tests/testkit/reference.h"

namespace {

using quiver_test::Rng;
namespace ref = quiver_test::ref;

constexpr std::int64_t kLengths[] = {0, 1, 3, 7, 8, 9, 31, 64, 65, 255, 1000, 4096};

template <class Body>
void for_each_backend(Body body) {
  for (const quiver::Isa isa :
       {quiver::Isa::kScalar, quiver::Isa::kNeon, quiver::Isa::kAvx2, quiver::Isa::kAvx512}) {
    if (isa != quiver::Isa::kScalar && !quiver::cpu_supports(isa)) {
      continue;
    }
    ASSERT_TRUE(quiver::set_isa_override(isa));
    body();
  }
  quiver::clear_isa_override();
}

template <class T>
void run_hash_diff(std::uint64_t seed) {
  Rng rng(seed);
  for (const std::int64_t n : kLengths) {
    std::vector<T> v(static_cast<std::size_t>(n) + 1);
    quiver_test::fill_uniform(rng, v.data(), n);
    const std::uint64_t hash_seed = rng.next();
    std::vector<std::uint64_t> got(static_cast<std::size_t>(n) + 1);
    quiver::hash64(quiver::BatchView<T>{v.data(), n}, hash_seed, got.data());
    for (std::int64_t i = 0; i < n; ++i) {
      ASSERT_EQ(got[static_cast<std::size_t>(i)],
                ref::qhash64_expected(v[static_cast<std::size_t>(i)], hash_seed))
          << "n=" << n << " i=" << i;
    }
  }
}

TEST(DiffHash, AllBackendsMatchOracle) {
  for_each_backend([&] {
    run_hash_diff<std::int8_t>(0xD1FF0701ull);
    run_hash_diff<std::int16_t>(0xD1FF0702ull);
    run_hash_diff<std::int32_t>(0xD1FF0703ull);
    run_hash_diff<std::int64_t>(0xD1FF0704ull);
    run_hash_diff<std::uint8_t>(0xD1FF0705ull);
    run_hash_diff<std::uint16_t>(0xD1FF0706ull);
    run_hash_diff<std::uint32_t>(0xD1FF0707ull);
    run_hash_diff<std::uint64_t>(0xD1FF0708ull);
    run_hash_diff<float>(0xD1FF0709ull);
    run_hash_diff<double>(0xD1FF070Aull);
  });
}

TEST(DiffHash, CombineMatchesOracleOnAllBackends) {
  Rng rng(0xD1FF070Bull);
  const std::int64_t n = 1000;
  std::vector<std::uint64_t> a(static_cast<std::size_t>(n));
  std::vector<std::uint64_t> b(static_cast<std::size_t>(n));
  for (std::int64_t i = 0; i < n; ++i) {
    a[static_cast<std::size_t>(i)] = rng.next();
    b[static_cast<std::size_t>(i)] = rng.next();
  }
  for_each_backend([&] {
    std::vector<std::uint64_t> got(static_cast<std::size_t>(n));
    quiver::hash64_combine(a.data(), b.data(), n, got.data());
    for (std::int64_t i = 0; i < n; ++i) {
      ASSERT_EQ(got[static_cast<std::size_t>(i)],
                ref::qhash64_combine_expected(a[static_cast<std::size_t>(i)],
                                              b[static_cast<std::size_t>(i)]));
    }
  });
}

}  // namespace
