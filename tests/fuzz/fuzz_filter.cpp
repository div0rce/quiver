// K2 differential fuzz target (REQ-TEST-007): bitmap- and selvec-driven filtering across
// all element types, out-of-place AND exact-alias in-place (out == in, ADR-023), executed
// on every host backend; outputs compared count-then-defined-prefix (REQ-MEM-008 — the
// scratch region past the cursor is intentionally backend-specific).
// Module: tests/fuzz | REQs: REQ-TEST-007, REQ-K2-001..003 | ADR-009, ADR-023
#include <cstdint>
#include <cstring>
#include <vector>

#include "quiver/filter.h"
#include "tests/fuzz/fuzz_common.h"

namespace {

constexpr std::int64_t kMaxN = 512;

template <class T>
void run(quiver_fuzz::Decoder& d) {
  const std::int64_t n = d.len(kMaxN);
  std::vector<T> in(static_cast<std::size_t>(n) + 1);
  d.fill(in.data(), n);
  const std::vector<std::uint8_t> selection = quiver_fuzz::bitmap(d, n);
  const std::vector<std::uint32_t> sel = quiver_fuzz::selvec(d, n);
  const bool use_selvec = d.boolean();
  const bool in_place = d.boolean();

  std::int64_t count0 = 0;
  std::vector<T> out0;
  bool first = true;
  for (const quiver::Isa isa : quiver_fuzz::host_backends()) {
    quiver_fuzz::check(quiver::set_isa_override(isa), "override rejected");
    // In-place runs mutate their own copy of the input (exact alias, out == in).
    std::vector<T> buf = in;
    std::vector<T> out(static_cast<std::size_t>(n) + 1);
    T* dst = in_place ? buf.data() : out.data();
    std::int64_t count = 0;
    if (use_selvec) {
      count =
          quiver::filter(quiver::BatchView<T>{buf.data(), n},
                         quiver::SelVec{sel.data(), static_cast<std::int64_t>(sel.size())}, dst);
    } else {
      count = quiver::filter(quiver::BatchView<T>{buf.data(), n},
                             quiver::BitmapView{selection.data()}, dst);
    }
    if (first) {
      count0 = count;
      out0.assign(dst, dst + count);
      first = false;
      continue;
    }
    quiver_fuzz::check(count == count0, "K2 count mismatch");
    quiver_fuzz::check(count == 0 || std::memcmp(dst, out0.data(),
                                                 static_cast<std::size_t>(count) * sizeof(T)) == 0,
                       "K2 defined-output mismatch");
  }
  quiver::clear_isa_override();
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  quiver_fuzz::Decoder d(data, size);
  quiver_fuzz::with_element_type(d, [&]<class T>(T) { run<T>(d); });
  return 0;
}
