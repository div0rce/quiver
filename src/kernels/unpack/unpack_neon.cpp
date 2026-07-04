// K8 unpack — NEON backend (PRD 08 §5 K8; ADR-026). Same specialization structure as the
// AVX2 backend: byte-aligned widths w ∈ {8, 16, 32} widen with vmovl chains, and
// w == 8*sizeof(Out) is a straight copy + FOR add; sub-byte and irregular widths route
// through the scalar reference's per-value bit gather (bit-identical by definition; the
// TBL/ushl sub-byte expansion is ledger-gated follow-up recorded on the family page).
// SECURITY: every specialized block reads exactly its own bytes inside the ceil(n*w/8)
// bound (REQ-K8-002/REQ-SEC-004); tails are scalar (ADR-015).
// Module: MOD-K8-UNPACK | REQs: REQ-K8-001..004, REQ-SEC-004 | ADR-003, ADR-026
#include "src/kernels/unpack/unpack_scalar_impl.h"

#if defined(__aarch64__) || defined(_M_ARM64)

#include <arm_neon.h>
#include <cstring>
#include <type_traits>

QUIVER_BEGIN_NAMESPACE
namespace detail::neon {

namespace {

// One 128-bit vector of Out lanes from kSrcBytes-wide packed values (zero-extension).
// The load touches exactly kSrcBytes * lanes bytes.
template <class Out, int kSrcBytes>
QUIVER_FORCE_INLINE void widen_store(const std::uint8_t* p, Out base, Out* out) noexcept {
  if constexpr (sizeof(Out) == 1) {  // kSrcBytes == 1
    const uint8x16_t v = vld1q_u8(p);
    vst1q_u8(out, vaddq_u8(v, vdupq_n_u8(base)));
  } else if constexpr (sizeof(Out) == 2) {
    uint16x8_t v;
    if constexpr (kSrcBytes == 1) {
      v = vmovl_u8(vld1_u8(p));  // 8 bytes -> 8 u16
    } else {
      v = vld1q_u16(reinterpret_cast<const std::uint16_t*>(p));
    }
    vst1q_u16(out, vaddq_u16(v, vdupq_n_u16(base)));
  } else if constexpr (sizeof(Out) == 4) {
    uint32x4_t v;
    if constexpr (kSrcBytes == 1) {
      std::uint32_t raw = 0;
      std::memcpy(&raw, p, 4);  // 4 bytes -> 4 u32
      v = vmovl_u16(vget_low_u16(vmovl_u8(vreinterpret_u8_u32(vdup_n_u32(raw)))));
    } else if constexpr (kSrcBytes == 2) {
      v = vmovl_u16(vld1_u16(reinterpret_cast<const std::uint16_t*>(p)));  // 8B -> 4 u32
    } else {
      v = vld1q_u32(reinterpret_cast<const std::uint32_t*>(p));
    }
    vst1q_u32(out, vaddq_u32(v, vdupq_n_u32(base)));
  } else {
    uint64x2_t v;
    if constexpr (kSrcBytes == 1) {
      std::uint16_t raw = 0;
      std::memcpy(&raw, p, 2);  // 2 bytes -> 2 u64
      const std::uint64_t lanes[2] = {static_cast<std::uint8_t>(raw & 0xFF),
                                      static_cast<std::uint8_t>(raw >> 8)};
      std::memcpy(&v, lanes, 16);
    } else if constexpr (kSrcBytes == 2) {
      std::uint16_t raw[2];
      std::memcpy(raw, p, 4);  // 4 bytes -> 2 u64
      const std::uint64_t lanes[2] = {raw[0], raw[1]};
      std::memcpy(&v, lanes, 16);
    } else if constexpr (kSrcBytes == 4) {
      v = vmovl_u32(vld1_u32(reinterpret_cast<const std::uint32_t*>(p)));  // 8B -> 2 u64
    } else {
      v = vld1q_u64(reinterpret_cast<const std::uint64_t*>(p));
    }
    vst1q_u64(out, vaddq_u64(v, vdupq_n_u64(base)));
  }
}

template <class Out, int kSrcBytes>
void unpack_bytes(const std::uint8_t* packed, std::int64_t n, Out base, Out* out) noexcept {
  constexpr std::int64_t kLanes = static_cast<std::int64_t>(16 / sizeof(Out));
  std::int64_t i = 0;
  for (; i + kLanes <= n; i += kLanes) {
    widen_store<Out, kSrcBytes>(packed + i * kSrcBytes, base, out + i);
  }
  for (; i < n; ++i) {  // scalar tail, same gather arithmetic
    const std::uint64_t v =
        scalar_impl::gather_bits(packed, i * (std::int64_t{8} * kSrcBytes), 8u * kSrcBytes);
    out[i] = static_cast<Out>(base + static_cast<Out>(v));
  }
}

template <class Out>
void unpack_impl(const std::uint8_t* packed, std::int64_t n, int bit_width, Out base,
                 Out* out) noexcept {
  switch (bit_width) {
  case 8:
    unpack_bytes<Out, 1>(packed, n, base, out);
    return;
  case 16:
    if constexpr (sizeof(Out) >= 2) {
      unpack_bytes<Out, 2>(packed, n, base, out);
      return;
    }
    break;
  case 32:
    if constexpr (sizeof(Out) >= 4) {
      unpack_bytes<Out, 4>(packed, n, base, out);
      return;
    }
    break;
  case 64:
    if constexpr (sizeof(Out) == 8) {
      unpack_bytes<Out, 8>(packed, n, base, out);
      return;
    }
    break;
  default:
    break;
  }
  scalar_impl::unpack<Out>(packed, n, bit_width, base, out);  // generic gather path
}

}  // namespace

// NOLINTBEGIN(bugprone-macro-parentheses): T expands to type names inside declarators.
#define QUIVER_K8_DEFINE(T)                                                                        \
  void k8_unpack(const std::uint8_t* packed, std::int64_t n, int bit_width, T base,                \
                 T* out) noexcept {                                                                \
    unpack_impl<T>(packed, n, bit_width, base, out);                                               \
  }

QUIVER_K8_DEFINE(std::uint8_t)
QUIVER_K8_DEFINE(std::uint16_t)
QUIVER_K8_DEFINE(std::uint32_t)
QUIVER_K8_DEFINE(std::uint64_t)
#undef QUIVER_K8_DEFINE
// NOLINTEND(bugprone-macro-parentheses)

}  // namespace detail::neon
QUIVER_END_NAMESPACE

#endif  // aarch64
