// consteval construction of the shared compaction LUTs — no code-generation build step
// (ADR-003 consequence); tests re-derive these tables at runtime and compare (PRD 05 §6).
// Module: MOD-KCOMMON | REQs: REQ-SIMD-005, REQ-CORE-004 (constinit, no dynamic init)
#include "src/kernels/common/luts.h"

QUIVER_BEGIN_NAMESPACE
namespace detail {

namespace {

consteval CompactLut32 build_lut32() {
  CompactLut32 lut{};
  for (int b = 0; b < 256; ++b) {
    int cursor = 0;
    for (int lane = 0; lane < 8; ++lane) {
      if ((b >> lane) & 1) {
        lut.perm[b][cursor++] = static_cast<std::uint32_t>(lane);
      }
    }
    for (int lane = 0; lane < 8; ++lane) {
      if (((b >> lane) & 1) == 0) {
        lut.perm[b][cursor++] = static_cast<std::uint32_t>(lane);
      }
    }
  }
  return lut;
}

consteval CompactLut64 build_lut64() {
  CompactLut64 lut{};
  for (int b = 0; b < 16; ++b) {
    int cursor = 0;
    for (int lane = 0; lane < 4; ++lane) {
      if ((b >> lane) & 1) {
        lut.perm[b][cursor++] = static_cast<std::uint64_t>(lane);
      }
    }
    for (int lane = 0; lane < 4; ++lane) {
      if (((b >> lane) & 1) == 0) {
        lut.perm[b][cursor++] = static_cast<std::uint64_t>(lane);
      }
    }
  }
  return lut;
}

consteval PopcountLut build_popcount() {
  PopcountLut lut{};
  for (int b = 0; b < 256; ++b) {
    int c = 0;
    for (int bit = 0; bit < 8; ++bit) {
      c += (b >> bit) & 1;
    }
    lut.count[b] = static_cast<std::uint8_t>(c);
  }
  return lut;
}

}  // namespace

constinit const CompactLut32 kCompactLut32 = build_lut32();
constinit const CompactLut64 kCompactLut64 = build_lut64();
constinit const PopcountLut kPopcountLut = build_popcount();

}  // namespace detail
QUIVER_END_NAMESPACE
