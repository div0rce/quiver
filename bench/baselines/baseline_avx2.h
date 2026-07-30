// Equal-ISA auto-vectorized baselines (ADR-011, REQ-BENCH-010): the family reference
// implementations recompiled under the AVX2 target region with vectorization enabled,
// exported as the `autovec-avx2` benchmark variant set (REQ-BENCH-002). The set mirrors
// exactly the entry points the family microbenchmarks measure; it grows with the bench
// matrix (full Tier A coverage by M5 per PRD 10 §7).
// Module: MOD-BENCH-BASELINES | REQs: REQ-BENCH-010, REQ-SIMD-006 | ADR-011
#pragma once

#include <cstdint>

#include "quiver/core.h"

#if defined(__x86_64__) || defined(_M_X64)
#define QUIVER_BENCH_HAVE_AUTOVEC_AVX2 1

namespace quiver::bench::autovec_avx2 {

std::int64_t compare_bitmap_i64(quiver::CompareOp op, const std::int64_t* in, std::int64_t n,
                                std::int64_t comparand, const std::uint8_t* validity,
                                std::uint8_t* out) noexcept;
std::int64_t filter_bitmap_i64(const std::int64_t* in, std::int64_t n,
                               const std::uint8_t* selection, std::int64_t* out) noexcept;
std::int64_t bitmap_to_selvec(const std::uint8_t* selection, std::int64_t n,
                              std::uint32_t* out) noexcept;
void mask_combine(quiver::MaskOp op, const std::uint8_t* a, const std::uint8_t* b, std::int64_t n,
                  std::uint8_t* out) noexcept;
void dict_decode_i64_u32(const std::int64_t* dict, std::int64_t dict_len,
                         const std::uint32_t* codes, std::int64_t n, std::int64_t* out) noexcept;
std::int64_t sum_wrap_i64(const std::int64_t* in, std::int64_t n,
                          const std::uint8_t* validity) noexcept;
double sum_wrap_f64(const double* in, std::int64_t n, const std::uint8_t* validity) noexcept;
void hash64_i64(const std::int64_t* in, std::int64_t n, std::uint64_t seed,
                std::uint64_t* out) noexcept;
void unpack_for_u32(const std::uint8_t* packed, std::int64_t n, int bit_width, std::uint32_t base,
                    std::uint32_t* out) noexcept;
void arith_i64(quiver::ArithOp op, const std::int64_t* a, const std::int64_t* b, std::int64_t n,
               std::int64_t* out) noexcept;
// K10. Both guarded forms the family benchmarks measure: the checked op returns the overflow
// count and fills the overflow bitmap, the saturating op clamps (REQ-K10-002/-003).
std::int64_t arith_checked_i64(quiver::ArithOp op, const std::int64_t* a, const std::int64_t* b,
                               std::int64_t n, std::int64_t* out,
                               std::uint8_t* overflow_bits) noexcept;
void arith_saturating_i64(quiver::ArithOp op, const std::int64_t* a, const std::int64_t* b,
                          std::int64_t n, std::int64_t* out) noexcept;

}  // namespace quiver::bench::autovec_avx2

#endif  // x86-64
