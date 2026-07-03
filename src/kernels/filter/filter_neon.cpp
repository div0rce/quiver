// K2 filter — NEON backend: nibble TBL compaction (PRD 08 K2 "Lemire simdprune lineage",
// Survey §4.1). Per selection nibble (pair for 64-bit lanes), a MOD-KCOMMON control row
// shuffles the selected lanes front-packed (TBL zero-fills the 0xFF tail) and an exact-width
// store advances the cursor by popcount. Stores never exceed the loaded group, so exact-alias
// in-place (out == in, ADR-023) is safe and every store stays inside the n-element capacity
// region (REQ-MEM-008): 8-bit lanes use two 4-byte nibble stores per loaded 8-byte group;
// 16/32/64-bit groups store exactly their own width. Scalar tails (ADR-015).
// selvec-driven filtering is random-access and delegates to the scalar core (identical).
// Bit-identical to filter_scalar_impl.h on defined output regions (REQ-KERNEL-002).
// Module: MOD-K2-FILTER | REQs: REQ-K2-001..003, REQ-SIMD-001..003/-005 | ADR-003, ADR-023
#include "src/kernels/common/luts.h"
#include "src/kernels/filter/filter_scalar_impl.h"

#if defined(__aarch64__) || defined(_M_ARM64)

#include <arm_neon.h>
#include <cstring>

QUIVER_BEGIN_NAMESPACE
namespace detail::neon {

namespace {

// Scalar remainder shared by every lane width (ADR-015 tails). Identical to the reference.
template <class T>
QUIVER_FORCE_INLINE std::int64_t filter_tail(const T* in, std::int64_t start, std::int64_t n,
                                             const std::uint8_t* selection, T* out,
                                             std::int64_t count) noexcept {
  for (std::int64_t i = start; i < n; ++i) {
    out[count] = in[i];
    count += ((selection[i >> 3] >> (i & 7)) & 1u) != 0 ? 1 : 0;
  }
  return count;
}

QUIVER_FORCE_INLINE std::uint32_t sel_nibble(const std::uint8_t* selection,
                                             std::int64_t i) noexcept {
  return (static_cast<std::uint32_t>(selection[i >> 3]) >> (i & 4)) & 0xFu;
}

template <class T>
std::int64_t filter_bitmap_impl(const T* in, std::int64_t n, const std::uint8_t* selection,
                                T* out) noexcept {
  std::int64_t count = 0;
  std::int64_t i = 0;
  if constexpr (sizeof(T) == 1) {
    // Per 8-byte group: one load, two nibble TBLs, two 4-byte stores (see file comment).
    for (; i + 8 <= n; i += 8) {
      const uint8x8_t src = vld1_u8(reinterpret_cast<const std::uint8_t*>(in) + i);
      const std::uint32_t lo = sel_nibble(selection, i);
      const std::uint32_t hi = sel_nibble(selection, i + 4);
      const uint8x8_t packed_lo = vtbl1_u8(src, vld1_u8(kCompactNib8.ctrl[lo]));
      // The high nibble's lanes are src bytes 4..7: shift the control by 4, restoring the
      // 0xFF fill afterward (0xFF + 4 wraps to a real index otherwise).
      const uint8x8_t c = vld1_u8(kCompactNib8.ctrl[hi]);
      const uint8x8_t c_hi = vorr_u8(vadd_u8(c, vdup_n_u8(4)), vceq_u8(c, vdup_n_u8(0xFF)));
      const uint8x8_t packed_hi = vtbl1_u8(src, c_hi);
      const std::uint32_t w_lo = vget_lane_u32(vreinterpret_u32_u8(packed_lo), 0);
      const std::uint32_t w_hi = vget_lane_u32(vreinterpret_u32_u8(packed_hi), 0);
      std::memcpy(reinterpret_cast<std::uint8_t*>(out) + count, &w_lo, 4);
      count += kPopcountLut.count[lo];
      std::memcpy(reinterpret_cast<std::uint8_t*>(out) + count, &w_hi, 4);
      count += kPopcountLut.count[hi];
    }
  } else if constexpr (sizeof(T) == 2) {
    for (; i + 4 <= n; i += 4) {
      const uint8x8_t src =
          vld1_u8(reinterpret_cast<const std::uint8_t*>(in + i));  // 4 lanes = 8 bytes
      const std::uint32_t nib = sel_nibble(selection, i);
      const uint8x8_t packed = vtbl1_u8(src, vld1_u8(kCompactNib16.ctrl[nib]));
      vst1_u8(reinterpret_cast<std::uint8_t*>(out + count), packed);
      count += kPopcountLut.count[nib];
    }
  } else if constexpr (sizeof(T) == 4) {
    for (; i + 4 <= n; i += 4) {
      const uint8x16_t src =
          vld1q_u8(reinterpret_cast<const std::uint8_t*>(in + i));  // 4 lanes = 16 bytes
      const std::uint32_t nib = sel_nibble(selection, i);
      const uint8x16_t packed = vqtbl1q_u8(src, vld1q_u8(kCompactNib32.ctrl[nib]));
      vst1q_u8(reinterpret_cast<std::uint8_t*>(out + count), packed);
      count += kPopcountLut.count[nib];
    }
  } else {
    for (; i + 2 <= n; i += 2) {
      const uint8x16_t src =
          vld1q_u8(reinterpret_cast<const std::uint8_t*>(in + i));  // 2 lanes = 16 bytes
      const std::uint32_t pair = (static_cast<std::uint32_t>(selection[i >> 3]) >> (i & 6)) & 3u;
      const uint8x16_t packed = vqtbl1q_u8(src, vld1q_u8(kCompactPair64.ctrl[pair]));
      vst1q_u8(reinterpret_cast<std::uint8_t*>(out + count), packed);
      count += kPopcountLut.count[pair];
    }
  }
  return filter_tail(in, i, n, selection, out, count);
}

}  // namespace

// NOLINTBEGIN(bugprone-macro-parentheses): T expands to type names inside declarators.
#define QUIVER_K2_DEFINE(T)                                                                        \
  std::int64_t k2_filter_bitmap(const T* in, std::int64_t n, const std::uint8_t* selection,        \
                                T* out) noexcept {                                                 \
    return filter_bitmap_impl<T>(in, n, selection, out);                                           \
  }                                                                                                \
  std::int64_t k2_filter_selvec(const T* in, const std::uint32_t* sel, std::int64_t sel_len,       \
                                T* out) noexcept {                                                 \
    return scalar_impl::filter_selvec<T>(in, sel, sel_len, out);                                   \
  }

QUIVER_K2_DEFINE(std::int8_t)
QUIVER_K2_DEFINE(std::int16_t)
QUIVER_K2_DEFINE(std::int32_t)
QUIVER_K2_DEFINE(std::int64_t)
QUIVER_K2_DEFINE(std::uint8_t)
QUIVER_K2_DEFINE(std::uint16_t)
QUIVER_K2_DEFINE(std::uint32_t)
QUIVER_K2_DEFINE(std::uint64_t)
QUIVER_K2_DEFINE(float)
QUIVER_K2_DEFINE(double)
#undef QUIVER_K2_DEFINE
// NOLINTEND(bugprone-macro-parentheses)

}  // namespace detail::neon
QUIVER_END_NAMESPACE

#endif  // aarch64
