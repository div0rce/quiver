// consteval construction of the shared compaction LUTs — no code-generation build step
// (ADR-003 consequence); tests re-derive these tables at runtime and compare (PRD 05 §6).
// Module: MOD-KCOMMON | REQs: REQ-SIMD-005, REQ-CORE-004 (constinit, no dynamic init)
#include "src/kernels/common/luts.h"

#include <cstddef>

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
    std::uint32_t lanes[4] = {};
    std::size_t cursor = 0;
    for (std::uint32_t lane = 0; lane < 4; ++lane) {
      if ((static_cast<std::uint32_t>(b) >> lane) & 1u) {
        lanes[cursor++] = lane;
      }
    }
    for (std::uint32_t lane = 0; lane < 4; ++lane) {
      if (((static_cast<std::uint32_t>(b) >> lane) & 1u) == 0u) {
        lanes[cursor++] = lane;
      }
    }
    // Expand each 64-bit lane index p into the epi32 pair {2p, 2p+1} for vpermd.
    // k is std::size_t so the 2*k subscripts are computed in the index type directly —
    // an int multiply widened to ptrdiff_t at the subscript trips
    // bugprone-implicit-widening-of-multiplication-result (REQ-STD-007).
    for (std::size_t k = 0; k < 4; ++k) {
      lut.perm[b][2 * k] = 2u * lanes[k];
      lut.perm[b][2 * k + 1] = 2u * lanes[k] + 1u;
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
