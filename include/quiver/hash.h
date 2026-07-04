// Surface A — K7 hash: batch qhash64 with cross-ISA/platform bit-identical output
// (API-K7-001/002; algorithm and constants FROZEN in ADR-012 — stable per major version;
// non-cryptographic by contract, Charter §7.4). No validity parameter by design: null
// handling is engine policy — combine the output with validity externally (the family doc
// page shows the idiom).
// Module: MOD-K7-HASH | REQs: REQ-K7-001..004 | ADR-006, ADR-012
#pragma once

#include "quiver/core.h"
#include "quiver/detail/config.h"
#include "quiver/detail/extern_decls.h"

QUIVER_BEGIN_NAMESPACE

// out[i] = qhash64(in.data[i], seed); writes exactly in.len values.
template <Element T>
void hash64(BatchView<T> in, std::uint64_t seed, std::uint64_t* out) noexcept {
  QUIVER_ASSERT(in.len >= 0 && in.len <= kMaxBatchLen,
                "hash64: 0 <= n <= kMaxBatchLen [REQ-API-005]");
  detail::k7_hash64(in.data, in.len, seed, out);
}

// out[i] = fmix64(a[i] ^ (b[i] + GOLDEN + (a[i] << 6) + (a[i] >> 2))) — the frozen ADR-012
// combine for multi-column keys. Exact aliasing out == a or out == b is permitted.
inline void hash64_combine(const std::uint64_t* a, const std::uint64_t* b, std::int64_t n,
                           std::uint64_t* out) noexcept {
  QUIVER_ASSERT(n >= 0 && n <= kMaxBatchLen,
                "hash64_combine: 0 <= n <= kMaxBatchLen [REQ-API-005]");
  detail::k7_hash64_combine(a, b, n, out);
}

QUIVER_END_NAMESPACE
