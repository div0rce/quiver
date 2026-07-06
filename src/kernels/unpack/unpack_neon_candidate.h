// K8 unpack — sub-byte NEON candidate (EXPERIMENTAL, not on production dispatch).
//
// This declares the vectorized sub-byte unpacker that is being evaluated as a replacement for the
// scalar per-value bit gather on widths 1..7. It is NOT wired into production dispatch: the
// `k8_unpack` backend still delegates sub-byte widths to the scalar reference unless the
// translation unit is compiled with -DQUIVER_K8_SUBBYTE_VECTOR=1 (the documented
// coverage-and-evaluation mechanism, mirroring QUIVER_K7_HASH_VECTOR). The function is exposed here
// so the differential and guard-page tests can exercise it directly on aarch64, giving CI
// correctness coverage of the candidate without changing what production runs. Ship to dispatch
// only after a quiet-machine ledger measurement (REQ-KERNEL-007). See
// docs/benchmarks/investigations/apple-m2-subbyte-unpack-neon.md.
//
// SECURITY CONTRACT (REQ-K8-002 / REQ-SEC-004): identical to the reference. For integer width w,
// 8 values occupy exactly w bytes and re-align to a byte boundary, so the vector loop processes
// 8-value blocks reading exactly w bytes each and a scalar tail handles the final <8 values; no
// byte past ceil(n*w/8) is ever read. Module: MOD-K8-UNPACK | REQs: REQ-K8-001..004, REQ-SEC-004 |
// ADR-026
#pragma once

#include <cstdint>

#include "quiver/core.h"

#if defined(__aarch64__) || defined(_M_ARM64)

QUIVER_BEGIN_NAMESPACE
namespace detail::neon {

// Sub-byte (bit_width in [1,7]) vectorized unpack-with-FOR. Bit-identical to
// scalar_impl::unpack for those widths; bounded to ceil(n*bit_width/8) input bytes. Caller must
// pass bit_width in [1,7]; other widths are out of scope for this candidate.
template <class Out>
void unpack_subbyte_candidate(const std::uint8_t* packed, std::int64_t n, int bit_width, Out base,
                              Out* out) noexcept;

}  // namespace detail::neon
QUIVER_END_NAMESPACE

#endif  // aarch64
