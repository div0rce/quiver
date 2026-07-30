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

// Everything the six K1 forms draw from the stream; which form runs is itself an input.
template <class T>
struct CompareInputs {
  quiver::BatchView<T> a;
  quiver::BatchView<T> b;
  quiver::BitmapView av;
  quiver::BitmapView bv;
  T comparand;
  T lo;
  T hi;
  quiver::CompareOp op;
  int form;
};

// What one backend produced. Bitmap forms fill bits, selvec forms fill idx.
struct CompareOutputs {
  std::int64_t count;
  std::vector<std::uint8_t> bits;
  std::vector<std::uint32_t> idx;
};

template <class T>
std::int64_t run_form(const CompareInputs<T>& in, CompareOutputs& out) {
  switch (in.form) {
  case 0:
    return quiver::compare_bitmap(in.op, in.a, in.comparand, in.av, out.bits.data());
  case 1:
    return quiver::compare_bitmap(in.op, in.a, in.b, in.av, in.bv, out.bits.data());
  case 2:
    return quiver::compare_between_bitmap(in.a, in.lo, in.hi, in.av, out.bits.data());
  case 3:
    return quiver::compare_selvec(in.op, in.a, in.comparand, in.av, out.idx.data());
  case 4:
    return quiver::compare_selvec(in.op, in.a, in.b, in.av, in.bv, out.idx.data());
  default:
    return quiver::compare_between_selvec(in.a, in.lo, in.hi, in.av, out.idx.data());
  }
}

// Later backends must reproduce the first backend's outputs exactly.
inline void expect_same(const CompareOutputs& got, const CompareOutputs& want, int form,
                        std::int64_t bytes) {
  quiver_fuzz::check(got.count == want.count, "K1 count mismatch");
  if (form < 3) {
    quiver_fuzz::check(
        std::memcmp(got.bits.data(), want.bits.data(), static_cast<std::size_t>(bytes)) == 0,
        "K1 bitmap mismatch");
    return;
  }
  quiver_fuzz::check(
      std::memcmp(got.idx.data(), want.idx.data(), static_cast<std::size_t>(got.count) * 4) == 0,
      "K1 selvec mismatch");
}

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
  const CompareInputs<T> in{quiver::BatchView<T>{a.data(), n},
                            quiver::BatchView<T>{b.data(), n},
                            avv,
                            bvv,
                            comparand,
                            lo,
                            hi,
                            op,
                            form};

  CompareOutputs ref{0, std::vector<std::uint8_t>(static_cast<std::size_t>(bytes) + 1),
                     std::vector<std::uint32_t>(static_cast<std::size_t>(n) + 1)};
  bool first = true;
  for (const quiver::Isa isa : quiver_fuzz::host_backends()) {
    quiver_fuzz::check(quiver::set_isa_override(isa), "override rejected");
    CompareOutputs got{0, std::vector<std::uint8_t>(static_cast<std::size_t>(bytes) + 1),
                       std::vector<std::uint32_t>(static_cast<std::size_t>(n) + 1)};
    got.count = run_form(in, got);
    if (first) {
      ref = got;
      first = false;
      continue;
    }
    expect_same(got, ref, form, bytes);
  }
  quiver::clear_isa_override();
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  quiver_fuzz::Decoder d(data, size);
  quiver_fuzz::with_element_type(d, [&]<class T>(T) { run<T>(d); });
  return 0;
}
