// MOD-TESTKIT naive reference oracles — the *second*, independently written oracle beside
// each family's scalar `_impl.h` reference (dual-oracle scheme, REQ-TEST-002). Family oracles
// are added in the same milestone as their families (M3/M6); M2 provides the shared bitmap
// primitives those oracles and the self-tests build on.
// Module: MOD-TESTKIT | REQs: REQ-TEST-002 | PRD 05 §7, PRD 12 §2
#pragma once

#include <cstdint>

namespace quiver_test::ref {

// LSB-first bit accessors (REQ-MEM-006 layout), written deliberately simply.
inline bool bit_get(const std::uint8_t* bits, std::int64_t i) {
  return ((bits[i >> 3] >> (i & 7)) & 1u) != 0;
}

inline void bit_set(std::uint8_t* bits, std::int64_t i) {
  bits[i >> 3] = static_cast<std::uint8_t>(bits[i >> 3] | (1u << (i & 7)));
}

inline std::int64_t popcount_bits(const std::uint8_t* bits, std::int64_t n) {
  std::int64_t c = 0;
  for (std::int64_t i = 0; i < n; ++i) {
    c += bit_get(bits, i) ? 1 : 0;
  }
  return c;
}

// Tail-bit check (ADR-016): true iff every bit at position >= n in the final byte is zero.
inline bool tail_bits_zero(const std::uint8_t* bits, std::int64_t n) {
  const int tail = static_cast<int>(n & 7);
  if (tail == 0 || n == 0) {
    return true;
  }
  const std::uint8_t last = bits[(n - 1) >> 3];
  return (last & static_cast<std::uint8_t>(~((1u << tail) - 1u))) == 0;
}

}  // namespace quiver_test::ref
