// K8 unpack — NEON backend (PRD 08 §5 K8; ADR-026). Same specialization structure as the
// AVX2 backend: byte-aligned widths w ∈ {8, 16, 32} widen with vmovl chains, and
// w == 8*sizeof(Out) is a straight copy + FOR add; sub-byte and irregular widths route
// through the scalar reference's per-value bit gather (bit-identical by definition).
// The sub-byte (w in [1,7]) ushl expansion is the SHIPPED default (QUIVER_K8_SUBBYTE_VECTOR=1): it
// vectorizes 8 values per iteration and measured 6.5x to 10.5x faster than the scalar gather on a
// quiet Apple M2 (u32, CV under 1%), so it is on production dispatch. Building with
// -DQUIVER_K8_SUBBYTE_VECTOR=0 reverts sub-byte to the scalar reference (the documented fallback
// and coverage mechanism, mirroring QUIVER_K7_HASH_VECTOR); the vector path is also exposed via
// unpack_neon_candidate.h for direct differential/guard-page testing. See the sub-byte unpack
// investigation page for the measurement and correctness proof.
// SECURITY: every specialized block reads exactly its own bytes inside the ceil(n*w/8)
// bound (REQ-K8-002/REQ-SEC-004); tails are scalar (ADR-015). For integer w, 8 values occupy
// exactly w bytes and re-align to a byte boundary, which is what makes the sub-byte vector loop
// bounded (8-value blocks reading exactly w bytes, scalar tail for the final <8).
// Module: MOD-K8-UNPACK | REQs: REQ-K8-001..004, REQ-SEC-004 | ADR-003, ADR-026
#include "src/kernels/unpack/unpack_scalar_impl.h"

#if defined(__aarch64__) || defined(_M_ARM64)

#include "src/kernels/unpack/unpack_neon_candidate.h"

#include <arm_neon.h>
#include <cstring>
#include <type_traits>

// Evidence-gated technique switch (REQ-KERNEL-007): default 1 ships the vectorized sub-byte path
// (measured 6.5x-10.5x over scalar on a quiet Apple M2). Build with -DQUIVER_K8_SUBBYTE_VECTOR=0 to
// revert sub-byte to the scalar reference (fallback and A/B coverage).
#ifndef QUIVER_K8_SUBBYTE_VECTOR
#define QUIVER_K8_SUBBYTE_VECTOR 1
#endif

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

// --- Sub-byte NEON candidate (w in [1,7]) --------------------------------------------------------
// Store 8 extracted values (two u32x4 vectors, lanes 0-3 and 4-7, each value < 128) with the
// wrapping FOR add, narrowed or widened to Out. Bit-identical to `base + value` in the reference.
template <class Out>
QUIVER_FORCE_INLINE void subbyte_store8(uint32x4_t lo, uint32x4_t hi, Out base, Out* dst) noexcept {
  if constexpr (sizeof(Out) == 4) {
    vst1q_u32(reinterpret_cast<std::uint32_t*>(dst), vaddq_u32(lo, vdupq_n_u32(base)));
    vst1q_u32(reinterpret_cast<std::uint32_t*>(dst) + 4, vaddq_u32(hi, vdupq_n_u32(base)));
  } else if constexpr (sizeof(Out) == 2) {
    const uint16x8_t v = vcombine_u16(vmovn_u32(lo), vmovn_u32(hi));
    vst1q_u16(reinterpret_cast<std::uint16_t*>(dst),
              vaddq_u16(v, vdupq_n_u16(static_cast<std::uint16_t>(base))));
  } else if constexpr (sizeof(Out) == 1) {
    const uint16x8_t v16 = vcombine_u16(vmovn_u32(lo), vmovn_u32(hi));
    const uint8x8_t v8 = vmovn_u16(v16);
    vst1_u8(reinterpret_cast<std::uint8_t*>(dst),
            vadd_u8(v8, vdup_n_u8(static_cast<std::uint8_t>(base))));
  } else {  // sizeof(Out) == 8
    const uint64x2_t b = vdupq_n_u64(base);
    vst1q_u64(reinterpret_cast<std::uint64_t*>(dst), vaddq_u64(vmovl_u32(vget_low_u32(lo)), b));
    vst1q_u64(reinterpret_cast<std::uint64_t*>(dst) + 2,
              vaddq_u64(vmovl_u32(vget_high_u32(lo)), b));
    vst1q_u64(reinterpret_cast<std::uint64_t*>(dst) + 4, vaddq_u64(vmovl_u32(vget_low_u32(hi)), b));
    vst1q_u64(reinterpret_cast<std::uint64_t*>(dst) + 6,
              vaddq_u64(vmovl_u32(vget_high_u32(hi)), b));
  }
}

// Core: bit_width in [1,7]. Every 8 values occupy exactly w bytes and re-align to a byte boundary,
// so each 8-value block reads exactly w bytes via an inline bounded byte load (never an
// over-reading vector load); the final fewer-than-8 values go through the scalar reference's
// exact-byte gather. No byte past ceil(n*w/8) is read (REQ-K8-002 / REQ-SEC-004). Bit-identical to
// scalar_impl::unpack.
template <class Out>
void unpack_subbyte_core(scalar_impl::PackedStream in, Out base, Out* out) noexcept {
  const std::uint8_t* packed = in.packed;
  const std::int64_t n = in.n;
  const int w = in.bit_width;
  const std::uint32_t mask = (1u << static_cast<unsigned>(w)) - 1u;
  const uint32x4_t vmask = vdupq_n_u32(mask);
  const std::int32_t sh[4] = {0, -w, -2 * w, -3 * w};  // per-lane right shifts j*w
  const int32x4_t shifts = vld1q_s32(sh);
  const std::int64_t nb = n / 8;  // whole 8-value blocks (each exactly w bytes)
  std::int64_t i = 0;
  for (std::int64_t block = 0; block < nb; ++block, i += 8) {
    // Assemble exactly w bytes (little-endian) inline; a runtime-size memcpy compiles to a call
    // per block, which would dominate the 8-value work. Reads only bytes [block*w, block*w+w).
    std::uint64_t word = 0;
    const std::uint8_t* src = packed + block * static_cast<std::int64_t>(w);
    for (int k = 0; k < w; ++k) {
      word |= static_cast<std::uint64_t>(src[k]) << (8 * k);
    }
    const auto packed_lo = static_cast<std::uint32_t>(word);
    const auto packed_hi = static_cast<std::uint32_t>(word >> (4u * static_cast<unsigned>(w)));
    const uint32x4_t v_lo = vandq_u32(vshlq_u32(vdupq_n_u32(packed_lo), shifts), vmask);  // 0..3
    const uint32x4_t v_hi = vandq_u32(vshlq_u32(vdupq_n_u32(packed_hi), shifts), vmask);  // 4..7
    subbyte_store8<Out>(v_lo, v_hi, base, out + i);
  }
  for (; i < n; ++i) {  // scalar tail, identical exact-byte gather (ADR-015)
    const std::uint64_t v = scalar_impl::gather_bits(packed, i * static_cast<std::int64_t>(w),
                                                     static_cast<unsigned>(w));
    out[i] = static_cast<Out>(base + static_cast<Out>(v));
  }
}

template <class Out>
void unpack_impl(scalar_impl::PackedStream in, Out base, Out* out) noexcept {
  const std::uint8_t* packed = in.packed;
  const std::int64_t n = in.n;
  const int bit_width = in.bit_width;
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
#if QUIVER_K8_SUBBYTE_VECTOR
  if (bit_width >= 1 && bit_width <= 7) {  // shipped default: vectorized sub-byte unpack
    unpack_subbyte_core<Out>({packed, n, bit_width}, base, out);
    return;
  }
#endif
  scalar_impl::unpack<Out>({packed, n, bit_width}, base, out);  // generic path (w=0, irregular w)
}

}  // namespace

// NOLINTBEGIN(bugprone-macro-parentheses): T expands to type names inside declarators.
#define QUIVER_K8_DEFINE(T)                                                                        \
  void k8_unpack(const std::uint8_t* packed, std::int64_t n, int bit_width, T base,                \
                 T* out) noexcept {                                                                \
    unpack_impl<T>({packed, n, bit_width}, base, out);                                             \
  }

QUIVER_K8_DEFINE(std::uint8_t)
QUIVER_K8_DEFINE(std::uint16_t)
QUIVER_K8_DEFINE(std::uint32_t)
QUIVER_K8_DEFINE(std::uint64_t)
#undef QUIVER_K8_DEFINE
// NOLINTEND(bugprone-macro-parentheses)

// Experimental candidate exposed for direct differential/guard-page testing on aarch64. Production
// dispatch does NOT call this (unpack_neon_candidate.h explains the seam); it is here so CI
// exercises the candidate's correctness and bounds without changing what the shipped k8_unpack
// runs.
template <class Out>
void unpack_subbyte_candidate(scalar_impl::PackedStream in, Out base, Out* out) noexcept {
  unpack_subbyte_core<Out>(in, base, out);
}
template void unpack_subbyte_candidate<std::uint8_t>(scalar_impl::PackedStream, std::uint8_t,
                                                     std::uint8_t*) noexcept;
template void unpack_subbyte_candidate<std::uint16_t>(scalar_impl::PackedStream, std::uint16_t,
                                                      std::uint16_t*) noexcept;
template void unpack_subbyte_candidate<std::uint32_t>(scalar_impl::PackedStream, std::uint32_t,
                                                      std::uint32_t*) noexcept;
template void unpack_subbyte_candidate<std::uint64_t>(scalar_impl::PackedStream, std::uint64_t,
                                                      std::uint64_t*) noexcept;

}  // namespace detail::neon
QUIVER_END_NAMESPACE

#endif  // aarch64
