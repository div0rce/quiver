// K2 filter — scalar reference implementation (the family specification, Charter T3).
// Semantics (PRD 04 §5 K2): dense order-preserving compaction; defined output = first count
// elements (bitmap form: capacity region n, unconditional-store scratch permitted within it,
// REQ-MEM-008); exact aliasing out == in permitted (forward-scan invariant: write index ≤
// read index, ADR-023); selvec form writes exactly sel.len elements in O(sel.len).
// Module: MOD-K2-FILTER | REQs: REQ-K2-001..003, REQ-KERNEL-003 | ADR-023
#pragma once

#include <cstdint>

#include "quiver/core.h"
#include "src/kernels/common/kernel_common.h"

QUIVER_BEGIN_NAMESPACE
namespace detail::scalar_impl {

// The batch a filter reads: the values, how many there are, and the selection bitmap over them.
// The three are meaningless apart, so they travel as one.
template <class T>
struct FilterInput {
  const T* in;
  std::int64_t n;
  const std::uint8_t* selection;
};

template <class T>
std::int64_t filter_bitmap(const T* in, std::int64_t n, const std::uint8_t* selection,
                           T* out) noexcept {
  std::int64_t count = 0;
  for (std::int64_t i = 0; i < n; ++i) {
    out[count] = in[i];  // unconditional store: scratch stays within [0, n) (REQ-MEM-008)
    count += bitmap_get(selection, i) ? 1 : 0;
  }
  return count;
}

template <class T>
std::int64_t filter_selvec(const T* in, const std::uint32_t* sel, std::int64_t sel_len,
                           T* out) noexcept {
  for (std::int64_t j = 0; j < sel_len; ++j) {
    out[j] = in[sel[j]];
  }
  return sel_len;
}

}  // namespace detail::scalar_impl
QUIVER_END_NAMESPACE
