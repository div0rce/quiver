// consteval construction of the shared compaction LUTs — no code-generation build step
// (ADR-003 consequence); tests re-derive these tables at runtime and compare (PRD 05 §6).
// Module: MOD-KCOMMON | REQs: REQ-SIMD-005, REQ-CORE-004 (constinit, no dynamic init)
#include "src/kernels/common/luts.h"

#include <cstddef>

QUIVER_BEGIN_NAMESPACE
namespace detail {

namespace {

// The compaction permutation for one selection mask: the selected lane indices in order,
// followed by the unselected ones in order. Both compaction LUTs are built from it.
consteval void compaction_order(std::uint32_t* out, int mask, int lanes) {
  int cursor = 0;
  for (int want = 1; want >= 0; --want) {
    for (int lane = 0; lane < lanes; ++lane) {
      if (((mask >> lane) & 1) == want) {
        out[cursor++] = static_cast<std::uint32_t>(lane);
      }
    }
  }
}

consteval CompactLut32 build_lut32() {
  CompactLut32 lut{};
  for (int b = 0; b < 256; ++b) {
    compaction_order(lut.perm[b], b, 8);
  }
  return lut;
}

consteval CompactLut64 build_lut64() {
  CompactLut64 lut{};
  for (int b = 0; b < 16; ++b) {
    std::uint32_t lanes[4] = {};
    compaction_order(lanes, b, 4);
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

// The byte indices of one selected lane, appended at `cursor`.
consteval int append_lane_bytes(std::uint8_t* ctrl, int cursor, int lane, int lane_bytes) {
  for (int byte = 0; byte < lane_bytes; ++byte) {
    ctrl[cursor++] = static_cast<std::uint8_t>(lane * lane_bytes + byte);
  }
  return cursor;
}

// One selection value's control row: byte indices of the selected lanes front-packed, 0xFF
// elsewhere (TBL zero-fill; REQ-MEM-008 scratch).
template <int kSelBits, int kLaneBytes, int kCtrlBytes>
consteval void fill_tbl_row(std::uint8_t* ctrl, int s) {
  int cursor = 0;
  for (int lane = 0; lane < kSelBits; ++lane) {
    if ((s >> lane) & 1) {
      cursor = append_lane_bytes(ctrl, cursor, lane, kLaneBytes);
    }
  }
  for (; cursor < kCtrlBytes; ++cursor) {
    ctrl[cursor] = 0xFF;
  }
}

// Generic nibble/pair TBL-control builder.
template <class Lut, int kSelBits, int kLaneBytes, int kCtrlBytes>
consteval Lut build_tbl_ctrl() {
  Lut lut{};
  for (int s = 0; s < (1 << kSelBits); ++s) {
    fill_tbl_row<kSelBits, kLaneBytes, kCtrlBytes>(lut.ctrl[s], s);
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
