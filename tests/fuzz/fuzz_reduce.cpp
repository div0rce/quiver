// K6 differential fuzz target (REQ-TEST-007): min/max/sum_wrap/sum_checked/SMA across all
// element types × validity × selection shapes on every host backend. min/max/SMA/checked
// compare bit-exact cross-backend (canonical qNaN makes float min/max ISA-stable, PRD 08
// §3.3). Float wrapping sums follow each backend's DOCUMENTED ADR-013 policy — dense AVX2
// is the blocked A=4 shape, everything else the strict fold — so each backend is checked
// against its own policy oracle instead of raw cross-backend equality (the PRD 08 §3 float
// rules carve-out to REQ-TEST-007).
// Module: tests/fuzz | REQs: REQ-TEST-007, REQ-K6-001..005 | ADR-009, ADR-013
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>

#include "quiver/reduce.h"
#include "tests/fuzz/fuzz_common.h"
#include "tests/testkit/reference.h"

namespace {

namespace ref = quiver_test::ref;

constexpr std::int64_t kMaxN = 512;

// The shape one fuzz input describes: the batch, its validity, and the selection when used.
template <class T>
struct ReduceShape {
  quiver::BatchView<T> in;
  quiver::BitmapView val;
  quiver::SelVec sel;
  bool with_sel;
};

// Every K6 result one backend produced for that shape.
template <class T>
struct ReduceResults {
  T min;
  T max;
  quiver::SumType<T> sum;
  quiver::MinMaxSummary<T> sma;
  bool overflow;
};

template <class T>
ReduceResults<T> reduce_all(const ReduceShape<T>& s) {
  return {s.with_sel ? quiver::reduce_min(s.in, s.val, s.sel) : quiver::reduce_min(s.in, s.val),
          s.with_sel ? quiver::reduce_max(s.in, s.val, s.sel) : quiver::reduce_max(s.in, s.val),
          s.with_sel ? quiver::reduce_sum_wrap(s.in, s.val, s.sel)
                     : quiver::reduce_sum_wrap(s.in, s.val),
          s.with_sel ? quiver::compute_min_max(s.in, s.val, s.sel)
                     : quiver::compute_min_max(s.in, s.val),
          false};
}

// The blocked float-sum shape this backend uses, or w == 0 meaning the strict fold. The
// EFFECTIVE backend can sit below the ISA cap (empty rows fall through): a kAvx512 cap resolves
// to the AVX2 backend until M7.
inline ref::BlockShape blocked_shape_for(quiver::Isa isa, bool with_sel, std::size_t elem_size) {
  if (with_sel) {
    return {0, 0};  // selected shapes use the strict fold on every backend
  }
  if (isa >= quiver::Isa::kAvx2 && quiver::cpu_supports(quiver::Isa::kAvx2)) {
    return {elem_size == 4 ? 8 : 4, 4};
  }
  if (isa >= quiver::Isa::kNeon && quiver::cpu_supports(quiver::Isa::kNeon)) {
    return {elem_size == 4 ? 4 : 2, 4};
  }
  return {0, 0};
}

// Per-backend policy oracle for float sums. NaN results compare as a CLASS — payloads follow
// hardware operand order, which C++ does not pin (gate M4).
template <class T>
void check_float_sum(const ReduceShape<T>& s, const ref::Participation& p, quiver::SumType<T> sum,
                     quiver::Isa isa) {
  const ref::BlockShape shape = blocked_shape_for(isa, s.with_sel, sizeof(T));
  const auto want = shape.w == 0
                        ? ref::sum_expected(s.in.data, s.in.len, p)
                        : ref::sum_blocked_expected<T>(s.in.data, s.in.len, s.val.bits, shape);
  const bool nan_class = (sum != sum) && (want != want);
  quiver_fuzz::check(nan_class || std::memcmp(&sum, &want, sizeof(sum)) == 0,
                     "K6 float sum vs policy");
}

// The checked form always reports the wrapped value (API-K6-003), so it must equal the wrap
// kernel's result; narrow types provably cannot overflow at n <= 512.
template <class T>
bool check_integer_sum(const ReduceShape<T>& s, quiver::SumType<T> sum) {
  quiver::SumType<T> checked_sum{};
  const bool overflow = s.with_sel ? quiver::reduce_sum_checked(s.in, s.val, s.sel, &checked_sum)
                                   : quiver::reduce_sum_checked(s.in, s.val, &checked_sum);
  quiver_fuzz::check(std::memcmp(&checked_sum, &sum, sizeof(sum)) == 0,
                     "K6 checked sum != wrapped sum");
  quiver_fuzz::check(!overflow || sizeof(T) == 8, "K6 narrow checked sum overflowed");
  return overflow;
}

// Later backends must reproduce the first backend's results exactly.
template <class T>
void expect_same(const ReduceResults<T>& got, const ReduceResults<T>& want) {
  quiver_fuzz::check(got.overflow == want.overflow, "K6 overflow flag mismatch");
  quiver_fuzz::check(std::memcmp(&got.min, &want.min, sizeof(T)) == 0, "K6 min mismatch");
  quiver_fuzz::check(std::memcmp(&got.max, &want.max, sizeof(T)) == 0, "K6 max mismatch");
  if constexpr (std::is_integral_v<T>) {
    quiver_fuzz::check(std::memcmp(&got.sum, &want.sum, sizeof(got.sum)) == 0,
                       "K6 int sum mismatch");
  }
  quiver_fuzz::check(std::memcmp(&got.sma.min, &want.sma.min, sizeof(T)) == 0 &&
                         std::memcmp(&got.sma.max, &want.sma.max, sizeof(T)) == 0 &&
                         got.sma.null_count == want.sma.null_count,
                     "K6 sma mismatch");
}

template <class T>
void run(quiver_fuzz::Decoder& d) {
  const std::int64_t n = d.len(kMaxN);
  std::vector<T> v(static_cast<std::size_t>(n) + 1);
  d.fill(v.data(), n);
  const std::vector<std::uint8_t> validity = quiver_fuzz::bitmap(d, n);
  const std::uint8_t* vd = d.boolean() ? validity.data() : nullptr;
  const std::vector<std::uint32_t> selv = quiver_fuzz::selvec(d, n);
  const bool with_sel = d.boolean();
  const ReduceShape<T> s{quiver::BatchView<T>{v.data(), n}, quiver::BitmapView{vd},
                         quiver::SelVec{selv.data(), static_cast<std::int64_t>(selv.size())},
                         with_sel};
  // Oracle-side empty-selection disambiguation (see reg_empty_selvec.cpp).
  static constexpr std::uint32_t kNoIdx = 0;
  const ref::Participation p{vd, with_sel ? (selv.empty() ? &kNoIdx : selv.data()) : nullptr,
                             static_cast<std::int64_t>(selv.size())};

  ReduceResults<T> ref_results{};
  bool first = true;
  for (const quiver::Isa isa : quiver_fuzz::host_backends()) {
    quiver_fuzz::check(quiver::set_isa_override(isa), "override rejected");
    ReduceResults<T> got = reduce_all(s);
    if constexpr (std::is_floating_point_v<T>) {
      check_float_sum(s, p, got.sum, isa);
    }
    if constexpr (std::is_integral_v<T>) {
      got.overflow = check_integer_sum(s, got.sum);
    }
    if (first) {
      ref_results = got;
      first = false;
      continue;
    }
    expect_same(got, ref_results);
  }
  quiver::clear_isa_override();
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  quiver_fuzz::Decoder d(data, size);
  quiver_fuzz::with_element_type(d, [&]<class T>(T) { run<T>(d); });
  return 0;
}
