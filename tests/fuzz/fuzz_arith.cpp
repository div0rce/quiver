// K9/K10 differential fuzz target (REQ-TEST-007): wrapping/IEEE arithmetic, checked
// overflow reporting (count + position bitmap), and saturation — every host backend vs the
// scalar baseline, with the scalar tier checked against the independent wide-math oracles.
// Module: tests/fuzz | REQs: REQ-TEST-007, REQ-K9-001, REQ-K10-001..002 | ADR-009, ADR-014
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>

#include "quiver/arith.h"
#include "tests/fuzz/fuzz_common.h"
#include "tests/testkit/reference.h"

namespace {

namespace ref = quiver_test::ref;
using quiver::ArithOp;

constexpr std::int64_t kMaxN = 512;

// K9 float results compare as a NaN CLASS, not bit-exact: IEEE add/sub/mul propagate
// whichever operand's NaN payload the hardware picks, and C++ pins neither the operand order
// nor the payload — so a NaN result is not reproducible across TUs (kernel vs oracle) or
// backends (scalar vs NEON), only its NaN-ness is. Finite results stay required bit-exact.
// Same documented policy as K6 reduce sums (gate M4; PRD 08 §3.6 float-specials).
template <class T>
bool value_matches(T x, T y) noexcept {
  if constexpr (std::is_floating_point_v<T>) {
    if (x != x && y != y) {  // both NaN → class-equal
      return true;
    }
  }
  return std::memcmp(&x, &y, sizeof(T)) == 0;
}

// One fuzz case: the operands, the op, and whether the right-hand side is a scalar broadcast.
template <class T>
struct ArithCase {
  const T* a;
  const T* b;
  std::int64_t n;
  T rhs;
  ArithOp op;
  bool scalar_rhs;
};

// The right-hand value at lane i, under whichever rhs shape this case selected.
template <class T>
T rhs_at(const ArithCase<T>& c, std::int64_t i) {
  return c.scalar_rhs ? c.rhs : c.b[static_cast<std::size_t>(i)];
}

// What one backend produced.
template <class T>
struct ArithOutputs {
  std::vector<T> values;
  std::vector<std::uint8_t> bits;
  std::int64_t count;
};

template <class T>
void run_wrap(const ArithCase<T>& c, T* out) {
  if (c.scalar_rhs) {
    quiver::arith(c.op, quiver::BatchView<T>{c.a, c.n}, c.rhs, out);
    return;
  }
  quiver::arith(c.op, quiver::BatchView<T>{c.a, c.n}, quiver::BatchView<T>{c.b, c.n}, out);
}

template <class T>
std::int64_t run_checked(const ArithCase<T>& c, T* wrapped, std::uint8_t* bits) {
  if (c.scalar_rhs) {
    return quiver::arith_checked(c.op, quiver::BatchView<T>{c.a, c.n}, c.rhs, wrapped, bits);
  }
  return quiver::arith_checked(c.op, quiver::BatchView<T>{c.a, c.n}, quiver::BatchView<T>{c.b, c.n},
                               wrapped, bits);
}

template <class T>
void run_saturating(const ArithCase<T>& c, T* out) {
  if (c.scalar_rhs) {
    quiver::arith_saturating(c.op, quiver::BatchView<T>{c.a, c.n}, c.rhs, out);
    return;
  }
  quiver::arith_saturating(c.op, quiver::BatchView<T>{c.a, c.n}, quiver::BatchView<T>{c.b, c.n},
                           out);
}

// The K10 guarded forms, checked against K9 and against the independent oracle.
template <class T>
std::int64_t check_guarded(const ArithCase<T>& c, const std::vector<T>& wrap_values,
                           std::uint8_t* bits) {
  std::vector<T> wrapped(static_cast<std::size_t>(c.n) + 1);
  const std::int64_t count = run_checked(c, wrapped.data(), bits);
  // checked wrapped values must equal the plain wrapping results (same op)
  quiver_fuzz::check(std::memcmp(wrapped.data(), wrap_values.data(),
                                 static_cast<std::size_t>(c.n) * sizeof(T)) == 0,
                     "K10 wrapped != K9 wrap");
  std::vector<T> sat(static_cast<std::size_t>(c.n) + 1);
  run_saturating(c, sat.data());
  for (std::int64_t i = 0; i < c.n; ++i) {
    quiver_fuzz::check(
        sat[static_cast<std::size_t>(i)] ==
            ref::arith_saturate_expected(c.op, c.a[static_cast<std::size_t>(i)], rhs_at(c, i)),
        "K10 saturation vs oracle");
  }
  return count;
}

// The scalar tier is checked against the independent oracle, not just against its peers.
template <class T>
void check_against_oracle(const ArithCase<T>& c, const ArithOutputs<T>& got) {
  for (std::int64_t i = 0; i < c.n; ++i) {
    const T vb = rhs_at(c, i);
    const T want = ref::arith_wrap_expected(c.op, c.a[static_cast<std::size_t>(i)], vb);
    quiver_fuzz::check(value_matches(got.values[static_cast<std::size_t>(i)], want),
                       "K9 scalar vs oracle");
    if constexpr (std::is_integral_v<T>) {
      quiver_fuzz::check(
          ref::bit_get(got.bits.data(), i) ==
              ref::arith_overflows_expected(c.op, c.a[static_cast<std::size_t>(i)], vb),
          "K10 overflow bit vs oracle");
    }
  }
}

// Later backends must reproduce the first backend's outputs. Floats compare NaN as a class
// (see value_matches); integers compare bit-exactly.
template <class T>
void expect_same(const ArithCase<T>& c, const ArithOutputs<T>& got, const ArithOutputs<T>& want) {
  if constexpr (std::is_floating_point_v<T>) {
    for (std::int64_t i = 0; i < c.n; ++i) {
      quiver_fuzz::check(value_matches(got.values[static_cast<std::size_t>(i)],
                                       want.values[static_cast<std::size_t>(i)]),
                         "K9 cross-backend mismatch");
    }
  } else {
    quiver_fuzz::check(std::memcmp(got.values.data(), want.values.data(),
                                   static_cast<std::size_t>(c.n) * sizeof(T)) == 0,
                       "K9 cross-backend mismatch");
    quiver_fuzz::check(got.count == want.count, "K10 count mismatch");
    quiver_fuzz::check(std::memcmp(got.bits.data(), want.bits.data(),
                                   (static_cast<std::size_t>(c.n) + 7) / 8) == 0,
                       "K10 bitmap mismatch");
  }
}

template <class T>
void run(quiver_fuzz::Decoder& d) {
  const std::int64_t n = d.len(kMaxN);
  const auto op = static_cast<ArithOp>(d.pick(3));
  const bool scalar_rhs = d.boolean();
  std::vector<T> a(static_cast<std::size_t>(n) + 1);
  std::vector<T> b(static_cast<std::size_t>(n) + 1);
  d.fill(a.data(), n);
  d.fill(b.data(), n);
  const T rhs = d.value<T>();
  const ArithCase<T> c{a.data(), b.data(), n, rhs, op, scalar_rhs};

  ArithOutputs<T> first{};
  bool have_first = false;
  for (const quiver::Isa isa : quiver_fuzz::host_backends()) {
    quiver_fuzz::check(quiver::set_isa_override(isa), "override rejected");
    ArithOutputs<T> got{std::vector<T>(static_cast<std::size_t>(n) + 1),
                        std::vector<std::uint8_t>((static_cast<std::size_t>(n) + 7) / 8 + 1), 0};
    run_wrap(c, got.values.data());
    if constexpr (std::is_integral_v<T>) {
      got.count = check_guarded(c, got.values, got.bits.data());
    }
    if (!have_first) {
      first = got;
      have_first = true;
      check_against_oracle(c, got);
      continue;
    }
    expect_same(c, got, first);
  }
  quiver::clear_isa_override();
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  quiver_fuzz::Decoder d(data, size);
  quiver_fuzz::with_element_type(d, [&]<class T>(T) { run<T>(d); });
  return 0;
}
