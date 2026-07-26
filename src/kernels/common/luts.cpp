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
    int lanes[4] = {};
    int cursor = 0;
    for (int lane = 0; lane < 4; ++lane) {
      if ((b >> lane) & 1) {
        lanes[cursor++] = lane;
      }
    }
    for (int lane = 0; lane < 4; ++lane) {
      if (((b >> lane) & 1) == 0) {
        lanes[cursor++] = lane;
      }
    }
    // Expand each 64-bit lane index p into the epi32 pair {2p, 2p+1} for vpermd.
    for (int k = 0; k < 4; ++k) {
      lut.perm[b][2 * k] = static_cast<std::uint32_t>(2 * lanes[k]);
      lut.perm[b][2 * k + 1] = static_cast<std::uint32_t>(2 * lanes[k] + 1);
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

// Generic nibble/pair TBL-control builder: for each selection value, byte indices of the
// selected lanes front-packed, 0xFF elsewhere (TBL zero-fill; REQ-MEM-008 scratch).
template <class Lut, int kSelBits, int kLaneBytes, int kCtrlBytes>
consteval Lut build_tbl_ctrl() {
  Lut lut{};
  for (int s = 0; s < (1 << kSelBits); ++s) {
    int cursor = 0;
    for (int lane = 0; lane < kSelBits; ++lane) {
      if ((s >> lane) & 1) {
        for (int byte = 0; byte < kLaneBytes; ++byte) {
          lut.ctrl[s][cursor++] = static_cast<std::uint8_t>(lane * kLaneBytes + byte);
        }
      }
    }
    for (; cursor < kCtrlBytes; ++cursor) {
      lut.ctrl[s][cursor] = 0xFF;
    }
  }
  return lut;
}

}  // namespace

constinit const CompactLut32 kCompactLut32 = build_lut32();
constinit const CompactLut64 kCompactLut64 = build_lut64();
constinit const PopcountLut kPopcountLut = build_popcount();
constinit const CompactNib8 kCompactNib8 = build_tbl_ctrl<CompactNib8, 4, 1, 8>();
constinit const CompactNib16 kCompactNib16 = build_tbl_ctrl<CompactNib16, 4, 2, 8>();
constinit const CompactNib32 kCompactNib32 = build_tbl_ctrl<CompactNib32, 4, 4, 16>();
constinit const CompactPair64 kCompactPair64 = build_tbl_ctrl<CompactPair64, 2, 8, 16>();

}  // namespace detail
QUIVER_END_NAMESPACE
