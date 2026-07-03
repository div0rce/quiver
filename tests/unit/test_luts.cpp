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
// 0xFF elsewhere.
template <class Lut>
void check_tbl_ctrl(const Lut& lut, int sel_bits, int lane_bytes, int ctrl_bytes) {
  for (int s = 0; s < (1 << sel_bits); ++s) {
    int c = 0;
    for (int lane = 0; lane < sel_bits; ++lane) {
      if ((s >> lane) & 1) {
        for (int byte = 0; byte < lane_bytes; ++byte) {
          ASSERT_EQ(lut.ctrl[s][c], lane * lane_bytes + byte) << "s=" << s << " pos=" << c;
          ++c;
        }
      }
    }
    for (; c < ctrl_bytes; ++c) {
      ASSERT_EQ(lut.ctrl[s][c], 0xFF) << "s=" << s << " fill pos=" << c;
    }
  }
}

TEST(Luts, NeonTblControlsRederive) {
  check_tbl_ctrl(kCompactNib8, 4, 1, 8);
  check_tbl_ctrl(kCompactNib16, 4, 2, 8);
  check_tbl_ctrl(kCompactNib32, 4, 4, 16);
  check_tbl_ctrl(kCompactPair64, 2, 8, 16);
}

}  // namespace
