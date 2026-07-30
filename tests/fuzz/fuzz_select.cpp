// K3 differential fuzz target (REQ-TEST-007): bitmap→selvec (count-then-defined-prefix,
// REQ-MEM-008) and selvec→bitmap (fully defined, tail-zeroed, ADR-016) on every host
// backend against the scalar baseline.
// Module: tests/fuzz | REQs: REQ-TEST-007, REQ-K3-001..002 | ADR-009
#include <cstdint>
#include <cstring>
#include <vector>

#include "quiver/select.h"
#include "tests/fuzz/fuzz_common.h"

namespace {

constexpr std::int64_t kMaxN = 4096;  // wide range: this family is all about bit geometry

// The first backend's results; every later backend must reproduce them exactly.
struct SelectRef {
  std::int64_t count;
  std::vector<std::uint32_t> idx;
  std::vector<std::uint8_t> bits;
  bool first;
};

void check_bitmap_to_selvec(const std::vector<std::uint8_t>& selection, std::int64_t n,
                            SelectRef& ref) {
  std::vector<std::uint32_t> idx(static_cast<std::size_t>(n) + 1);
  const std::int64_t count =
      quiver::bitmap_to_selvec(quiver::BitmapView{selection.data()}, n, idx.data());
  if (ref.first) {
    ref.count = count;
    ref.idx = idx;
    return;
  }
  quiver_fuzz::check(count == ref.count, "K3 count mismatch");
  quiver_fuzz::check(std::memcmp(idx.data(), ref.idx.data(), static_cast<std::size_t>(count) * 4) ==
                         0,
                     "K3 selvec mismatch");
}

void check_selvec_to_bitmap(const std::vector<std::uint32_t>& sel, std::int64_t n,
                            std::int64_t bytes, SelectRef& ref) {
  std::vector<std::uint8_t> bits(static_cast<std::size_t>(bytes) + 1);
  quiver::selvec_to_bitmap(quiver::SelVec{sel.data(), static_cast<std::int64_t>(sel.size())}, n,
                           bits.data());
  if (ref.first) {
    ref.bits = bits;
    return;
  }
  quiver_fuzz::check(std::memcmp(bits.data(), ref.bits.data(), static_cast<std::size_t>(bytes)) ==
                         0,
                     "K3 bitmap mismatch");
}

void run(quiver_fuzz::Decoder& d) {
  const std::int64_t n = d.len(kMaxN);
  const std::int64_t bytes = (n + 7) / 8;
  const std::vector<std::uint8_t> selection = quiver_fuzz::bitmap(d, n);
  const std::vector<std::uint32_t> sel = quiver_fuzz::selvec(d, n);
  const bool to_selvec = d.boolean();

  SelectRef ref{0, std::vector<std::uint32_t>(static_cast<std::size_t>(n) + 1),
                std::vector<std::uint8_t>(static_cast<std::size_t>(bytes) + 1), true};
  for (const quiver::Isa isa : quiver_fuzz::host_backends()) {
    quiver_fuzz::check(quiver::set_isa_override(isa), "override rejected");
    if (to_selvec) {
      check_bitmap_to_selvec(selection, n, ref);
    } else {
      check_selvec_to_bitmap(sel, n, bytes, ref);
    }
    ref.first = false;
  }
  quiver::clear_isa_override();
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  quiver_fuzz::Decoder d(data, size);
  run(d);
  return 0;
}
