// A small kernel pipeline: compare into a selection vector (K1), gather the selected values
// (K5), and reduce them (K6) — the composition the demo layer exercises. Ordinary consumer
// settings (exceptions + RTTI, REQ-INT-006). Module: MOD-EXAMPLES
#include <cstdint>
#include <cstdio>
#include <vector>

#include "quiver/quiver.h"

// --8<-- [start:pipeline]
int main() {
  const std::vector<std::int32_t> in = {5, 1, 9, 3, 7, 2, 8, 4, 6, 0};
  const auto n = static_cast<std::int64_t>(in.size());

  // K1: indices of elements >= 5, as a (sorted, in-range) selection vector.
  std::vector<std::uint32_t> sel(static_cast<std::size_t>(n));
  const std::int64_t k =
      quiver::compare_selvec(quiver::CompareOp::kGe, quiver::BatchView<std::int32_t>{in.data(), n},
                             5, quiver::BitmapView{nullptr}, sel.data());

  // K5: gather those elements.
  std::vector<std::int32_t> picked(static_cast<std::size_t>(k));
  quiver::take(quiver::BatchView<std::int32_t>{in.data(), n}, quiver::SelVec{sel.data(), k},
               picked.data());

  // K6: sum them (wrapping into the wider accumulator).
  const auto sum = quiver::reduce_sum_wrap(quiver::BatchView<std::int32_t>{picked.data(), k},
                                           quiver::BitmapView{nullptr});

  std::printf("selected %lld elements >= 5; sum = %lld\n", static_cast<long long>(k),
              static_cast<long long>(sum));
  return 0;
}
// --8<-- [end:pipeline]
