// K1 differential fuzz target (REQ-TEST-007): every form × op × type × validity shape,
// decoded contract-valid, executed on every host backend, outputs compared to the scalar
// baseline — bitmaps byte-exact over bitmap_bytes(n) (tail-zeroing included, ADR-016),
// selvecs count-then-defined-prefix (REQ-MEM-008).
// Module: tests/fuzz | REQs: REQ-TEST-007, REQ-K1-001..003 | ADR-009
#include <cstdint>
#include <cstring>
#include <vector>

#include "quiver/compare.h"
#include "tests/fuzz/fuzz_common.h"

namespace {

constexpr std::int64_t kMaxN = 512;

template <class T>
void run(quiver_fuzz::Decoder& d) {
  const std::int64_t n = d.len(kMaxN);
  const std::int64_t bytes = (n + 7) / 8;
  const int form = d.pick(6);
  const auto op = static_cast<quiver::CompareOp>(d.pick(6));
  std::vector<T> a(static_cast<std::size_t>(n) + 1);
  std::vector<T> b(static_cast<std::size_t>(n) + 1);
  d.fill(a.data(), n);
  d.fill(b.data(), n);
  const T comparand = d.value<T>();
  const T lo = d.value<T>();
  const T hi = d.value<T>();
  const std::vector<std::uint8_t> av = quiver_fuzz::bitmap(d, n);
  const std::vector<std::uint8_t> bv = quiver_fuzz::bitmap(d, n);
  const quiver::BitmapView avv{d.boolean() ? av.data() : nullptr};
  const quiver::BitmapView bvv{d.boolean() ? bv.data() : nullptr};
  const quiver::BatchView<T> ina{a.data(), n};
  const quiver::BatchView<T> inb{b.data(), n};

  std::int64_t count0 = 0;
  std::vector<std::uint8_t> bits0(static_cast<std::size_t>(bytes) + 1);
  std::vector<std::uint32_t> idx0(static_cast<std::size_t>(n) + 1);
  bool first = true;
  for (const quiver::Isa isa : quiver_fuzz::host_backends()) {
    quiver_fuzz::check(quiver::set_isa_override(isa), "override rejected");
    std::int64_t count = 0;
    std::vector<std::uint8_t> bits(static_cast<std::size_t>(bytes) + 1);
    std::vector<std::uint32_t> idx(static_cast<std::size_t>(n) + 1);
    switch (form) {
    case 0:
      count = quiver::compare_bitmap(op, ina, comparand, avv, bits.data());
      break;
    case 1:
      count = quiver::compare_bitmap(op, ina, inb, avv, bvv, bits.data());
      break;
    case 2:
      count = quiver::compare_between_bitmap(ina, lo, hi, avv, bits.data());
      break;
    case 3:
      count = quiver::compare_selvec(op, ina, comparand, avv, idx.data());
      break;
    case 4:
      count = quiver::compare_selvec(op, ina, inb, avv, bvv, idx.data());
      break;
    default:
      count = quiver::compare_between_selvec(ina, lo, hi, avv, idx.data());
      break;
    }
    if (first) {
      count0 = count;
      bits0 = bits;
      idx0 = idx;
      first = false;
      continue;
    }
    quiver_fuzz::check(count == count0, "K1 count mismatch");
    if (form < 3) {
      quiver_fuzz::check(std::memcmp(bits.data(), bits0.data(), static_cast<std::size_t>(bytes)) ==
                             0,
                         "K1 bitmap mismatch");
    } else {
      quiver_fuzz::check(
          std::memcmp(idx.data(), idx0.data(), static_cast<std::size_t>(count) * 4) == 0,
          "K1 selvec mismatch");
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
