// K3 sel_convert — scalar reference implementation (the family specification, Charter T3).
// Semantics (PRD 04 §5 K3): lossless conversion between the two selection representations;
// round-trips are identities (property-tested). bitmap_to_selvec: strictly increasing defined
// output = first count = popcount entries within the n-element capacity region (REQ-MEM-008).
// selvec_to_bitmap: set bits are exactly sel; all other bits including tails zero (ADR-016).
// Module: MOD-K3-SELECT | REQs: REQ-K3-001..002 | ADR-016, ADR-025
#pragma once

#include <cstdint>
#include <cstring>

#include "quiver/core.h"
#include "src/kernels/common/kernel_common.h"

QUIVER_BEGIN_NAMESPACE
namespace detail::scalar_impl {

inline std::int64_t bitmap_to_selvec(const std::uint8_t* selection, std::int64_t n,
                                     std::uint32_t* out) noexcept {
  std::int64_t count = 0;
  for (std::int64_t i = 0; i < n; ++i) {
    out[count] = static_cast<std::uint32_t>(i);  // scratch within capacity (REQ-MEM-008)
    count += bitmap_get(selection, i) ? 1 : 0;
  }
  return count;
}

inline void selvec_to_bitmap(const std::uint32_t* sel, std::int64_t sel_len, std::int64_t n,
                             std::uint8_t* out) noexcept {
  std::memset(out, 0, static_cast<std::size_t>(bitmap_byte_count(n)));
  for (std::int64_t j = 0; j < sel_len; ++j) {
    const std::uint32_t i = sel[j];
    out[i >> 3] = static_cast<std::uint8_t>(out[i >> 3] | (1u << (i & 7)));
  }
}

}  // namespace detail::scalar_impl
QUIVER_END_NAMESPACE
