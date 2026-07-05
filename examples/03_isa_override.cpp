// Runtime dispatch: inspect the selected ISA tier, force a specific tier, and confirm results
// are identical across tiers. Ordinary consumer settings (REQ-INT-006). Module: MOD-EXAMPLES
#include <cstdint>
#include <cstdio>
#include <vector>

#include "quiver/quiver.h"

namespace {
const char* isa_name(quiver::Isa isa) {
  switch (isa) {
  case quiver::Isa::kScalar:
    return "scalar";
  case quiver::Isa::kNeon:
    return "neon";
  case quiver::Isa::kAvx2:
    return "avx2";
  case quiver::Isa::kAvx512:
    return "avx512";
  }
  return "?";
}

std::int64_t count_gt_zero(const std::vector<std::int32_t>& in) {
  std::vector<std::uint8_t> bits((in.size() + 7) / 8);
  return quiver::compare_bitmap(
      quiver::CompareOp::kGt,
      quiver::BatchView<std::int32_t>{in.data(), static_cast<std::int64_t>(in.size())}, 0,
      quiver::BitmapView{nullptr}, bits.data());
}
}  // namespace

int main() {
  const std::vector<std::int32_t> in = {-2, 3, 0, 5, -1, 8};
  std::printf("default active ISA: %s\n", isa_name(quiver::active_isa()));

  const std::int64_t base = count_gt_zero(in);

  // Force the scalar tier (always accepted) and confirm the result is unchanged.
  quiver::set_isa_override(quiver::Isa::kScalar);
  std::printf("forced active ISA: %s\n", isa_name(quiver::active_isa()));
  const std::int64_t scalar_count = count_gt_zero(in);
  quiver::clear_isa_override();

  std::printf("positives: %lld (scalar tier agrees: %s)\n", static_cast<long long>(base),
              base == scalar_count ? "yes" : "NO");
  return base == scalar_count ? 0 : 1;
}
