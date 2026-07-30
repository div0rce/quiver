// MOD-KCOMMON compaction LUTs: runtime re-derivation vs the consteval-built constants
// (REQ-SIMD-005 / PRD 05 §6 invariant — generator and table must agree). The re-derivation
// below is written independently and naively on purpose; a mismatch means the consteval
// builder or the table shape changed without its spec.
// Covers: REQ-SIMD-005
#include <cstdint>

#include <gtest/gtest.h>

#include "src/kernels/common/luts.h"

namespace {

using namespace quiver::detail;

// Independent transcription of the compaction order. Derived from the DEFINITION by rank — a
// selected lane lands after the selected lanes before it, an unselected one after all selected
// lanes plus the unselected ones before it — deliberately NOT by replaying the production
// builder's two passes, so this stays a real oracle for it (the dual-oracle discipline).
void expected_compaction(std::uint32_t* out, int mask, int lanes) {
  int selected = 0;
  for (int lane = 0; lane < lanes; ++lane) {
    selected += (mask >> lane) & 1;
  }
  int taken = 0;
  int dropped = 0;
  for (int lane = 0; lane < lanes; ++lane) {
    const bool on = ((mask >> lane) & 1) != 0;
    const int pos = on ? taken++ : selected + dropped++;
    out[pos] = static_cast<std::uint32_t>(lane);
  }
}

TEST(Luts, CompactLut32Rederives) {
  for (int b = 0; b < 256; ++b) {
    std::uint32_t expect[8];
    expected_compaction(expect, b, 8);
    for (int k = 0; k < 8; ++k) {
      ASSERT_EQ(kCompactLut32.perm[b][k], expect[k]) << "byte=" << b << " lane " << k;
    }
  }
}

TEST(Luts, CompactLut64Rederives) {
  // The table stores the epi32 pair indices {2p, 2p+1} that vpermd consumes, not the raw
  // 64-bit lane indices p — re-derive both steps independently (luts.h).
  for (int b = 0; b < 16; ++b) {
    std::uint32_t lanes[4];
    expected_compaction(lanes, b, 4);
    for (int k = 0; k < 4; ++k) {
      ASSERT_EQ(kCompactLut64.perm[b][2 * k], 2 * lanes[k]) << "nibble=" << b << " k=" << k;
      ASSERT_EQ(kCompactLut64.perm[b][2 * k + 1], 2 * lanes[k] + 1) << "nibble=" << b << " k=" << k;
    }
  }
}

TEST(Luts, CompactLut64RowsAre32ByteAligned) {
  // compact4_64bit loads rows with _mm256_load_si256 (aligned); a misaligned row would fault.
  for (int b = 0; b < 16; ++b) {
    const auto addr = reinterpret_cast<std::uintptr_t>(&kCompactLut64.perm[b][0]);
    ASSERT_EQ(addr % 32u, 0u) << "row " << b << " is not 32-byte aligned";
  }
}

TEST(Luts, PopcountLutRederives) {
  for (int b = 0; b < 256; ++b) {
    int c = 0;
    for (int bit = 0; bit < 8; ++bit) {
      c += (b >> bit) & 1;
    }
    ASSERT_EQ(kPopcountLut.count[b], c) << "byte=" << b;
  }
}

// Generic check for the TBL control tables: selected lanes' byte indices front-packed,
// 0xFF elsewhere. Bounds are template parameters so every subscript is provably in-range
// (gcc -Warray-bounds cannot see through runtime bounds when the assert machinery keeps
// this from inlining).
template <int kLaneBytes, class Row>
void check_lane_bytes(const Row& ctrl, int at, int lane, int s) {
  for (int byte = 0; byte < kLaneBytes; ++byte) {
    ASSERT_EQ(ctrl[at + byte], lane * kLaneBytes + byte) << "s=" << s << " pos=" << (at + byte);
  }
}

template <int kSelBits, int kLaneBytes, int kCtrlBytes, class Row>
void check_tbl_row(const Row& ctrl, int s) {
  int c = 0;
  for (int lane = 0; lane < kSelBits; ++lane) {
    if ((s >> lane) & 1) {
      check_lane_bytes<kLaneBytes>(ctrl, c, lane, s);
      c += kLaneBytes;
    }
  }
  for (; c < kCtrlBytes; ++c) {
    ASSERT_EQ(ctrl[c], 0xFF) << "s=" << s << " fill pos=" << c;
  }
}

template <int kSelBits, int kLaneBytes, int kCtrlBytes, class Lut>
void check_tbl_ctrl(const Lut& lut) {
  for (int s = 0; s < (1 << kSelBits); ++s) {
    check_tbl_row<kSelBits, kLaneBytes, kCtrlBytes>(lut.ctrl[s], s);
  }
}

TEST(Luts, NeonTblControlsRederive) {
  check_tbl_ctrl<4, 1, 8>(kCompactNib8);
  check_tbl_ctrl<4, 2, 8>(kCompactNib16);
  check_tbl_ctrl<4, 4, 16>(kCompactNib32);
  check_tbl_ctrl<2, 8, 16>(kCompactPair64);
}

}  // namespace
