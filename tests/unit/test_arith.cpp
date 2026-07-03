// K9/K10 unit tests: wrapping-by-name boundaries, the REQ-K10-002 boundary matrix
// ({min, min+1, -1, 0, 1, max-1, max} cross-products per op, INT_MIN edge cases enumerated),
// exact saturation, ADR-014 count/bitmap agreement, and the aliasing deltas.
// Covers: REQ-K9-001, REQ-K10-001..003
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include "quiver/arith.h"
#include "tests/testkit/reference.h"

namespace {

namespace ref = quiver_test::ref;
using quiver::ArithOp;

TEST(Arith, WrappingByName) {
  std::int32_t a = std::numeric_limits<std::int32_t>::max();
  std::int32_t b = 1;
  std::int32_t r = 0;
  quiver::arith(ArithOp::kAdd, quiver::BatchView<std::int32_t>{&a, 1},
                quiver::BatchView<std::int32_t>{&b, 1}, &r);
  EXPECT_EQ(r, std::numeric_limits<std::int32_t>::min());  // documented wrap
}

TEST(Arith, InPlaceAliasing) {
  std::int64_t a[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  quiver::arith(ArithOp::kMul, quiver::BatchView<std::int64_t>{a, 9}, std::int64_t{3}, a);
  for (int i = 0; i < 9; ++i) {
    EXPECT_EQ(a[i], 3 * (i + 1));
  }
}

// The REQ-K10-002 boundary matrix, run for every op × int type via the checked and
// saturating forms against the independent wide-math oracle.
template <class T>
void boundary_matrix() {
  const T probes[] = {std::numeric_limits<T>::min(),
                      static_cast<T>(std::numeric_limits<T>::min() + 1),
                      static_cast<T>(-1),
                      T{0},
                      T{1},
                      static_cast<T>(std::numeric_limits<T>::max() - 1),
                      std::numeric_limits<T>::max()};
  constexpr int kN = static_cast<int>(sizeof(probes) / sizeof(probes[0]));
  T a[kN * kN];
  T b[kN * kN];
  for (int i = 0; i < kN; ++i) {
    for (int j = 0; j < kN; ++j) {
      a[i * kN + j] = probes[i];
      b[i * kN + j] = probes[j];
    }
  }
  constexpr std::int64_t n = kN * kN;
  for (const ArithOp op : {ArithOp::kAdd, ArithOp::kSub, ArithOp::kMul}) {
    T wrapped[n];
    std::uint8_t bits[(n + 7) / 8];
    const std::int64_t count = quiver::arith_checked(op, quiver::BatchView<T>{a, n},
                                                     quiver::BatchView<T>{b, n}, wrapped, bits);
    std::int64_t want_count = 0;
    for (std::int64_t i = 0; i < n; ++i) {
      const bool ov = ref::arith_overflows_expected(op, a[i], b[i]);
      want_count += ov ? 1 : 0;
      ASSERT_EQ(wrapped[i], ref::arith_wrap_expected(op, a[i], b[i]))
          << "op=" << static_cast<int>(op) << " i=" << i;
      ASSERT_EQ(ref::bit_get(bits, i), ov) << "op=" << static_cast<int>(op) << " i=" << i;
    }
    ASSERT_EQ(count, want_count);
    ASSERT_TRUE(ref::tail_bits_zero(bits, n));

    T sat[n];
    quiver::arith_saturating(op, quiver::BatchView<T>{a, n}, quiver::BatchView<T>{b, n}, sat);
    for (std::int64_t i = 0; i < n; ++i) {
      ASSERT_EQ(sat[i], ref::arith_saturate_expected(op, a[i], b[i]))
          << "op=" << static_cast<int>(op) << " i=" << i;
    }
  }
}

TEST(ArithGuarded, BoundaryMatrixI8) {
  boundary_matrix<std::int8_t>();
}
TEST(ArithGuarded, BoundaryMatrixI16) {
  boundary_matrix<std::int16_t>();
}
TEST(ArithGuarded, BoundaryMatrixI32) {
  boundary_matrix<std::int32_t>();
}
TEST(ArithGuarded, BoundaryMatrixI64) {
  boundary_matrix<std::int64_t>();
}
TEST(ArithGuarded, BoundaryMatrixU8) {
  boundary_matrix<std::uint8_t>();
}
TEST(ArithGuarded, BoundaryMatrixU16) {
  boundary_matrix<std::uint16_t>();
}
TEST(ArithGuarded, BoundaryMatrixU32) {
  boundary_matrix<std::uint32_t>();
}
TEST(ArithGuarded, BoundaryMatrixU64) {
  boundary_matrix<std::uint64_t>();
}

TEST(ArithGuarded, IntMinEdgeCases) {
  // INT_MIN * -1 and INT_MIN / negation family (REQ-K10-002 enumerated cases).
  std::int32_t a = std::numeric_limits<std::int32_t>::min();
  std::int32_t b = -1;
  std::int32_t r = 0;
  std::uint8_t bit = 0;
  EXPECT_EQ(quiver::arith_checked(ArithOp::kMul, quiver::BatchView<std::int32_t>{&a, 1},
                                  quiver::BatchView<std::int32_t>{&b, 1}, &r, &bit),
            1);
  EXPECT_EQ(r, std::numeric_limits<std::int32_t>::min());  // wrapped
  EXPECT_EQ(bit & 1, 1);
  quiver::arith_saturating(ArithOp::kMul, quiver::BatchView<std::int32_t>{&a, 1},
                           quiver::BatchView<std::int32_t>{&b, 1}, &r);
  EXPECT_EQ(r, std::numeric_limits<std::int32_t>::max());  // (-min) saturates to max
  // 0 - INT_MIN overflows; saturates to max.
  std::int32_t zero = 0;
  EXPECT_EQ(quiver::arith_checked(ArithOp::kSub, quiver::BatchView<std::int32_t>{&zero, 1},
                                  quiver::BatchView<std::int32_t>{&a, 1}, &r, nullptr),
            1);
  quiver::arith_saturating(ArithOp::kSub, quiver::BatchView<std::int32_t>{&zero, 1},
                           quiver::BatchView<std::int32_t>{&a, 1}, &r);
  EXPECT_EQ(r, std::numeric_limits<std::int32_t>::max());
}

TEST(ArithGuarded, NullBitmapStillCounts) {
  std::uint8_t a[16];
  std::uint8_t b[16];
  for (int i = 0; i < 16; ++i) {
    a[i] = 250;
    b[i] = static_cast<std::uint8_t>(i);
  }
  std::uint8_t r[16];
  const auto count = quiver::arith_checked(ArithOp::kAdd, quiver::BatchView<std::uint8_t>{a, 16},
                                           quiver::BatchView<std::uint8_t>{b, 16}, r, nullptr);
  EXPECT_EQ(count, 10);  // 250 + i overflows for i >= 6
}

TEST(Arith, ValidityOverloadIsComposition) {
  const std::int64_t n = 27;
  std::int16_t a[27];
  std::int16_t b[27];
  std::uint8_t av[4] = {0xF0, 0x33, 0xFF, 0x01};
  std::uint8_t bv[4] = {0xCC, 0xFF, 0x0F, 0x07};
  for (int i = 0; i < n; ++i) {
    a[i] = static_cast<std::int16_t>(i * 100);
    b[i] = static_cast<std::int16_t>(7 - i);
  }
  std::int16_t out[27];
  std::uint8_t out_v[4];
  quiver::arith(ArithOp::kAdd, quiver::BatchView<std::int16_t>{a, n},
                quiver::BatchView<std::int16_t>{b, n}, quiver::BitmapView{av},
                quiver::BitmapView{bv}, out, out_v);
  for (int i = 0; i < n; ++i) {
    EXPECT_EQ(out[i], static_cast<std::int16_t>(a[i] + b[i]));
    EXPECT_EQ(ref::bit_get(out_v, i), ref::bit_get(av, i) && ref::bit_get(bv, i));
  }
  EXPECT_TRUE(ref::tail_bits_zero(out_v, n));
}

}  // namespace
