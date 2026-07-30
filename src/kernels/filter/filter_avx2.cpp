// K2 filter — AVX2 backend: emulated-compress compaction (PRD 08 K2, Survey §4.1).
// 32-bit lanes: per selection byte, kCompactLut32 row -> vpermd, full-vector store, cursor
// advances by popcount (all stores stay within the n-element capacity region: the output
// cursor never exceeds the processed-input count, REQ-MEM-008 / PRD 09 §6).
// 64-bit lanes: per nibble via kCompactLut64, which stores the epi32 pair indices directly.
// 8/16-bit lanes: BMI2 PDEP/PEXT byte-compaction (mask expanded to byte/word granularity,
// PEXT extracts selected lanes) — a documented technique deviation from the PRD's pshufb
// sketch, recorded in the M4 gate (same output, simpler and exact; bmi2 is in the AVX2
// target set; Zen 2 microcoded-PEXT caveat noted for the ledger).
// selvec-driven filtering is random-access and delegates to the scalar core (identical).
// Bit-identical to filter_scalar_impl.h on defined output regions (REQ-KERNEL-002).
// Module: MOD-K2-FILTER | REQs: REQ-K2-001..003, REQ-SIMD-001..003/-005 | ADR-003, ADR-023
#include "src/kernels/common/luts.h"
#include "src/kernels/common/target_regions.h"
#include "src/kernels/filter/filter_scalar_impl.h"

#if defined(__x86_64__) || defined(_M_X64)

#include <cstring>
#include <immintrin.h>

QUIVER_BEGIN_NAMESPACE
QUIVER_TARGET_AVX2_BEGIN
namespace detail::avx2 {

namespace {

// Scalar remainder shared by every lane width (ADR-015 tails; also used for in-place safety
// on the trailing partial byte). Identical to the reference core.
template <class T>
QUIVER_FORCE_INLINE std::int64_t filter_tail(scalar_impl::FilterInput<T> batch, std::int64_t start,
                                             T* out, std::int64_t count) noexcept {
  const T* in = batch.in;
  const std::int64_t n = batch.n;
  const std::uint8_t* selection = batch.selection;
  for (std::int64_t i = start; i < n; ++i) {
    out[count] = in[i];
    count += ((selection[i >> 3] >> (i & 7)) & 1u) != 0 ? 1 : 0;
  }
  return count;
}

QUIVER_FORCE_INLINE std::int64_t compact8_32bit(const std::uint32_t* in, std::uint8_t byte,
                                                std::uint32_t* out, std::int64_t count) noexcept {
  const __m256i v = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(in));
  const __m256i perm =
      _mm256_loadu_si256(reinterpret_cast<const __m256i*>(kCompactLut32.perm[byte]));
  _mm256_storeu_si256(reinterpret_cast<__m256i*>(out + count),
                      _mm256_permutevar8x32_epi32(v, perm));
  return count + kPopcountLut.count[byte];
}

// In-place safety (ADR-023): both half-block vectors are LOADED before either store — a
// store at the cursor may reach into the current block's later lanes, so reads must come
// first (the scalar core has the same property element-wise).
QUIVER_FORCE_INLINE std::int64_t compact4_64bit(__m256i v, unsigned nibble, std::uint64_t* out,
                                                std::int64_t count) noexcept {
  // The nibble row already holds the epi32 pair indices {2p, 2p+1} (luts.h), so the vpermd
  // control vector is one aligned 32-byte load instead of an 8-way scalar insert chain.
  const __m256i pairs =
      _mm256_load_si256(reinterpret_cast<const __m256i*>(kCompactLut64.perm[nibble]));
  _mm256_storeu_si256(reinterpret_cast<__m256i*>(out + count),
                      _mm256_permutevar8x32_epi32(v, pairs));
  return count + kPopcountLut.count[nibble];
}

// BMI2 byte-granular compaction: 8 one-byte lanes per selection byte.
QUIVER_FORCE_INLINE std::int64_t compact8_8bit(const std::uint8_t* in, std::uint8_t byte,
                                               std::uint8_t* out, std::int64_t count) noexcept {
  std::uint64_t data = 0;
  std::memcpy(&data, in, 8);
  const std::uint64_t lane_mask = _pdep_u64(byte, 0x0101010101010101ull) * 0xFFull;
  const std::uint64_t packed = _pext_u64(data, lane_mask);
  std::memcpy(out + count, &packed, 8);  // full store within capacity (REQ-MEM-008)
  return count + kPopcountLut.count[byte];
}

// BMI2 word-granular compaction: 4 two-byte lanes per selection nibble.
QUIVER_FORCE_INLINE std::int64_t compact4_16bit(std::uint64_t data, unsigned nibble,
                                                std::uint16_t* out, std::int64_t count) noexcept {
  const std::uint64_t lane_mask = _pdep_u64(nibble, 0x0001000100010001ull) * 0xFFFFull;
  const std::uint64_t packed = _pext_u64(data, lane_mask);
  std::memcpy(out + count, &packed, 8);
  return count + kPopcountLut.count[nibble];
}

template <class T>
std::int64_t filter_bitmap_impl(const T* in, std::int64_t n, const std::uint8_t* selection,
                                T* out) noexcept {
  std::int64_t count = 0;
  const std::int64_t full_bytes = n >> 3;
  if constexpr (sizeof(T) == 4) {
    for (std::int64_t b = 0; b < full_bytes; ++b) {
      count = compact8_32bit(reinterpret_cast<const std::uint32_t*>(in) + (b << 3), selection[b],
                             reinterpret_cast<std::uint32_t*>(out), count);
    }
  } else if constexpr (sizeof(T) == 8) {
    for (std::int64_t b = 0; b < full_bytes; ++b) {
      const std::uint8_t byte = selection[b];
      const auto* base = reinterpret_cast<const std::uint64_t*>(in) + (b << 3);
      const __m256i lo = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(base));
      const __m256i hi = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(base + 4));
      count = compact4_64bit(lo, byte & 0xFu, reinterpret_cast<std::uint64_t*>(out), count);
      count = compact4_64bit(hi, (byte >> 4) & 0xFu, reinterpret_cast<std::uint64_t*>(out), count);
    }
  } else if constexpr (sizeof(T) == 1) {
    for (std::int64_t b = 0; b < full_bytes; ++b) {
      count = compact8_8bit(reinterpret_cast<const std::uint8_t*>(in) + (b << 3), selection[b],
                            reinterpret_cast<std::uint8_t*>(out), count);
    }
  } else {  // sizeof(T) == 2
    for (std::int64_t b = 0; b < full_bytes; ++b) {
      const std::uint8_t byte = selection[b];
      const auto* base = reinterpret_cast<const std::uint16_t*>(in) + (b << 3);
      std::uint64_t lo = 0;
      std::uint64_t hi = 0;
      std::memcpy(&lo, base, 8);  // both halves read before any store (in-place safety)
      std::memcpy(&hi, base + 4, 8);
      count = compact4_16bit(lo, byte & 0xFu, reinterpret_cast<std::uint16_t*>(out), count);
      count = compact4_16bit(hi, (byte >> 4) & 0xFu, reinterpret_cast<std::uint16_t*>(out), count);
    }
  }
  return filter_tail({in, n, selection}, full_bytes << 3, out, count);
}

}  // namespace

// In-place safety argument (ADR-023, out == in permitted): each block's input lanes are
// fully loaded into registers before its stores; a store spans [count, count+lanes) with
// count <= block_start, i.e. never past the current block's end — so no unread input is
// ever clobbered. Scratch past the cursor stays inside the n-element capacity region
// (REQ-MEM-008). Bit-identical to the scalar reference on defined output regions.
// NOLINTBEGIN(bugprone-macro-parentheses): T expands to type names inside declarators.
#define QUIVER_K2_DEFINE(suffix, T)                                                                \
  std::int64_t k2_filter_bitmap(const T* in, std::int64_t n, const std::uint8_t* selection,        \
                                T* out) noexcept {                                                 \
    return filter_bitmap_impl<T>(in, n, selection, out);                                           \
  }                                                                                                \
  std::int64_t k2_filter_selvec(const T* in, const std::uint32_t* sel, std::int64_t sel_len,       \
                                T* out) noexcept {                                                 \
    return scalar_impl::filter_selvec<T>(in, sel, sel_len, out);                                   \
  }

QUIVER_K2_DEFINE(i8, std::int8_t)
QUIVER_K2_DEFINE(i16, std::int16_t)
QUIVER_K2_DEFINE(i32, std::int32_t)
QUIVER_K2_DEFINE(i64, std::int64_t)
QUIVER_K2_DEFINE(u8, std::uint8_t)
QUIVER_K2_DEFINE(u16, std::uint16_t)
QUIVER_K2_DEFINE(u32, std::uint32_t)
QUIVER_K2_DEFINE(u64, std::uint64_t)
QUIVER_K2_DEFINE(f32, float)
QUIVER_K2_DEFINE(f64, double)
#undef QUIVER_K2_DEFINE
// NOLINTEND(bugprone-macro-parentheses)

}  // namespace detail::avx2
QUIVER_TARGET_AVX2_END
QUIVER_END_NAMESPACE

#endif  // x86-64
