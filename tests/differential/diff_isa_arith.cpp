// K9/K10 differential matrix: every host backend vs the independent wide-math oracles,
// byte-exact (integer wrap, IEEE floats, checked count/bitmap, saturation).
// Covers: REQ-TEST-002/-003, REQ-K9-001, REQ-K10-001..002
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include "quiver/arith.h"
#include "quiver/dispatch.h"
#include "tests/testkit/generators.h"
#include "tests/testkit/reference.h"

namespace {

using quiver::ArithOp;
using quiver_test::Rng;
namespace ref = quiver_test::ref;

constexpr std::int64_t kLengths[] = {0, 1, 7, 8, 9, 31, 33, 255, 1000};

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
void run_arith_diff(std::uint64_t seed) {
  Rng rng(seed);
  for (const ArithOp op : {ArithOp::kAdd, ArithOp::kSub, ArithOp::kMul}) {
    for (const std::int64_t n : kLengths) {
      std::vector<T> a(static_cast<std::size_t>(n) + 1);
      std::vector<T> b(static_cast<std::size_t>(n) + 1);
      quiver_test::fill_uniform(rng, a.data(), n);
      quiver_test::fill_uniform(rng, b.data(), n);
      std::vector<T> got(static_cast<std::size_t>(n) + 1);
      quiver::arith(op, quiver::BatchView<T>{a.data(), n}, quiver::BatchView<T>{b.data(), n},
                    got.data());
      for (std::int64_t i = 0; i < n; ++i) {
        const T want = ref::arith_wrap_expected(op, a[static_cast<std::size_t>(i)],
                                                b[static_cast<std::size_t>(i)]);
        ASSERT_EQ(std::memcmp(&got[static_cast<std::size_t>(i)], &want, sizeof(T)), 0)
            << "op=" << static_cast<int>(op) << " n=" << n << " i=" << i;
      }
      // scalar-rhs form
      const T rhs = b.empty() ? T{} : b[0];
      quiver::arith(op, quiver::BatchView<T>{a.data(), n}, rhs, got.data());
      for (std::int64_t i = 0; i < n; ++i) {
        const T want = ref::arith_wrap_expected(op, a[static_cast<std::size_t>(i)], rhs);
        ASSERT_EQ(std::memcmp(&got[static_cast<std::size_t>(i)], &want, sizeof(T)), 0);
      }
    }
  }
}

template <class T>
void run_guarded_diff(std::uint64_t seed) {
  Rng rng(seed);
  for (const ArithOp op : {ArithOp::kAdd, ArithOp::kSub, ArithOp::kMul}) {
    for (const std::int64_t n : kLengths) {
      std::vector<T> a(static_cast<std::size_t>(n) + 1);
      std::vector<T> b(static_cast<std::size_t>(n) + 1);
      quiver_test::fill_uniform(rng, a.data(), n);
      quiver_test::fill_uniform(rng, b.data(), n);
      std::vector<T> wrapped(static_cast<std::size_t>(n) + 1);
      std::vector<std::uint8_t> bits((static_cast<std::size_t>(n) + 7) / 8 + 1);
      const std::int64_t count =
          quiver::arith_checked(op, quiver::BatchView<T>{a.data(), n},
                                quiver::BatchView<T>{b.data(), n}, wrapped.data(), bits.data());
      std::int64_t want_count = 0;
      for (std::int64_t i = 0; i < n; ++i) {
        const T va = a[static_cast<std::size_t>(i)];
        const T vb = b[static_cast<std::size_t>(i)];
        ASSERT_EQ(wrapped[static_cast<std::size_t>(i)], ref::arith_wrap_expected(op, va, vb));
        const bool ov = ref::arith_overflows_expected(op, va, vb);
        want_count += ov ? 1 : 0;
        ASSERT_EQ(ref::bit_get(bits.data(), i), ov)
            << "op=" << static_cast<int>(op) << " n=" << n << " i=" << i;
      }
      ASSERT_EQ(count, want_count);
      if (n > 0) {
        ASSERT_TRUE(ref::tail_bits_zero(bits.data(), n));
      }
      std::vector<T> sat(static_cast<std::size_t>(n) + 1);
      quiver::arith_saturating(op, quiver::BatchView<T>{a.data(), n},
                               quiver::BatchView<T>{b.data(), n}, sat.data());
      for (std::int64_t i = 0; i < n; ++i) {
        ASSERT_EQ(sat[static_cast<std::size_t>(i)],
                  ref::arith_saturate_expected(op, a[static_cast<std::size_t>(i)],
                                               b[static_cast<std::size_t>(i)]));
      }
    }
  }
}

TEST(DiffArith, AllBackendsMatchOracle) {
  for_each_backend([&] {
    run_arith_diff<std::int8_t>(0xD1FF0901ull);
    run_arith_diff<std::int16_t>(0xD1FF0902ull);
    run_arith_diff<std::int32_t>(0xD1FF0903ull);
    run_arith_diff<std::int64_t>(0xD1FF0904ull);
    run_arith_diff<std::uint8_t>(0xD1FF0905ull);
    run_arith_diff<std::uint16_t>(0xD1FF0906ull);
    run_arith_diff<std::uint32_t>(0xD1FF0907ull);
    run_arith_diff<std::uint64_t>(0xD1FF0908ull);
    run_arith_diff<float>(0xD1FF0909ull);
    run_arith_diff<double>(0xD1FF090Aull);
  });
}

// Float specials: NaN results compare as a CLASS across backends and vs the oracle (IEEE
// add/sub/mul propagate an implementation-defined NaN payload; C++ pins neither operand order
// nor payload — gate M4 decision, same policy as K6 reduce sums). Finite results stay
// bit-exact. `fill_uniform` never emits NaN/Inf, so this coverage is explicit.
template <class T>
void run_arith_nan_diff() {
  const T qnan = std::numeric_limits<T>::quiet_NaN();
  const T inf = std::numeric_limits<T>::infinity();
  const std::vector<T> a = {qnan, T{1.5}, inf, -inf, T{0}, qnan, inf, T{-3.25}};
  const std::vector<T> b = {qnan, qnan, -inf, inf, inf, T{2.0}, T{0}, T{4.0}};
  const auto n = static_cast<std::int64_t>(a.size());
  for (const ArithOp op : {ArithOp::kAdd, ArithOp::kSub, ArithOp::kMul}) {
    std::vector<T> got(a.size());
    quiver::arith(op, quiver::BatchView<T>{a.data(), n}, quiver::BatchView<T>{b.data(), n},
                  got.data());
    for (std::int64_t i = 0; i < n; ++i) {
      const T want = ref::arith_wrap_expected(op, a[static_cast<std::size_t>(i)],
                                              b[static_cast<std::size_t>(i)]);
      const T g = got[static_cast<std::size_t>(i)];
      const bool nan_class = (g != g) && (want != want);  // both NaN
      ASSERT_TRUE(nan_class || std::memcmp(&g, &want, sizeof(T)) == 0)
          << "op=" << static_cast<int>(op) << " i=" << i;
    }
  }
}

TEST(DiffArith, NaNResultsCompareAsClass) {
  for_each_backend([&] {
    run_arith_nan_diff<float>();
    run_arith_nan_diff<double>();
  });
}

TEST(DiffArithGuarded, AllBackendsMatchOracle) {
  for_each_backend([&] {
    run_guarded_diff<std::int8_t>(0xD1FF0A01ull);
    run_guarded_diff<std::int16_t>(0xD1FF0A02ull);
    run_guarded_diff<std::int32_t>(0xD1FF0A03ull);
    run_guarded_diff<std::int64_t>(0xD1FF0A04ull);
    run_guarded_diff<std::uint8_t>(0xD1FF0A05ull);
    run_guarded_diff<std::uint16_t>(0xD1FF0A06ull);
    run_guarded_diff<std::uint32_t>(0xD1FF0A07ull);
    run_guarded_diff<std::uint64_t>(0xD1FF0A08ull);
  });
}

}  // namespace
