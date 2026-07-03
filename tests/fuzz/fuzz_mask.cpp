// K4 differential fuzz target (REQ-TEST-007): combine (all four ops), not, popcount, and
// the all/any/none queries on every host backend; bitmap outputs byte-exact over
// bitmap_bytes(n) including tail zeroing (ADR-016). Input tail bits are deliberately dirty —
// the kernels must mask them, never trust them.
// Module: tests/fuzz | REQs: REQ-TEST-007, REQ-K4-001..003 | ADR-009
#include <cstdint>
#include <cstring>
#include <vector>

#include "quiver/mask.h"
#include "tests/fuzz/fuzz_common.h"

namespace {

constexpr std::int64_t kMaxN = 4096;

void run(quiver_fuzz::Decoder& d) {
  const std::int64_t n = d.len(kMaxN);
  const std::int64_t bytes = (n + 7) / 8;
  const std::vector<std::uint8_t> a = quiver_fuzz::bitmap(d, n);
  const std::vector<std::uint8_t> b = quiver_fuzz::bitmap(d, n);
  const auto op = static_cast<quiver::MaskOp>(d.pick(4));
  const quiver::BitmapView av{a.data()};
  const quiver::BitmapView bv{b.data()};

  std::vector<std::uint8_t> comb0(static_cast<std::size_t>(bytes) + 1);
  std::vector<std::uint8_t> not0(static_cast<std::size_t>(bytes) + 1);
  std::int64_t pop0 = 0;
  bool all0 = false;
  bool any0 = false;
  bool none0 = false;
  bool first = true;
  for (const quiver::Isa isa : quiver_fuzz::host_backends()) {
    quiver_fuzz::check(quiver::set_isa_override(isa), "override rejected");
    std::vector<std::uint8_t> comb(static_cast<std::size_t>(bytes) + 1);
    std::vector<std::uint8_t> neg(static_cast<std::size_t>(bytes) + 1);
    quiver::mask_combine(op, av, bv, n, comb.data());
    quiver::mask_not(av, n, neg.data());
    const std::int64_t pop = quiver::mask_popcount(av, n);
    const bool all = quiver::mask_all(av, n);
    const bool any = quiver::mask_any(av, n);
    const bool none = quiver::mask_none(av, n);
    if (first) {
      comb0 = comb;
      not0 = neg;
      pop0 = pop;
      all0 = all;
      any0 = any;
      none0 = none;
      first = false;
      continue;
    }
    quiver_fuzz::check(std::memcmp(comb.data(), comb0.data(), static_cast<std::size_t>(bytes)) == 0,
                       "K4 combine mismatch");
    quiver_fuzz::check(std::memcmp(neg.data(), not0.data(), static_cast<std::size_t>(bytes)) == 0,
                       "K4 not mismatch");
    quiver_fuzz::check(pop == pop0, "K4 popcount mismatch");
    quiver_fuzz::check(all == all0 && any == any0 && none == none0, "K4 query mismatch");
  }
  quiver::clear_isa_override();
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  quiver_fuzz::Decoder d(data, size);
  run(d);
  return 0;
}
