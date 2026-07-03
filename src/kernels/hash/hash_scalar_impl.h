// K7 hash — scalar reference implementation. THIS FILE IS THE FAMILY'S SPECIFICATION
// (Charter T3): every backend must match it bit-for-bit, on every ISA and platform
// (REQ-K7-002 — cross-platform stability is the family's entire point).
//
// Algorithm (ADR-012, FROZEN — any change to constants or rounds is a major-version event):
//   GOLDEN = 0x9E3779B97F4A7C15   C1 = 0xFF51AFD7ED558CCD   C2 = 0xC4CEB9FE1A85EC53
//   fmix64(x): x ^= x>>33; x *= C1; x ^= x>>33; x *= C2; x ^= x>>33
//   key64(v):  integers -> the value's two's-complement bit pattern ZERO-extended to 64
//              bits (sign bits deliberately NOT extended — documented); f32/f64 -> the bit
//              pattern with -0.0 canonicalized to +0.0 first (NaN payloads hash as-is)
//   qhash64(v, seed)     = fmix64(key64(v) ^ (seed + GOLDEN))
//   hash64_combine(a, b) = fmix64(a ^ (b + GOLDEN + (a << 6) + (a >> 2)))
// Non-cryptographic by contract (Charter §7.4). No validity parameter by design: null
// handling is engine policy (REQ-K7-004; the doc page shows the composition idiom).
// Module: MOD-K7-HASH | REQs: REQ-K7-001..004, REQ-KERNEL-001..002 | ADR-012
#pragma once

#include <bit>
#include <cstdint>
#include <type_traits>

#include "quiver/core.h"
#include "src/kernels/common/kernel_common.h"

QUIVER_BEGIN_NAMESPACE
namespace detail::scalar_impl {

inline constexpr std::uint64_t kHashGolden = 0x9E3779B97F4A7C15ull;
inline constexpr std::uint64_t kHashC1 = 0xFF51AFD7ED558CCDull;
inline constexpr std::uint64_t kHashC2 = 0xC4CEB9FE1A85EC53ull;

QUIVER_FORCE_INLINE constexpr std::uint64_t fmix64(std::uint64_t x) noexcept {
  x ^= x >> 33;
  x *= kHashC1;
  x ^= x >> 33;
  x *= kHashC2;
  x ^= x >> 33;
  return x;
}

// key64: zero-extended bit pattern; floats canonicalize -0.0 to +0.0 first (ADR-012).
template <class T>
QUIVER_FORCE_INLINE std::uint64_t key64(T v) noexcept {
  if constexpr (std::is_same_v<T, float>) {
    const float c = (v == 0.0f) ? 0.0f : v;  // +0.0 for both zeros; NaN != 0 keeps payload
    return std::bit_cast<std::uint32_t>(c);
  } else if constexpr (std::is_same_v<T, double>) {
    const double c = (v == 0.0) ? 0.0 : v;
    return std::bit_cast<std::uint64_t>(c);
  } else {
    using U = std::make_unsigned_t<T>;
    return static_cast<std::uint64_t>(static_cast<U>(v));  // zero-extend the bit pattern
  }
}

QUIVER_FORCE_INLINE constexpr std::uint64_t qhash64_key(std::uint64_t key,
                                                        std::uint64_t seed) noexcept {
  return fmix64(key ^ (seed + kHashGolden));
}

// 4x-unrolled independent chains (PRD 08 K7 scalar technique; Survey §3.9).
template <class T>
void hash64(const T* in, std::int64_t n, std::uint64_t seed, std::uint64_t* out) noexcept {
  const std::uint64_t premix = seed + kHashGolden;
  std::int64_t i = 0;
  for (; i + 4 <= n; i += 4) {
    const std::uint64_t h0 = fmix64(key64(in[i + 0]) ^ premix);
    const std::uint64_t h1 = fmix64(key64(in[i + 1]) ^ premix);
    const std::uint64_t h2 = fmix64(key64(in[i + 2]) ^ premix);
    const std::uint64_t h3 = fmix64(key64(in[i + 3]) ^ premix);
    out[i + 0] = h0;
    out[i + 1] = h1;
    out[i + 2] = h2;
    out[i + 3] = h3;
  }
  for (; i < n; ++i) {
    out[i] = fmix64(key64(in[i]) ^ premix);
  }
}

QUIVER_FORCE_INLINE constexpr std::uint64_t combine_one(std::uint64_t a,
                                                        std::uint64_t b) noexcept {
  return fmix64(a ^ (b + kHashGolden + (a << 6) + (a >> 2)));
}

// out == a or out == b exact aliasing permitted (elementwise forward pass, API-K7-002).
inline void hash64_combine(const std::uint64_t* a, const std::uint64_t* b, std::int64_t n,
                           std::uint64_t* out) noexcept {
  std::int64_t i = 0;
  for (; i + 4 <= n; i += 4) {
    const std::uint64_t h0 = combine_one(a[i + 0], b[i + 0]);
    const std::uint64_t h1 = combine_one(a[i + 1], b[i + 1]);
    const std::uint64_t h2 = combine_one(a[i + 2], b[i + 2]);
    const std::uint64_t h3 = combine_one(a[i + 3], b[i + 3]);
    out[i + 0] = h0;
    out[i + 1] = h1;
    out[i + 2] = h2;
    out[i + 3] = h3;
  }
  for (; i < n; ++i) {
    out[i] = combine_one(a[i], b[i]);
  }
}

}  // namespace detail::scalar_impl
QUIVER_END_NAMESPACE
