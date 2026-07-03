// Surface A — K8 unpack: bit-unpacking with frame-of-reference fusion (API-K8-001/002;
// layout frozen in ADR-026: value i occupies bits [i*w, (i+1)*w), LSB-first little-endian,
// Parquet-compatible). SECURITY: `packed` is treated as untrusted — the kernels read
// exactly ceil(n*bit_width/8) bytes, never beyond (REQ-K8-002/REQ-SEC-004).
// Module: MOD-K8-UNPACK | REQs: REQ-K8-001..004 | ADR-006, ADR-026
#pragma once

#include "quiver/core.h"
#include "quiver/detail/config.h"
#include "quiver/detail/extern_decls.h"

QUIVER_BEGIN_NAMESPACE

// Unpacks n bit_width-wide values; bit_width == 0 means every value is 0 and `packed` may
// be null (REQ-K8-003). Writes exactly n elements.
template <UnpackOut Out>
void unpack(const std::uint8_t* packed, std::int64_t n, int bit_width, Out* out) noexcept {
  QUIVER_ASSERT(n >= 0 && n <= kMaxBatchLen, "unpack: 0 <= n <= kMaxBatchLen [REQ-API-005]");
  QUIVER_ASSERT(bit_width >= 0 && bit_width <= 8 * static_cast<int>(sizeof(Out)),
                "unpack: 0 <= bit_width <= 8*sizeof(Out) [API-K8-001]");
  QUIVER_ASSERT(packed != nullptr || bit_width == 0 || n == 0,
                "unpack: packed may be null only when bit_width == 0 [REQ-K8-003]");
  detail::k8_unpack(packed, n, bit_width, Out{0}, out);
}

// Frame-of-reference fusion: out[i] = base + value_i (wrapping unsigned add).
template <UnpackOut Out>
void unpack_for(const std::uint8_t* packed, std::int64_t n, int bit_width, Out base,
                Out* out) noexcept {
  QUIVER_ASSERT(n >= 0 && n <= kMaxBatchLen, "unpack_for: 0 <= n <= kMaxBatchLen [REQ-API-005]");
  QUIVER_ASSERT(bit_width >= 0 && bit_width <= 8 * static_cast<int>(sizeof(Out)),
                "unpack_for: 0 <= bit_width <= 8*sizeof(Out) [API-K8-002]");
  QUIVER_ASSERT(packed != nullptr || bit_width == 0 || n == 0,
                "unpack_for: packed may be null only when bit_width == 0 [REQ-K8-003]");
  detail::k8_unpack(packed, n, bit_width, base, out);
}

QUIVER_END_NAMESPACE
