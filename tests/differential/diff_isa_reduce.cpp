// K6 differential matrix: reductions vs the participation-order oracle (float sums bit-exact
// against the strict-fold policy oracle, REQ-TEST-004) over L-set × null density × selection.
// Covers: REQ-TEST-002/-003/-004, REQ-K6-001..004
#include <cstdint>
#include <cstdlib>
#include <vector>

#include <gtest/gtest.h>

#include "quiver/dispatch.h"
#include "quiver/reduce.h"
#include "tests/testkit/assertions.h"
#include "tests/testkit/generators.h"
#include "tests/testkit/reference.h"
#include <cstring>

namespace {

using quiver_test::Rng;
namespace ref = quiver_test::ref;

// Applies a functor to a zero of each of the ten element types (REQ-API-004 sweep helper).
template <class F>
void for_each_element_type(F f) {
  f(std::int8_t{});
  f(std::int16_t{});
  f(std::int32_t{});
  f(std::int64_t{});
  f(std::uint8_t{});
  f(std::uint16_t{});
  f(std::uint32_t{});
  f(std::uint64_t{});
  f(float{});
  f(double{});
}

// PR-tier length set: every tail residue for widths <= 64 B plus representative large sizes
// (REQ-TEST-003 L-set); QUIVER_NIGHTLY widens the sampled axes to the full cross product.
constexpr std::int64_t kLengths[] = {0,  1,  2,  3,  7,   8,   9,   15,  16,  17,  31,   32,
                                     33, 63, 64, 65, 127, 128, 129, 255, 256, 257, 1000, 4096};

[[maybe_unused]] inline bool nightly() {
  return std::getenv("QUIVER_NIGHTLY") != nullptr;
}
// Runs `body` once per host-executable tier (REQ-TEST-003 backend column; scalar-only at M3).
template <class Body>
void for_each_backend(Body body) {
  for (const quiver::Isa isa :
       {quiver::Isa::kScalar, quiver::Isa::kNeon, quiver::Isa::kAvx2, quiver::Isa::kAvx512}) {
    if (isa != quiver::Isa::kScalar && !quiver::cpu_supports(isa)) {
      continue;
    }
    ASSERT_TRUE(quiver::set_isa_override(isa));
    quiver::warmup();
    body(isa);
  }
  quiver::clear_isa_override();
}

// Float-sum expectation per (backend, shape): dense shapes follow the EFFECTIVE backend's
// documented ADR-013 policy (scalar = strict fold; AVX2 = blocked f32 {w=8,a=4} / f64
// {w=4,a=4}; NEON = blocked f32 {w=4,a=4} / f64 {w=2,a=4}); selected shapes are the strict
// fold on every backend (REQ-TEST-004). The effective backend can sit below the ISA cap:
// dispatch falls through empty row slots, so a kAvx512 cap resolves to the AVX2 backend
// until the AVX-512 rows land at M7 (update the policy map then).
template <class T>
quiver::SumType<T> expected_sum(quiver::Isa isa, const T* data, std::int64_t n,
                                const ref::Participation& p) {
  if constexpr (std::is_floating_point_v<T>) {
    if (p.sel == nullptr) {
      if (isa >= quiver::Isa::kAvx2 && quiver::cpu_supports(quiver::Isa::kAvx2)) {
        return ref::sum_blocked_expected<T>(data, n, p.validity, {sizeof(T) == 4 ? 8 : 4, 4});
      }
      if (isa >= quiver::Isa::kNeon && quiver::cpu_supports(quiver::Isa::kNeon)) {
        return ref::sum_blocked_expected<T>(data, n, p.validity, {sizeof(T) == 4 ? 4 : 2, 4});
      }
    }
  }
  return ref::sum_expected(data, n, p);
}

template <class T>
struct ReduceTriple {
  T min;
  T max;
  quiver::SumType<T> sum;
};

// One shape under test: the batch, its validity, and the selection (when this case uses one).
template <class T>
struct ReduceShape {
  quiver::BatchView<T> in;
  quiver::BitmapView val;
  quiver::SelVec sel;
  bool with_sel;
};

// Bit-exact comparison (floats: the per-backend policy oracle, REQ-TEST-004). NaN float sums
// compare as a CLASS: IEEE add propagates whichever operand's payload the hardware sees first,
// and C++ does not pin FP operand order (gate M4 amendment).
template <class T>
void expect_reduce_matches(const ReduceTriple<T>& got, const ReduceTriple<T>& want,
                           std::int64_t n) {
  bool sum_nan_class = false;
  if constexpr (std::is_floating_point_v<T>) {
    sum_nan_class = (got.sum != got.sum) && (want.sum != want.sum);
  }
  ASSERT_EQ(std::memcmp(&got.min, &want.min, sizeof(T)), 0) << "min n=" << n;
  ASSERT_EQ(std::memcmp(&got.max, &want.max, sizeof(T)), 0) << "max n=" << n;
  ASSERT_TRUE(sum_nan_class || std::memcmp(&got.sum, &want.sum, sizeof(want.sum)) == 0)
      << "sum n=" << n;
}

template <class T>
void check_reduce_shape(quiver::Isa isa, ReduceShape<T> s, const ref::Participation& p) {
  const std::int64_t n = s.in.len;
  const ReduceTriple<T> want{ref::min_expected(s.in.data, n, p), ref::max_expected(s.in.data, n, p),
                             expected_sum(isa, s.in.data, n, p)};
  const ReduceTriple<T> got{
      s.with_sel ? quiver::reduce_min(s.in, s.val, s.sel) : quiver::reduce_min(s.in, s.val),
      s.with_sel ? quiver::reduce_max(s.in, s.val, s.sel) : quiver::reduce_max(s.in, s.val),
      s.with_sel ? quiver::reduce_sum_wrap(s.in, s.val, s.sel)
                 : quiver::reduce_sum_wrap(s.in, s.val)};
  expect_reduce_matches(got, want, n);

  const quiver::MinMaxSummary<T> sma = s.with_sel ? quiver::compute_min_max(s.in, s.val, s.sel)
                                                  : quiver::compute_min_max(s.in, s.val);
  ASSERT_EQ(std::memcmp(&sma.min, &want.min, sizeof(T)), 0);
  ASSERT_EQ(std::memcmp(&sma.max, &want.max, sizeof(T)), 0);
}

template <class T>
void run_reduce_diff(quiver::Isa isa, std::uint64_t seed) {
  Rng rng(seed);
  for (const std::int64_t n : kLengths) {
    std::vector<T> v(static_cast<std::size_t>(n) + 1);
    quiver_test::fill_uniform(rng, v.data(), n);
    for (const int null_pct : {0, 10, 50}) {
      std::vector<std::uint8_t> validity((static_cast<std::size_t>(n) + 7) / 8 + 1);
      quiver_test::fill_bitmap_uniform(rng, validity.data(), n, 100 - null_pct);
      const std::uint8_t* vd = null_pct == 0 ? nullptr : validity.data();
      for (const bool with_sel : {false, true}) {
        std::vector<std::uint8_t> selbits((static_cast<std::size_t>(n) + 7) / 8 + 1);
        quiver_test::fill_bitmap_uniform(rng, selbits.data(), n, 60);
        const auto selv = ref::selvec_expected(selbits.data(), n);
        // The oracle encodes "no selection" as sel == nullptr, so an EMPTY selection (whose
        // vector data() is null) needs a non-null stand-in — same disambiguation the facade
        // performs (reg_empty_selvec.cpp).
        static constexpr std::uint32_t kNoIdx = 0;
        const std::uint32_t* selp = selv.empty() ? &kNoIdx : selv.data();
        const ref::Participation p{vd, with_sel ? selp : nullptr,
                                   static_cast<std::int64_t>(selv.size())};
        check_reduce_shape<T>(isa,
                              {quiver::BatchView<T>{v.data(), n}, quiver::BitmapView{vd},
                               quiver::SelVec{selv.data(), static_cast<std::int64_t>(selv.size())},
                               with_sel},
                              p);
      }
    }
  }
}

TEST(DiffReduce, AllBackendsMatchOracle) {
  for_each_backend([&](quiver::Isa isa) {
    for_each_element_type([&]<class T>(T) {
      run_reduce_diff<T>(isa, 0xD1FF0006ull + static_cast<std::uint64_t>(isa));
    });
  });
}

}  // namespace
