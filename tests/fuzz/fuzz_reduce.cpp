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

template <class T>
void run(quiver_fuzz::Decoder& d) {
  const std::int64_t n = d.len(kMaxN);
  std::vector<T> v(static_cast<std::size_t>(n) + 1);
  d.fill(v.data(), n);
  const std::vector<std::uint8_t> validity = quiver_fuzz::bitmap(d, n);
  const std::uint8_t* vd = d.boolean() ? validity.data() : nullptr;
  const std::vector<std::uint32_t> selv = quiver_fuzz::selvec(d, n);
  const bool with_sel = d.boolean();
  const quiver::BatchView<T> in{v.data(), n};
  const quiver::BitmapView val{vd};
  const quiver::SelVec sel{selv.data(), static_cast<std::int64_t>(selv.size())};
  // Oracle-side empty-selection disambiguation (see reg_empty_selvec.cpp).
  static constexpr std::uint32_t kNoIdx = 0;
  const ref::Participation p{vd, with_sel ? (selv.empty() ? &kNoIdx : selv.data()) : nullptr,
                             static_cast<std::int64_t>(selv.size())};

  T min0{};
  T max0{};
  quiver::SumType<T> sum0{};
  quiver::Sma<T> sma0{};
  bool overflow0 = false;
  bool first = true;
  for (const quiver::Isa isa : quiver_fuzz::host_backends()) {
    quiver_fuzz::check(quiver::set_isa_override(isa), "override rejected");
    const T mn = with_sel ? quiver::reduce_min(in, val, sel) : quiver::reduce_min(in, val);
    const T mx = with_sel ? quiver::reduce_max(in, val, sel) : quiver::reduce_max(in, val);
    const auto sm =
        with_sel ? quiver::reduce_sum_wrap(in, val, sel) : quiver::reduce_sum_wrap(in, val);
    const quiver::Sma<T> sma =
        with_sel ? quiver::compute_sma(in, val, sel) : quiver::compute_sma(in, val);

    if constexpr (std::is_floating_point_v<T>) {
      // Per-backend policy oracle for sums (dense AVX2 = blocked {w,a}; else strict fold).
      // NaN results compare as a class — payloads follow hardware operand order, which C++
      // does not pin (gate M4 amendment to the ADR-013 oracle claim).
      const auto want =
          (!with_sel && isa == quiver::Isa::kAvx2)
              ? ref::sum_blocked_expected<T>(v.data(), n, vd, sizeof(T) == 4 ? 8 : 4, 4)
              : ref::sum_expected(v.data(), n, p);
      const bool nan_class = (sm != sm) && (want != want);
      quiver_fuzz::check(nan_class || std::memcmp(&sm, &want, sizeof(sm)) == 0,
                         "K6 float sum vs policy");
    }
    bool overflow = false;
    if constexpr (std::is_integral_v<T>) {
      quiver::SumType<T> checked_sum{};
      overflow = with_sel ? quiver::reduce_sum_checked(in, val, sel, &checked_sum)
                          : quiver::reduce_sum_checked(in, val, &checked_sum);
      // The checked form always reports the wrapped value (API-K6-003), so it must equal
      // the wrap kernel's result; narrow types provably cannot overflow at n <= 512.
      quiver_fuzz::check(std::memcmp(&checked_sum, &sm, sizeof(sm)) == 0,
                         "K6 checked sum != wrapped sum");
      quiver_fuzz::check(!overflow || sizeof(T) == 8, "K6 narrow checked sum overflowed");
    }
    if (first) {
      min0 = mn;
      max0 = mx;
      sum0 = sm;
      sma0 = sma;
      overflow0 = overflow;
      first = false;
      continue;
    }
    quiver_fuzz::check(overflow == overflow0, "K6 overflow flag mismatch");
    quiver_fuzz::check(std::memcmp(&mn, &min0, sizeof(T)) == 0, "K6 min mismatch");
    quiver_fuzz::check(std::memcmp(&mx, &max0, sizeof(T)) == 0, "K6 max mismatch");
    if constexpr (std::is_integral_v<T>) {
      quiver_fuzz::check(std::memcmp(&sm, &sum0, sizeof(sm)) == 0, "K6 int sum mismatch");
    }
    quiver_fuzz::check(std::memcmp(&sma.min, &sma0.min, sizeof(T)) == 0 &&
                           std::memcmp(&sma.max, &sma0.max, sizeof(T)) == 0 &&
                           sma.null_count == sma0.null_count,
                       "K6 sma mismatch");
  }
  quiver::clear_isa_override();
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  quiver_fuzz::Decoder d(data, size);
  quiver_fuzz::with_element_type(d, [&]<class T>(T) { run<T>(d); });
  return 0;
}
