// Nullable data: validity bitmaps (LSB-first, 1 = valid) flow through compare (K1), combine via
// mask algebra (K4), and gate a reduction (K6). Ordinary consumer settings (REQ-INT-006).
// Module: MOD-EXAMPLES
#include <cstdint>
#include <cstdio>
#include <vector>

#include "quiver/quiver.h"

// --8<-- [start:nullable]
namespace {
// Pack a bool-per-element vector into an LSB-first validity bitmap.
std::vector<std::uint8_t> pack(const std::vector<int>& flags) {
  std::vector<std::uint8_t> bits((flags.size() + 7) / 8);
  for (std::size_t i = 0; i < flags.size(); ++i) {
    if (flags[i]) {
      bits[i >> 3] = static_cast<std::uint8_t>(bits[i >> 3] | (1u << (i & 7)));
    }
  }
  return bits;
}
}  // namespace

int main() {
  const std::vector<std::int32_t> in = {10, 20, 30, 40, 50, 60};
  const auto n = static_cast<std::int64_t>(in.size());
  const auto valid = pack({1, 1, 0, 1, 0, 1});   // elements 2 and 4 are null
  const auto second = pack({1, 0, 1, 1, 1, 1});  // a second validity mask to combine

  // K4: AND the two validity bitmaps -> a lane is live only where both agree.
  std::vector<std::uint8_t> live((static_cast<std::size_t>(n) + 7) / 8);
  quiver::mask_combine(quiver::MaskOp::kAnd, quiver::BitmapView{valid.data()},
                       quiver::BitmapView{second.data()}, n, live.data());

  // K1: elements > 25 among the live lanes (validity ANDs into the predicate).
  std::vector<std::uint8_t> hits((static_cast<std::size_t>(n) + 7) / 8);
  const std::int64_t matched =
      quiver::compare_bitmap(quiver::CompareOp::kGt, quiver::BatchView<std::int32_t>{in.data(), n},
                             25, quiver::BitmapView{live.data()}, hits.data());

  // K6: min/max over the live lanes only.
  const std::int32_t lo = quiver::reduce_min(quiver::BatchView<std::int32_t>{in.data(), n},
                                             quiver::BitmapView{live.data()});
  const std::int32_t hi = quiver::reduce_max(quiver::BatchView<std::int32_t>{in.data(), n},
                                             quiver::BitmapView{live.data()});

  std::printf("live-lane range [%d, %d]; %lld live elements > 25\n", lo, hi,
              static_cast<long long>(matched));
  return 0;
}
// --8<-- [end:nullable]
