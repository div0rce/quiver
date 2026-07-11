// Guard-page suite (REQ-TEST-006): every Tier A kernel runs with inputs AND outputs flush
// against protected pages, across all tail residues — the executable proof of REQ-MEM-001
// (no over-read/-write) with output guards at the CAPACITY boundary (REQ-MEM-008). The fused
// K5 decode additionally proves it never touches unselected code positions (REQ-K5-003):
// unselected codes live in the protected page.
// Covers: REQ-MEM-001/-008, REQ-TEST-006, REQ-K5-003, REQ-SIMD-003 (per-ISA from M4)
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "quiver/quiver.h"
#include "tests/testkit/generators.h"
#include "tests/testkit/reference.h"

namespace {

using quiver_test::Guard;
using quiver_test::GuardedBuffer;

template <class T>
void guard_sweep_for_type() {
  quiver_test::Rng rng(0x6A2DED);
  for (const Guard g : {Guard::kEnd, Guard::kStart}) {
    for (const std::int64_t n : {1, 7, 8, 9, 63, 64, 65, 127, 128, 129, 255, 256, 257}) {
      const std::int64_t bytes = (n + 7) / 8;
      GuardedBuffer<T> in(n, g);
      quiver_test::fill_uniform(rng, in.data(), n);
      GuardedBuffer<std::uint8_t> sel_bits(bytes, g);
      quiver_test::fill_bitmap_uniform(rng, sel_bits.data(), n, 50);
      GuardedBuffer<std::uint8_t> out_bits(bytes, g);
      GuardedBuffer<std::uint32_t> out_idx(n, g);  // capacity region n (REQ-MEM-008)
      GuardedBuffer<T> out_vals(n, g);

      quiver::compare_bitmap(quiver::CompareOp::kNe, quiver::BatchView<T>{in.data(), n}, T{},
                             quiver::BitmapView{nullptr}, out_bits.data());
      quiver::compare_selvec(quiver::CompareOp::kNe, quiver::BatchView<T>{in.data(), n}, T{},
                             quiver::BitmapView{sel_bits.data()}, out_idx.data());
      quiver::filter(quiver::BatchView<T>{in.data(), n}, quiver::BitmapView{sel_bits.data()},
                     out_vals.data());
      const std::int64_t cnt =
          quiver::bitmap_to_selvec(quiver::BitmapView{sel_bits.data()}, n, out_idx.data());
      quiver::selvec_to_bitmap(quiver::SelVec{out_idx.data(), cnt}, n, out_bits.data());
      quiver::mask_not(quiver::BitmapView{sel_bits.data()}, n, out_bits.data());
      (void)quiver::mask_popcount(quiver::BitmapView{sel_bits.data()}, n);
      quiver::take(quiver::BatchView<T>{in.data(), n}, quiver::SelVec{out_idx.data(), cnt},
                   out_vals.data());
      (void)quiver::reduce_min(quiver::BatchView<T>{in.data(), n},
                               quiver::BitmapView{sel_bits.data()});
      (void)quiver::compute_min_max(quiver::BatchView<T>{in.data(), n},
                                    quiver::BitmapView{sel_bits.data()});
    }
  }
}

TEST(InvGuardPages, TierAKernelsStayInBounds) {
  ASSERT_NE(quiver_test::GuardedBuffer<std::uint8_t>(8, Guard::kEnd).data(), nullptr)
      << "guard-page allocation unavailable on this platform";
  guard_sweep_for_type<std::int8_t>();
  guard_sweep_for_type<std::int32_t>();
  guard_sweep_for_type<std::int64_t>();
  guard_sweep_for_type<double>();
}

TEST(InvGuardPages, FusedDecodeNeverTouchesUnselectedCodes) {  // REQ-K5-003
  // Codes buffer: first half accessible, second half in the protected page. The selection
  // only names positions in the accessible half; the fused decode must not fault.
  const std::int64_t n_total = 1024;
  const std::int64_t n_live = 512;
  GuardedBuffer<std::uint16_t> codes(n_live, Guard::kEnd);  // codes[n_live..) would fault
  quiver_test::Rng rng(0xFE7C4);
  constexpr std::int64_t kDict = 41;
  for (std::int64_t i = 0; i < n_live; ++i) {
    codes.data()[i] = static_cast<std::uint16_t>(rng.next_below(kDict));
  }
  std::vector<double> dict(kDict);
  quiver_test::fill_uniform(rng, dict.data(), kDict);
  std::vector<std::uint32_t> sel;
  for (std::int64_t i = 0; i < n_live; i += 3) {
    sel.push_back(static_cast<std::uint32_t>(i));
  }
  std::vector<double> out(sel.size());
  // n covers the full (partly protected) logical batch; only selected positions are read.
  quiver::dict_decode(quiver::BatchView<double>{dict.data(), kDict}, codes.data(), n_total,
                      quiver::SelVec{sel.data(), static_cast<std::int64_t>(sel.size())},
                      out.data());
  for (std::size_t j = 0; j < sel.size(); ++j) {
    ASSERT_EQ(out[j], dict[codes.data()[sel[j]]]);
  }
}

}  // namespace
