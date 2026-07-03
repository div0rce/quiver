// K5 differential fuzz target (REQ-TEST-007): take (arbitrary order + duplicates, indices
// in bounds by construction, ADR-025) and dict_decode (plain and fused-selected forms,
// codes in bounds by construction) across value × code types on every host backend;
// outputs fully defined, byte-exact.
// Module: tests/fuzz | REQs: REQ-TEST-007, REQ-K5-001..003 | ADR-009, ADR-025
#include <cstdint>
#include <cstring>
#include <vector>

#include "quiver/take.h"
#include "tests/fuzz/fuzz_common.h"

namespace {

constexpr std::int64_t kMaxN = 512;
constexpr std::int64_t kMaxDict = 200;  // fits every code type incl. uint8_t

template <class T, class C>
void run_decode(quiver_fuzz::Decoder& d, const std::vector<T>& dict) {
  const std::int64_t dict_len = static_cast<std::int64_t>(dict.size());
  const std::int64_t n = d.len(kMaxN);
  std::vector<C> codes(static_cast<std::size_t>(n) + 1);
  for (std::int64_t i = 0; i < n; ++i) {
    codes[static_cast<std::size_t>(i)] =
        static_cast<C>(d.u32() % static_cast<std::uint32_t>(dict_len));
  }
  const std::vector<std::uint32_t> sel = quiver_fuzz::selvec(d, n);
  const bool fused = d.boolean();
  const std::int64_t out_len = fused ? static_cast<std::int64_t>(sel.size()) : n;

  std::vector<T> out0(static_cast<std::size_t>(out_len) + 1);
  bool first = true;
  for (const quiver::Isa isa : quiver_fuzz::host_backends()) {
    quiver_fuzz::check(quiver::set_isa_override(isa), "override rejected");
    std::vector<T> out(static_cast<std::size_t>(out_len) + 1);
    if (fused) {
      quiver::dict_decode(quiver::BatchView<T>{dict.data(), dict_len}, codes.data(), n,
                          quiver::SelVec{sel.data(), static_cast<std::int64_t>(sel.size())},
                          out.data());
    } else {
      quiver::dict_decode(quiver::BatchView<T>{dict.data(), dict_len}, codes.data(), n, out.data());
    }
    if (first) {
      out0 = out;
      first = false;
      continue;
    }
    quiver_fuzz::check(
        std::memcmp(out.data(), out0.data(), static_cast<std::size_t>(out_len) * sizeof(T)) == 0,
        "K5 dict_decode mismatch");
  }
}

template <class T>
void run(quiver_fuzz::Decoder& d) {
  const std::int64_t values_len = d.len(kMaxDict - 1) + 1;  // >= 1 so indices can be valid
  std::vector<T> values(static_cast<std::size_t>(values_len));
  d.fill(values.data(), values_len);

  if (d.boolean()) {  // --- take ---
    const std::int64_t idx_len = d.len(kMaxN);
    std::vector<std::uint32_t> idx(static_cast<std::size_t>(idx_len) + 1);
    for (std::int64_t j = 0; j < idx_len; ++j) {
      idx[static_cast<std::size_t>(j)] = d.u32() % static_cast<std::uint32_t>(values_len);
    }
    std::vector<T> out0(static_cast<std::size_t>(idx_len) + 1);
    bool first = true;
    for (const quiver::Isa isa : quiver_fuzz::host_backends()) {
      quiver_fuzz::check(quiver::set_isa_override(isa), "override rejected");
      std::vector<T> out(static_cast<std::size_t>(idx_len) + 1);
      quiver::take(quiver::BatchView<T>{values.data(), values_len},
                   quiver::SelVec{idx.data(), idx_len}, out.data());
      if (first) {
        out0 = out;
        first = false;
        continue;
      }
      quiver_fuzz::check(
          std::memcmp(out.data(), out0.data(), static_cast<std::size_t>(idx_len) * sizeof(T)) == 0,
          "K5 take mismatch");
    }
  } else {  // --- dict_decode, code type from the stream ---
    switch (d.pick(3)) {
    case 0:
      run_decode<T, std::uint8_t>(d, values);
      break;
    case 1:
      run_decode<T, std::uint16_t>(d, values);
      break;
    default:
      run_decode<T, std::uint32_t>(d, values);
      break;
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
