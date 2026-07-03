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

TEST(Luts, CompactLut32Rederives) {
  for (int b = 0; b < 256; ++b) {
    std::uint32_t expect[8];
    int c = 0;
    for (std::uint32_t lane = 0; lane < 8; ++lane) {
      if ((b >> lane) & 1) {
        expect[c++] = lane;
      }
    }
    for (std::uint32_t lane = 0; lane < 8; ++lane) {
      if (((b >> lane) & 1) == 0) {
        expect[c++] = lane;
      }
    }
    for (int k = 0; k < 8; ++k) {
      ASSERT_EQ(kCompactLut32.perm[b][k], expect[k]) << "byte=" << b << " lane " << k;
    }
  }
}

TEST(Luts, CompactLut64Rederives) {
  for (int b = 0; b < 16; ++b) {
    std::uint64_t expect[4];
    int c = 0;
    for (std::uint64_t lane = 0; lane < 4; ++lane) {
      if ((b >> lane) & 1) {
        expect[c++] = lane;
      }
    }
    for (std::uint64_t lane = 0; lane < 4; ++lane) {
      if (((b >> lane) & 1) == 0) {
        expect[c++] = lane;
      }
    }
    for (int k = 0; k < 4; ++k) {
      ASSERT_EQ(kCompactLut64.perm[b][k], expect[k]) << "nibble=" << b;
    }
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
template <int kSelBits, int kLaneBytes, int kCtrlBytes, class Lut>
void check_tbl_ctrl(const Lut& lut) {
  for (int s = 0; s < (1 << kSelBits); ++s) {
    int c = 0;
    for (int lane = 0; lane < kSelBits; ++lane) {
      if ((s >> lane) & 1) {
        for (int byte = 0; byte < kLaneBytes; ++byte) {
          ASSERT_EQ(lut.ctrl[s][c], lane * kLaneBytes + byte) << "s=" << s << " pos=" << c;
          ++c;
        }
      }
    }
    for (; c < kCtrlBytes; ++c) {
      ASSERT_EQ(lut.ctrl[s][c], 0xFF) << "s=" << s << " fill pos=" << c;
    }
  }
}

TEST(Luts, NeonTblControlsRederive) {
  check_tbl_ctrl<4, 1, 8>(kCompactNib8);
  check_tbl_ctrl<4, 2, 8>(kCompactNib16);
  check_tbl_ctrl<4, 4, 16>(kCompactNib32);
  check_tbl_ctrl<2, 8, 16>(kCompactPair64);
}

}  // namespace
