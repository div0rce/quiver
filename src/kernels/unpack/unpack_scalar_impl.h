// K8 unpack — scalar reference implementation (the family specification, Charter T3).
//
// Layout (ADR-026, frozen): value i occupies bits [i*w, (i+1)*w) of the packed stream;
// bit j lives at byte j/8, bit j%8 — LSB-first little-endian, Parquet-RLE-compatible,
// byte-order-independent by definition. The reference below is a DIRECT transcription of
// that definition (per-value byte-by-byte bit gathering) — readable, and correct for every
// width 0..64 including the awkward 57..63 range where shift-buffer reservoirs need care;
// fast word-refill variants belong to the SIMD backends, which must match this bit-for-bit.
//
// SECURITY CONTRACT (REQ-K8-002 / REQ-SEC-004): `packed` bytes are UNTRUSTED; the kernel
// reads EXACTLY ceil(n*w/8) bytes — value i touches only the bytes containing its own bits,
// all inside that bound. Page-guard tested and fuzz-prioritized. w == 0 means every value
// is 0 (or `base` for unpack_for) and `packed` may be null (REQ-K8-003). FOR fusion is one
// wrapping add in the store path (REQ-STD-008).
// Module: MOD-K8-UNPACK | REQs: REQ-K8-001..004, REQ-SEC-004 | ADR-026
#pragma once

#include <cstdint>
#include <type_traits>

#include "quiver/core.h"
#include "src/kernels/common/kernel_common.h"

QUIVER_BEGIN_NAMESPACE
namespace detail::scalar_impl {

// Bits [bit, bit+w) of the packed stream, LSB-first (ADR-026). Reads only the bytes those
// bits live in.
QUIVER_FORCE_INLINE std::uint64_t gather_bits(const std::uint8_t* packed, std::int64_t bit,
                                              unsigned w) noexcept {
  std::uint64_t v = 0;
  unsigned got = 0;
  while (got < w) {
    const std::uint8_t byte = packed[bit >> 3];
    const auto off = static_cast<unsigned>(bit & 7);
    const unsigned avail = 8 - off;
    const unsigned take = (w - got) < avail ? (w - got) : avail;
    const auto piece = static_cast<std::uint64_t>((byte >> off) & ((1u << take) - 1u));
    v |= piece << got;
    got += take;
    bit += take;
  }
  return v;
}

template <class Out>
void unpack(const std::uint8_t* packed, std::int64_t n, int bit_width, Out base,
            Out* out) noexcept {
  static_assert(std::is_unsigned_v<Out>, "unpack outputs are unsigned (PRD 04 K8)");
  if (bit_width == 0) {  // REQ-K8-003: all values are base; packed may be null
    for (std::int64_t i = 0; i < n; ++i) {
      out[i] = base;
    }
    return;
  }
  const auto w = static_cast<unsigned>(bit_width);
  for (std::int64_t i = 0; i < n; ++i) {
    const std::uint64_t v = gather_bits(packed, i * static_cast<std::int64_t>(w), w);
    out[i] = static_cast<Out>(base + static_cast<Out>(v));  // wrapping FOR add (unsigned)
  }
}

}  // namespace detail::scalar_impl
QUIVER_END_NAMESPACE
