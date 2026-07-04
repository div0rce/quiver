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

  std::vector<T> first(static_cast<std::size_t>(n) + 1);
  std::vector<std::uint8_t> first_bits((static_cast<std::size_t>(n) + 7) / 8 + 1);
  std::int64_t first_count = 0;
  bool have_first = false;
  for (const quiver::Isa isa : quiver_fuzz::host_backends()) {
    quiver_fuzz::check(quiver::set_isa_override(isa), "override rejected");
    std::vector<T> got(static_cast<std::size_t>(n) + 1);
    if (scalar_rhs) {
      quiver::arith(op, quiver::BatchView<T>{a.data(), n}, rhs, got.data());
    } else {
      quiver::arith(op, quiver::BatchView<T>{a.data(), n}, quiver::BatchView<T>{b.data(), n},
                    got.data());
    }
    std::vector<std::uint8_t> bits((static_cast<std::size_t>(n) + 7) / 8 + 1);
    std::int64_t count = 0;
    if constexpr (std::is_integral_v<T>) {
      std::vector<T> wrapped(static_cast<std::size_t>(n) + 1);
      count = scalar_rhs ? quiver::arith_checked(op, quiver::BatchView<T>{a.data(), n}, rhs,
                                                 wrapped.data(), bits.data())
                         : quiver::arith_checked(op, quiver::BatchView<T>{a.data(), n},
                                                 quiver::BatchView<T>{b.data(), n}, wrapped.data(),
                                                 bits.data());
      // checked wrapped values must equal the plain wrapping results (same op)
      quiver_fuzz::check(
          std::memcmp(wrapped.data(), got.data(), static_cast<std::size_t>(n) * sizeof(T)) == 0,
          "K10 wrapped != K9 wrap");
      std::vector<T> sat(static_cast<std::size_t>(n) + 1);
      if (scalar_rhs) {
        quiver::arith_saturating(op, quiver::BatchView<T>{a.data(), n}, rhs, sat.data());
      } else {
        quiver::arith_saturating(op, quiver::BatchView<T>{a.data(), n},
                                 quiver::BatchView<T>{b.data(), n}, sat.data());
      }
      for (std::int64_t i = 0; i < n; ++i) {
        const T vb = scalar_rhs ? rhs : b[static_cast<std::size_t>(i)];
        quiver_fuzz::check(sat[static_cast<std::size_t>(i)] ==
                               ref::arith_saturate_expected(op, a[static_cast<std::size_t>(i)], vb),
                           "K10 saturation vs oracle");
      }
    }
    if (!have_first) {
      first = got;
      first_bits = bits;
      first_count = count;
      have_first = true;
      for (std::int64_t i = 0; i < n; ++i) {  // scalar tier vs the independent oracle
        const T vb = scalar_rhs ? rhs : b[static_cast<std::size_t>(i)];
        const T want = ref::arith_wrap_expected(op, a[static_cast<std::size_t>(i)], vb);
        quiver_fuzz::check(value_matches(got[static_cast<std::size_t>(i)], want),
                           "K9 scalar vs oracle");
        if constexpr (std::is_integral_v<T>) {
          quiver_fuzz::check(
              ref::bit_get(first_bits.data(), i) ==
                  ref::arith_overflows_expected(op, a[static_cast<std::size_t>(i)], vb),
              "K10 overflow bit vs oracle");
        }
      }
      continue;
    }
    if constexpr (std::is_floating_point_v<T>) {
      for (std::int64_t i = 0; i < n; ++i) {  // NaN-class cross-backend (see value_matches)
        quiver_fuzz::check(
            value_matches(got[static_cast<std::size_t>(i)], first[static_cast<std::size_t>(i)]),
            "K9 cross-backend mismatch");
      }
    } else {
      quiver_fuzz::check(
          std::memcmp(got.data(), first.data(), static_cast<std::size_t>(n) * sizeof(T)) == 0,
          "K9 cross-backend mismatch");
    }
    if constexpr (std::is_integral_v<T>) {
      quiver_fuzz::check(count == first_count, "K10 count mismatch");
      quiver_fuzz::check(
          std::memcmp(bits.data(), first_bits.data(), (static_cast<std::size_t>(n) + 7) / 8) == 0,
          "K10 bitmap mismatch");
    }
  }
  quiver::clear_isa_override();
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  quiver_fuzz::Decoder d(data, size);
  quiver_fuzz::with_element_type(d, [&]<class T>(T) { run<T>(d); });
  return 0;
}
