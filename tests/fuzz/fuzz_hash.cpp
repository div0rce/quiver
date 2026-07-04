// K7 differential fuzz target (REQ-TEST-007): every host backend must produce bit-identical
// hashes (the family's cross-ISA promise), checked against the independent ADR-012 oracle.
// Module: tests/fuzz | REQs: REQ-TEST-007, REQ-K7-001..002 | ADR-009, ADR-012
#include <cstdint>
#include <cstring>
#include <vector>

#include "quiver/hash.h"
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
  const std::uint64_t seed = d.value<std::uint64_t>();
  std::vector<std::uint64_t> first(static_cast<std::size_t>(n) + 1);
  bool have_first = false;
  for (const quiver::Isa isa : quiver_fuzz::host_backends()) {
    quiver_fuzz::check(quiver::set_isa_override(isa), "override rejected");
    std::vector<std::uint64_t> got(static_cast<std::size_t>(n) + 1);
    quiver::hash64(quiver::BatchView<T>{v.data(), n}, seed, got.data());
    if (!have_first) {
      first = got;
      have_first = true;
      // scalar tier also validates against the independent oracle
      for (std::int64_t i = 0; i < n; ++i) {
        quiver_fuzz::check(got[static_cast<std::size_t>(i)] ==
                               ref::qhash64_expected(v[static_cast<std::size_t>(i)], seed),
                           "K7 scalar vs oracle");
      }
      continue;
    }
    quiver_fuzz::check(std::memcmp(got.data(), first.data(), static_cast<std::size_t>(n) * 8) == 0,
                       "K7 cross-backend hash mismatch");
  }
  quiver::clear_isa_override();
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  quiver_fuzz::Decoder d(data, size);
  quiver_fuzz::with_element_type(d, [&]<class T>(T) { run<T>(d); });
  return 0;
}
