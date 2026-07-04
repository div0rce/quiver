// K8 differential fuzz target — the RAW-BYTE harness (REQ-SEC-004): `packed` is untrusted
// by contract, so the fuzz bytes are fed DIRECTLY as the packed stream (no structured
// decoding of the payload); only n and bit_width are decoded, and n is clamped so the
// stream stays within the exact ceil(n*w/8) byte bound the kernel may read. Every host
// backend must agree bit-for-bit, and the scalar tier is checked against the independent
// ADR-026 oracle. K8 carries the largest fuzz budget (REQ-TEST-007).
// Module: tests/fuzz | REQs: REQ-TEST-007, REQ-SEC-004, REQ-K8-001..004 | ADR-009, ADR-026
#include <cstdint>
#include <cstring>
#include <vector>

#include "quiver/unpack.h"
#include "tests/fuzz/fuzz_common.h"
#include "tests/testkit/reference.h"

namespace {

namespace ref = quiver_test::ref;

template <class Out>
void run(quiver_fuzz::Decoder& d, const std::uint8_t* raw, std::size_t raw_size) {
  const int max_w = 8 * static_cast<int>(sizeof(Out));
  const int w = d.pick(max_w + 1);  // 0..max_w inclusive
  const auto base = d.value<Out>();
  // The REMAINING fuzz bytes are the packed stream, raw. Clamp n to the byte budget.
  const std::size_t consumed = raw_size - d.remaining();
  const std::uint8_t* packed = raw + consumed;
  const std::size_t avail = d.remaining();
  std::int64_t n;
  if (w == 0) {
    n = static_cast<std::int64_t>(avail % 300);  // packed unused (may be null by contract)
    packed = nullptr;
  } else {
    n = static_cast<std::int64_t>((avail * 8) / static_cast<std::size_t>(w));
    if (n > 2048) {
      n = 2048;
    }
  }
  std::vector<Out> first(static_cast<std::size_t>(n) + 1);
  bool have_first = false;
  for (const quiver::Isa isa : quiver_fuzz::host_backends()) {
    quiver_fuzz::check(quiver::set_isa_override(isa), "override rejected");
    std::vector<Out> got(static_cast<std::size_t>(n) + 1);
    quiver::unpack_for(packed, n, w, base, got.data());
    if (!have_first) {
      first = got;
      have_first = true;
      for (std::int64_t i = 0; i < n; ++i) {  // scalar tier vs the independent oracle
        const Out want = static_cast<Out>(
            base + (w == 0 ? Out{0} : ref::unpack_value_expected<Out>(packed, i, w)));
        quiver_fuzz::check(got[static_cast<std::size_t>(i)] == want, "K8 scalar vs oracle");
      }
      continue;
    }
    quiver_fuzz::check(
        std::memcmp(got.data(), first.data(), static_cast<std::size_t>(n) * sizeof(Out)) == 0,
        "K8 cross-backend mismatch");
  }
  quiver::clear_isa_override();
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  quiver_fuzz::Decoder d(data, size);
  switch (d.pick(4)) {
  case 0:
    run<std::uint8_t>(d, data, size);
    break;
  case 1:
    run<std::uint16_t>(d, data, size);
    break;
  case 2:
    run<std::uint32_t>(d, data, size);
    break;
  default:
    run<std::uint64_t>(d, data, size);
    break;
  }
  return 0;
}
