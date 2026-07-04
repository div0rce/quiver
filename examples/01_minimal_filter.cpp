// Minimal Quiver use: compare a batch into a selection bitmap (K1), then compact the selected
// elements densely (K2). Builds as an ordinary consumer — exceptions and RTTI on (REQ-INT-006).
// Module: MOD-EXAMPLES
#include <cstdint>
#include <cstdio>
#include <vector>

#include "quiver/quiver.h"

int main() {
  const std::vector<std::int32_t> in = {5, 1, 9, 3, 7, 2, 8, 4, 6, 0};
  const auto n = static_cast<std::int64_t>(in.size());
  std::vector<std::uint8_t> selected((static_cast<std::size_t>(n) + 7) / 8);

  // K1: which elements are > 4? (null validity = all valid)
  const std::int64_t hits =
      quiver::compare_bitmap(quiver::CompareOp::kGt, quiver::BatchView<std::int32_t>{in.data(), n},
                             4, quiver::BitmapView{nullptr}, selected.data());

  // K2: compact those elements to the front of `out`.
  std::vector<std::int32_t> out(static_cast<std::size_t>(n));
  const std::int64_t kept = quiver::filter(quiver::BatchView<std::int32_t>{in.data(), n},
                                           quiver::BitmapView{selected.data()}, out.data());

  std::printf("%lld elements > 4 (bitmap popcount %lld):", static_cast<long long>(kept),
              static_cast<long long>(hits));
  for (std::int64_t i = 0; i < kept; ++i) {
    std::printf(" %d", out[static_cast<std::size_t>(i)]);
  }
  std::printf("\n");
  return 0;
}
