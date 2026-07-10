// K8 unpack — sub-byte NEON vectorizer (SHIPPED default on aarch64).
//
// This declares the vectorized sub-byte unpacker that replaced the scalar per-value bit gather on
// widths 1..7. It is the production path by default (QUIVER_K8_SUBBYTE_VECTOR=1), promoted after a
// quiet Apple M2 measurement showed 6.5x to 10.5x over scalar at CV under 1% (REQ-KERNEL-007).
// Building with -DQUIVER_K8_SUBBYTE_VECTOR=0 reverts sub-byte to the scalar reference (fallback and
// A/B coverage, mirroring QUIVER_K7_HASH_VECTOR). The function is also exposed here so the
// differential and guard-page tests exercise it directly on aarch64. See
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
