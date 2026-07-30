// Equal-ISA auto-vectorized baselines (ADR-011, REQ-BENCH-010): recompiles the pure family
// references (`_scalar_impl.h`, REQ-SIMD-006) under the AVX2 target region so the compiler
// may auto-vectorize them with the SAME instruction set the explicit backends use — the
// fair-baseline requirement (Survey §7.5; strawman-baseline pitfall).
//
// ODR/COMDAT integrity: the references are templates also instantiated (baseline-ISA) inside
// libquiver; if this TU instantiated the same symbols under AVX2, the linker could silently
// unify them either way — corrupting the `scalar` variant with AVX2 codegen or this baseline
// with scalar codegen. The includes below therefore reopen the headers with the namespace
// macros redefined, instantiating everything inside a PRIVATE root namespace: every symbol
// here is disjoint from the library's by construction. The using-directives make the real
// core types and kernel_common helpers visible to the reopened header bodies.
// Module: MOD-BENCH-BASELINES | REQs: REQ-BENCH-010, REQ-SIMD-006 | ADR-003, ADR-011
#include "bench/baselines/baseline_avx2.h"

#if defined(__x86_64__) || defined(_M_X64)

#include <bit>
#include <cstdint>
#include <limits>
#include <type_traits>

#include "quiver/core.h"
#include "src/kernels/common/kernel_common.h"
#include "src/kernels/common/target_regions.h"

namespace quiver_autovec_avx2_impl {
using namespace ::quiver;          // NOLINT(google-build-using-namespace): see file comment
using namespace ::quiver::detail;  // NOLINT(google-build-using-namespace): see file comment
}  // namespace quiver_autovec_avx2_impl

// Reopen the reference headers into the private root (see file comment). The redefinition is
// intentional and scoped to this TU.
#undef QUIVER_BEGIN_NAMESPACE
#undef QUIVER_END_NAMESPACE
#define QUIVER_BEGIN_NAMESPACE namespace quiver_autovec_avx2_impl {
#define QUIVER_END_NAMESPACE }

QUIVER_TARGET_AVX2_BEGIN

#include "src/kernels/arith/arith_scalar_impl.h"
#include "src/kernels/compare/compare_scalar_impl.h"
#include "src/kernels/filter/filter_scalar_impl.h"
#include "src/kernels/hash/hash_scalar_impl.h"
#include "src/kernels/mask/mask_scalar_impl.h"
#include "src/kernels/reduce/reduce_scalar_impl.h"
#include "src/kernels/select/select_scalar_impl.h"
#include "src/kernels/take/take_scalar_impl.h"
#include "src/kernels/unpack/unpack_scalar_impl.h"

namespace quiver::bench::autovec_avx2 {

namespace impl = ::quiver_autovec_avx2_impl::detail::scalar_impl;

std::int64_t compare_bitmap_i64(quiver::CompareOp op, const std::int64_t* in, std::int64_t n,
                                std::int64_t comparand, const std::uint8_t* validity,
                                std::uint8_t* out) noexcept {
  return impl::compare_bitmap<std::int64_t>(op, {in, n, validity}, comparand, out);
}

std::int64_t filter_bitmap_i64(const std::int64_t* in, std::int64_t n,
                               const std::uint8_t* selection, std::int64_t* out) noexcept {
  return impl::filter_bitmap<std::int64_t>(in, n, selection, out);
}

std::int64_t bitmap_to_selvec(const std::uint8_t* selection, std::int64_t n,
                              std::uint32_t* out) noexcept {
  return impl::bitmap_to_selvec(selection, n, out);
}

void mask_combine(quiver::MaskOp op, const std::uint8_t* a, const std::uint8_t* b, std::int64_t n,
                  std::uint8_t* out) noexcept {
  impl::mask_combine(op, {a, b}, n, out);
}

void dict_decode_i64_u32(const std::int64_t* dict, std::int64_t dict_len,
                         const std::uint32_t* codes, std::int64_t n, std::int64_t* out) noexcept {
  impl::dict_decode<std::int64_t, std::uint32_t>({dict, dict_len}, codes, n, out);
}

std::int64_t sum_wrap_i64(const std::int64_t* in, std::int64_t n,
                          const std::uint8_t* validity) noexcept {
  return impl::reduce_sum_wrap<std::int64_t>(in, {n, validity, nullptr, 0});
}

double sum_wrap_f64(const double* in, std::int64_t n, const std::uint8_t* validity) noexcept {
  // The reference float sum is a STRICT sequential fold; the honest autovec baseline keeps
  // those semantics — if the compiler cannot vectorize it without reassociation, that IS the
  // published comparison (Charter T7; ADR-013).
  return impl::reduce_sum_wrap<double>(in, {n, validity, nullptr, 0});
}

void hash64_i64(const std::int64_t* in, std::int64_t n, std::uint64_t seed,
                std::uint64_t* out) noexcept {
  impl::hash64<std::int64_t>(in, n, seed, out);
}

void unpack_for_u32(const std::uint8_t* packed, std::int64_t n, int bit_width, std::uint32_t base,
                    std::uint32_t* out) noexcept {
  impl::unpack<std::uint32_t>({packed, n, bit_width}, base, out);
}

void arith_i64(quiver::ArithOp op, const std::int64_t* a, const std::int64_t* b, std::int64_t n,
               std::int64_t* out) noexcept {
  impl::arith<std::int64_t>(op, {a, n}, b, out);
}

}  // namespace quiver::bench::autovec_avx2

QUIVER_TARGET_AVX2_END

#endif  // x86-64
