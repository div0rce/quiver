// MOD-KCOMMON: shared kernel utilities — bitmap word/byte access, tail masks, participation
// helpers, and wrapping-arithmetic helpers. Families stay mutually independent by sharing
// only this module (REQ-REPO-009).
// Module: MOD-KCOMMON | REQs: REQ-MEM-006, REQ-STD-008, REQ-KERNEL-002 | ADR-016
#pragma once

#include <bit>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "quiver/core.h"
#include "quiver/detail/config.h"

QUIVER_BEGIN_NAMESPACE
namespace detail {

// --- Bitmap primitives (LSB-first, 1 = valid/selected; REQ-MEM-006) --------------------------

QUIVER_FORCE_INLINE bool bitmap_get(const std::uint8_t* bits, std::int64_t i) noexcept {
  return ((bits[i >> 3] >> (i & 7)) & 1u) != 0;
}

// Validity test honoring the null-means-all-valid convention (REQ-API-008).
QUIVER_FORCE_INLINE bool is_valid(const std::uint8_t* validity, std::int64_t i) noexcept {
  return validity == nullptr || bitmap_get(validity, i);
}

QUIVER_FORCE_INLINE std::int64_t bitmap_bytes(std::int64_t n) noexcept {
  return (n + 7) >> 3;
}

// Mask covering the live bits of the final byte; producers zero everything above it
// (ADR-016 tail-zeroing — what makes bitmap outputs memcmp-comparable).
QUIVER_FORCE_INLINE std::uint8_t tail_mask(std::int64_t n) noexcept {
  const int tail = static_cast<int>(n & 7);
  return tail == 0 ? std::uint8_t{0xFF} : static_cast<std::uint8_t>((1u << tail) - 1u);
}

QUIVER_FORCE_INLINE void zero_tail_bits(std::uint8_t* bits, std::int64_t n) noexcept {
  if (n > 0) {
    bits[(n - 1) >> 3] = static_cast<std::uint8_t>(bits[(n - 1) >> 3] & tail_mask(n));
  }
}

// 64-bit word access over byte streams via memcpy — never a misaligned dereference
// (REQ-SEC-002, REQ-SIMD-007 discipline applies to scalar code too).
QUIVER_FORCE_INLINE std::uint64_t load_word(const std::uint8_t* p) noexcept {
  std::uint64_t w = 0;
  std::memcpy(&w, p, sizeof(w));
  return w;
}

QUIVER_FORCE_INLINE void store_word(std::uint8_t* p, std::uint64_t w) noexcept {
  std::memcpy(p, &w, sizeof(w));
}

// --- Wrapping arithmetic on possibly-signed types (REQ-STD-008 unsigned-internal idiom) ------

template <class T>
QUIVER_FORCE_INLINE T wrapping_add(T a, T b) noexcept {
  using U = std::make_unsigned_t<T>;
  return static_cast<T>(static_cast<U>(static_cast<U>(a) + static_cast<U>(b)));
}

template <class T>
QUIVER_FORCE_INLINE T wrapping_sub(T a, T b) noexcept {
  using U = std::make_unsigned_t<T>;
  return static_cast<T>(static_cast<U>(static_cast<U>(a) - static_cast<U>(b)));
}

template <class T>
QUIVER_FORCE_INLINE T wrapping_mul(T a, T b) noexcept {
  using U = std::make_unsigned_t<T>;
  return static_cast<T>(static_cast<U>(static_cast<U>(a) * static_cast<U>(b)));
}

}  // namespace detail
QUIVER_END_NAMESPACE
