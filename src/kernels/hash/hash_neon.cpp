// K7 hash — NEON backend. The technique is EVIDENCE-GATED (REQ-KERNEL-007; PRD 08 §5 K7):
// (a) vector fmix64 with the 64-bit multiply decomposed via umull/umlal 32x32 products, vs
// (b) unrolled GPR scalar (the reference's 4x-unrolled chain — Apple Firestorm has two GPR
// multiply pipes, Survey §3.9/§4.1). BOTH are compiled below; kUseVectorHash selects the
// shipped default, decided by measurement on the registered Apple M2 at the M6 gate (the
// decision, data, and date are recorded on the family doc page). Results are bit-identical
// either way (REQ-K7-002).
// Module: MOD-K7-HASH | REQs: REQ-K7-001..002, REQ-KERNEL-007, REQ-SIMD-001..003 | ADR-012
#include "src/kernels/hash/hash_scalar_impl.h"

#if defined(__aarch64__) || defined(_M_ARM64)

#include <arm_neon.h>
#include <cstring>
#include <type_traits>

QUIVER_BEGIN_NAMESPACE
namespace detail::neon {

namespace {

// Evidence-gated technique switch (REQ-KERNEL-007): the shipped default is decided by the M6
// ledger measurement recorded in docs/api/hash.md. Both variants stay compiled; the losing
// one is re-evaluated (and kept UBSan/fuzz/differential test-covered) by building with
// -DQUIVER_K7_HASH_VECTOR=1, which flips the default — the documented coverage mechanism for
// the non-shipped variant (see docs/investigations/k7-neon-hash.md). Flip the shipped default
// only with new ledger data.
#ifndef QUIVER_K7_HASH_VECTOR
#define QUIVER_K7_HASH_VECTOR 0
#endif
constexpr bool kUseVectorHash = (QUIVER_K7_HASH_VECTOR != 0);

// Wrapping 64x64 -> low-64 multiply via umull/umlal 32x32 partials.
QUIVER_FORCE_INLINE uint64x2_t hsh_mul64_lo(uint64x2_t a, uint64x2_t b) noexcept {
  const uint32x2_t a_lo = vmovn_u64(a);
  const uint32x2_t b_lo = vmovn_u64(b);
  const uint32x2_t a_hi = vshrn_n_u64(a, 32);
  const uint32x2_t b_hi = vshrn_n_u64(b, 32);
  const uint64x2_t lo_lo = vmull_u32(a_lo, b_lo);
  uint64x2_t cross = vmull_u32(a_lo, b_hi);
  cross = vmlal_u32(cross, a_hi, b_lo);
  return vaddq_u64(lo_lo, vshlq_n_u64(cross, 32));
}

QUIVER_FORCE_INLINE uint64x2_t fmix64_vec(uint64x2_t x) noexcept {
  const uint64x2_t c1 = vdupq_n_u64(scalar_impl::kHashC1);
  const uint64x2_t c2 = vdupq_n_u64(scalar_impl::kHashC2);
  x = veorq_u64(x, vshrq_n_u64(x, 33));
  x = hsh_mul64_lo(x, c1);
  x = veorq_u64(x, vshrq_n_u64(x, 33));
  x = hsh_mul64_lo(x, c2);
  x = veorq_u64(x, vshrq_n_u64(x, 33));
  return x;
}

// key64 for 2 consecutive elements as 64-bit lanes (zero-extension; floats canonicalize
// -0.0 -> +0.0 first, exactly like the scalar reference).
template <class T>
QUIVER_FORCE_INLINE uint64x2_t key64_vec(const T* p) noexcept {
  if constexpr (std::is_same_v<T, float>) {
    float32x2_t v = vld1_f32(p);
    const uint32x2_t zero_mask = vceq_f32(v, vdup_n_f32(0.0f));
    v = vreinterpret_f32_u32(vbic_u32(vreinterpret_u32_f32(v), zero_mask));
    return vmovl_u32(vreinterpret_u32_f32(v));
  } else if constexpr (std::is_same_v<T, double>) {
    float64x2_t v = vld1q_f64(p);
    const uint64x2_t zero_mask = vceqq_f64(v, vdupq_n_f64(0.0));
    return vbicq_u64(vreinterpretq_u64_f64(v), zero_mask);
  } else if constexpr (sizeof(T) == 8) {
    uint64x2_t v;
    std::memcpy(&v, p, 16);
    return v;
  } else {
    // Narrow integers: two scalar zero-extensions beat a widen chain at width 2.
    using U = std::make_unsigned_t<T>;
    const std::uint64_t k0 = static_cast<U>(p[0]);
    const std::uint64_t k1 = static_cast<U>(p[1]);
    return vcombine_u64(vdup_n_u64(k0), vdup_n_u64(k1));
  }
}

template <class T>
void hash64_vector(const T* in, std::int64_t n, std::uint64_t seed, std::uint64_t* out) noexcept {
  const std::uint64_t premix = seed + scalar_impl::kHashGolden;
  const uint64x2_t premix_v = vdupq_n_u64(premix);
  std::int64_t i = 0;
  for (; i + 4 <= n; i += 4) {  // 2 vectors in flight (REQ-SIMD-008 posture)
    const uint64x2_t h0 = fmix64_vec(veorq_u64(key64_vec(in + i), premix_v));
    const uint64x2_t h1 = fmix64_vec(veorq_u64(key64_vec(in + i + 2), premix_v));
    vst1q_u64(out + i, h0);
    vst1q_u64(out + i + 2, h1);
  }
  for (; i < n; ++i) {
    out[i] = scalar_impl::fmix64(scalar_impl::key64(in[i]) ^ premix);
  }
}

}  // namespace

// NOLINTBEGIN(bugprone-macro-parentheses): T expands to type names inside declarators.
#define QUIVER_K7_DEFINE(T)                                                                        \
  void k7_hash64(const T* in, std::int64_t n, std::uint64_t seed, std::uint64_t* out) noexcept {   \
    if (kUseVectorHash) {                                                                          \
      hash64_vector<T>(in, n, seed, out);                                                          \
    } else {                                                                                       \
      scalar_impl::hash64<T>(in, n, seed, out); /* unrolled GPR chain (variant b) */               \
    }                                                                                              \
  }

QUIVER_K7_DEFINE(std::int8_t)
QUIVER_K7_DEFINE(std::int16_t)
QUIVER_K7_DEFINE(std::int32_t)
QUIVER_K7_DEFINE(std::int64_t)
QUIVER_K7_DEFINE(std::uint8_t)
QUIVER_K7_DEFINE(std::uint16_t)
QUIVER_K7_DEFINE(std::uint32_t)
QUIVER_K7_DEFINE(std::uint64_t)
QUIVER_K7_DEFINE(float)
QUIVER_K7_DEFINE(double)
#undef QUIVER_K7_DEFINE
// NOLINTEND(bugprone-macro-parentheses)

void k7_hash64_combine(const std::uint64_t* a, const std::uint64_t* b, std::int64_t n,
                       std::uint64_t* out) noexcept {
  scalar_impl::hash64_combine(a, b, n, out);  // GPR chain (same evidence posture as hash64)
}

}  // namespace detail::neon
QUIVER_END_NAMESPACE

#endif  // aarch64
