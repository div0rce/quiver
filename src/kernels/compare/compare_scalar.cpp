// K1 compare — concrete scalar backends: one symbol per admissible template-parameter
// combination (ADR-006), each a thin instantiation of the compare_scalar_impl.h reference.
// GENERATED-BY-HAND-STABLE: regenerate only with a documented spec change (REQ-KERNEL-005).
// Module: MOD-K1-COMPARE | REQs: REQ-KERNEL-001/-005 | ADR-006
#include "src/kernels/compare/compare_scalar_impl.h"

QUIVER_BEGIN_NAMESPACE
namespace detail::scalar {

std::int64_t k1_compare_bitmap(CompareOp op, const std::int8_t* in, std::int64_t n,
                               std::int8_t comparand, const std::uint8_t* validity,
                               std::uint8_t* out) noexcept {
  return scalar_impl::compare_bitmap<std::int8_t>(op, {in, n, validity}, comparand, out);
}

std::int64_t k1_compare_bitmap2(CompareOp op, const std::int8_t* a, const std::int8_t* b,
                                std::int64_t n, const std::uint8_t* a_validity,
                                const std::uint8_t* b_validity, std::uint8_t* out) noexcept {
  return scalar_impl::compare_bitmap2<std::int8_t>(op, {a, n, a_validity}, {b, n, b_validity}, out);
}

std::int64_t k1_compare_between_bitmap(const std::int8_t* in, std::int64_t n, std::int8_t lo,
                                       std::int8_t hi, const std::uint8_t* validity,
                                       std::uint8_t* out) noexcept {
  return scalar_impl::compare_between_bitmap<std::int8_t>({in, n, validity}, lo, hi, out);
}

std::int64_t k1_compare_selvec(CompareOp op, const std::int8_t* in, std::int64_t n,
                               std::int8_t comparand, const std::uint8_t* validity,
                               std::uint32_t* out) noexcept {
  return scalar_impl::compare_selvec<std::int8_t>(op, {in, n, validity}, comparand, out);
}

std::int64_t k1_compare_selvec2(CompareOp op, const std::int8_t* a, const std::int8_t* b,
                                std::int64_t n, const std::uint8_t* a_validity,
                                const std::uint8_t* b_validity, std::uint32_t* out) noexcept {
  return scalar_impl::compare_selvec2<std::int8_t>(op, {a, n, a_validity}, {b, n, b_validity}, out);
}

std::int64_t k1_compare_between_selvec(const std::int8_t* in, std::int64_t n, std::int8_t lo,
                                       std::int8_t hi, const std::uint8_t* validity,
                                       std::uint32_t* out) noexcept {
  return scalar_impl::compare_between_selvec<std::int8_t>({in, n, validity}, lo, hi, out);
}

std::int64_t k1_compare_bitmap(CompareOp op, const std::int16_t* in, std::int64_t n,
                               std::int16_t comparand, const std::uint8_t* validity,
                               std::uint8_t* out) noexcept {
  return scalar_impl::compare_bitmap<std::int16_t>(op, {in, n, validity}, comparand, out);
}

std::int64_t k1_compare_bitmap2(CompareOp op, const std::int16_t* a, const std::int16_t* b,
                                std::int64_t n, const std::uint8_t* a_validity,
                                const std::uint8_t* b_validity, std::uint8_t* out) noexcept {
  return scalar_impl::compare_bitmap2<std::int16_t>(op, {a, n, a_validity}, {b, n, b_validity},
                                                    out);
}

std::int64_t k1_compare_between_bitmap(const std::int16_t* in, std::int64_t n, std::int16_t lo,
                                       std::int16_t hi, const std::uint8_t* validity,
                                       std::uint8_t* out) noexcept {
  return scalar_impl::compare_between_bitmap<std::int16_t>({in, n, validity}, lo, hi, out);
}

std::int64_t k1_compare_selvec(CompareOp op, const std::int16_t* in, std::int64_t n,
                               std::int16_t comparand, const std::uint8_t* validity,
                               std::uint32_t* out) noexcept {
  return scalar_impl::compare_selvec<std::int16_t>(op, {in, n, validity}, comparand, out);
}

std::int64_t k1_compare_selvec2(CompareOp op, const std::int16_t* a, const std::int16_t* b,
                                std::int64_t n, const std::uint8_t* a_validity,
                                const std::uint8_t* b_validity, std::uint32_t* out) noexcept {
  return scalar_impl::compare_selvec2<std::int16_t>(op, {a, n, a_validity}, {b, n, b_validity},
                                                    out);
}

std::int64_t k1_compare_between_selvec(const std::int16_t* in, std::int64_t n, std::int16_t lo,
                                       std::int16_t hi, const std::uint8_t* validity,
                                       std::uint32_t* out) noexcept {
  return scalar_impl::compare_between_selvec<std::int16_t>({in, n, validity}, lo, hi, out);
}

std::int64_t k1_compare_bitmap(CompareOp op, const std::int32_t* in, std::int64_t n,
                               std::int32_t comparand, const std::uint8_t* validity,
                               std::uint8_t* out) noexcept {
  return scalar_impl::compare_bitmap<std::int32_t>(op, {in, n, validity}, comparand, out);
}

std::int64_t k1_compare_bitmap2(CompareOp op, const std::int32_t* a, const std::int32_t* b,
                                std::int64_t n, const std::uint8_t* a_validity,
                                const std::uint8_t* b_validity, std::uint8_t* out) noexcept {
  return scalar_impl::compare_bitmap2<std::int32_t>(op, {a, n, a_validity}, {b, n, b_validity},
                                                    out);
}

std::int64_t k1_compare_between_bitmap(const std::int32_t* in, std::int64_t n, std::int32_t lo,
                                       std::int32_t hi, const std::uint8_t* validity,
                                       std::uint8_t* out) noexcept {
  return scalar_impl::compare_between_bitmap<std::int32_t>({in, n, validity}, lo, hi, out);
}

std::int64_t k1_compare_selvec(CompareOp op, const std::int32_t* in, std::int64_t n,
                               std::int32_t comparand, const std::uint8_t* validity,
                               std::uint32_t* out) noexcept {
  return scalar_impl::compare_selvec<std::int32_t>(op, {in, n, validity}, comparand, out);
}

std::int64_t k1_compare_selvec2(CompareOp op, const std::int32_t* a, const std::int32_t* b,
                                std::int64_t n, const std::uint8_t* a_validity,
                                const std::uint8_t* b_validity, std::uint32_t* out) noexcept {
  return scalar_impl::compare_selvec2<std::int32_t>(op, {a, n, a_validity}, {b, n, b_validity},
                                                    out);
}

std::int64_t k1_compare_between_selvec(const std::int32_t* in, std::int64_t n, std::int32_t lo,
                                       std::int32_t hi, const std::uint8_t* validity,
                                       std::uint32_t* out) noexcept {
  return scalar_impl::compare_between_selvec<std::int32_t>({in, n, validity}, lo, hi, out);
}

std::int64_t k1_compare_bitmap(CompareOp op, const std::int64_t* in, std::int64_t n,
                               std::int64_t comparand, const std::uint8_t* validity,
                               std::uint8_t* out) noexcept {
  return scalar_impl::compare_bitmap<std::int64_t>(op, {in, n, validity}, comparand, out);
}

std::int64_t k1_compare_bitmap2(CompareOp op, const std::int64_t* a, const std::int64_t* b,
                                std::int64_t n, const std::uint8_t* a_validity,
                                const std::uint8_t* b_validity, std::uint8_t* out) noexcept {
  return scalar_impl::compare_bitmap2<std::int64_t>(op, {a, n, a_validity}, {b, n, b_validity},
                                                    out);
}

std::int64_t k1_compare_between_bitmap(const std::int64_t* in, std::int64_t n, std::int64_t lo,
                                       std::int64_t hi, const std::uint8_t* validity,
                                       std::uint8_t* out) noexcept {
  return scalar_impl::compare_between_bitmap<std::int64_t>({in, n, validity}, lo, hi, out);
}

std::int64_t k1_compare_selvec(CompareOp op, const std::int64_t* in, std::int64_t n,
                               std::int64_t comparand, const std::uint8_t* validity,
                               std::uint32_t* out) noexcept {
  return scalar_impl::compare_selvec<std::int64_t>(op, {in, n, validity}, comparand, out);
}

std::int64_t k1_compare_selvec2(CompareOp op, const std::int64_t* a, const std::int64_t* b,
                                std::int64_t n, const std::uint8_t* a_validity,
                                const std::uint8_t* b_validity, std::uint32_t* out) noexcept {
  return scalar_impl::compare_selvec2<std::int64_t>(op, {a, n, a_validity}, {b, n, b_validity},
                                                    out);
}

std::int64_t k1_compare_between_selvec(const std::int64_t* in, std::int64_t n, std::int64_t lo,
                                       std::int64_t hi, const std::uint8_t* validity,
                                       std::uint32_t* out) noexcept {
  return scalar_impl::compare_between_selvec<std::int64_t>({in, n, validity}, lo, hi, out);
}

std::int64_t k1_compare_bitmap(CompareOp op, const std::uint8_t* in, std::int64_t n,
                               std::uint8_t comparand, const std::uint8_t* validity,
                               std::uint8_t* out) noexcept {
  return scalar_impl::compare_bitmap<std::uint8_t>(op, {in, n, validity}, comparand, out);
}

std::int64_t k1_compare_bitmap2(CompareOp op, const std::uint8_t* a, const std::uint8_t* b,
                                std::int64_t n, const std::uint8_t* a_validity,
                                const std::uint8_t* b_validity, std::uint8_t* out) noexcept {
  return scalar_impl::compare_bitmap2<std::uint8_t>(op, {a, n, a_validity}, {b, n, b_validity},
                                                    out);
}

std::int64_t k1_compare_between_bitmap(const std::uint8_t* in, std::int64_t n, std::uint8_t lo,
                                       std::uint8_t hi, const std::uint8_t* validity,
                                       std::uint8_t* out) noexcept {
  return scalar_impl::compare_between_bitmap<std::uint8_t>({in, n, validity}, lo, hi, out);
}

std::int64_t k1_compare_selvec(CompareOp op, const std::uint8_t* in, std::int64_t n,
                               std::uint8_t comparand, const std::uint8_t* validity,
                               std::uint32_t* out) noexcept {
  return scalar_impl::compare_selvec<std::uint8_t>(op, {in, n, validity}, comparand, out);
}

std::int64_t k1_compare_selvec2(CompareOp op, const std::uint8_t* a, const std::uint8_t* b,
                                std::int64_t n, const std::uint8_t* a_validity,
                                const std::uint8_t* b_validity, std::uint32_t* out) noexcept {
  return scalar_impl::compare_selvec2<std::uint8_t>(op, {a, n, a_validity}, {b, n, b_validity},
                                                    out);
}

std::int64_t k1_compare_between_selvec(const std::uint8_t* in, std::int64_t n, std::uint8_t lo,
                                       std::uint8_t hi, const std::uint8_t* validity,
                                       std::uint32_t* out) noexcept {
  return scalar_impl::compare_between_selvec<std::uint8_t>({in, n, validity}, lo, hi, out);
}

std::int64_t k1_compare_bitmap(CompareOp op, const std::uint16_t* in, std::int64_t n,
                               std::uint16_t comparand, const std::uint8_t* validity,
                               std::uint8_t* out) noexcept {
  return scalar_impl::compare_bitmap<std::uint16_t>(op, {in, n, validity}, comparand, out);
}

std::int64_t k1_compare_bitmap2(CompareOp op, const std::uint16_t* a, const std::uint16_t* b,
                                std::int64_t n, const std::uint8_t* a_validity,
                                const std::uint8_t* b_validity, std::uint8_t* out) noexcept {
  return scalar_impl::compare_bitmap2<std::uint16_t>(op, {a, n, a_validity}, {b, n, b_validity},
                                                     out);
}

std::int64_t k1_compare_between_bitmap(const std::uint16_t* in, std::int64_t n, std::uint16_t lo,
                                       std::uint16_t hi, const std::uint8_t* validity,
                                       std::uint8_t* out) noexcept {
  return scalar_impl::compare_between_bitmap<std::uint16_t>({in, n, validity}, lo, hi, out);
}

std::int64_t k1_compare_selvec(CompareOp op, const std::uint16_t* in, std::int64_t n,
                               std::uint16_t comparand, const std::uint8_t* validity,
                               std::uint32_t* out) noexcept {
  return scalar_impl::compare_selvec<std::uint16_t>(op, {in, n, validity}, comparand, out);
}

std::int64_t k1_compare_selvec2(CompareOp op, const std::uint16_t* a, const std::uint16_t* b,
                                std::int64_t n, const std::uint8_t* a_validity,
                                const std::uint8_t* b_validity, std::uint32_t* out) noexcept {
  return scalar_impl::compare_selvec2<std::uint16_t>(op, {a, n, a_validity}, {b, n, b_validity},
                                                     out);
}

std::int64_t k1_compare_between_selvec(const std::uint16_t* in, std::int64_t n, std::uint16_t lo,
                                       std::uint16_t hi, const std::uint8_t* validity,
                                       std::uint32_t* out) noexcept {
  return scalar_impl::compare_between_selvec<std::uint16_t>({in, n, validity}, lo, hi, out);
}

std::int64_t k1_compare_bitmap(CompareOp op, const std::uint32_t* in, std::int64_t n,
                               std::uint32_t comparand, const std::uint8_t* validity,
                               std::uint8_t* out) noexcept {
  return scalar_impl::compare_bitmap<std::uint32_t>(op, {in, n, validity}, comparand, out);
}

std::int64_t k1_compare_bitmap2(CompareOp op, const std::uint32_t* a, const std::uint32_t* b,
                                std::int64_t n, const std::uint8_t* a_validity,
                                const std::uint8_t* b_validity, std::uint8_t* out) noexcept {
  return scalar_impl::compare_bitmap2<std::uint32_t>(op, {a, n, a_validity}, {b, n, b_validity},
                                                     out);
}

std::int64_t k1_compare_between_bitmap(const std::uint32_t* in, std::int64_t n, std::uint32_t lo,
                                       std::uint32_t hi, const std::uint8_t* validity,
                                       std::uint8_t* out) noexcept {
  return scalar_impl::compare_between_bitmap<std::uint32_t>({in, n, validity}, lo, hi, out);
}

std::int64_t k1_compare_selvec(CompareOp op, const std::uint32_t* in, std::int64_t n,
                               std::uint32_t comparand, const std::uint8_t* validity,
                               std::uint32_t* out) noexcept {
  return scalar_impl::compare_selvec<std::uint32_t>(op, {in, n, validity}, comparand, out);
}

std::int64_t k1_compare_selvec2(CompareOp op, const std::uint32_t* a, const std::uint32_t* b,
                                std::int64_t n, const std::uint8_t* a_validity,
                                const std::uint8_t* b_validity, std::uint32_t* out) noexcept {
  return scalar_impl::compare_selvec2<std::uint32_t>(op, {a, n, a_validity}, {b, n, b_validity},
                                                     out);
}

std::int64_t k1_compare_between_selvec(const std::uint32_t* in, std::int64_t n, std::uint32_t lo,
                                       std::uint32_t hi, const std::uint8_t* validity,
                                       std::uint32_t* out) noexcept {
  return scalar_impl::compare_between_selvec<std::uint32_t>({in, n, validity}, lo, hi, out);
}

std::int64_t k1_compare_bitmap(CompareOp op, const std::uint64_t* in, std::int64_t n,
                               std::uint64_t comparand, const std::uint8_t* validity,
                               std::uint8_t* out) noexcept {
  return scalar_impl::compare_bitmap<std::uint64_t>(op, {in, n, validity}, comparand, out);
}

std::int64_t k1_compare_bitmap2(CompareOp op, const std::uint64_t* a, const std::uint64_t* b,
                                std::int64_t n, const std::uint8_t* a_validity,
                                const std::uint8_t* b_validity, std::uint8_t* out) noexcept {
  return scalar_impl::compare_bitmap2<std::uint64_t>(op, {a, n, a_validity}, {b, n, b_validity},
                                                     out);
}

std::int64_t k1_compare_between_bitmap(const std::uint64_t* in, std::int64_t n, std::uint64_t lo,
                                       std::uint64_t hi, const std::uint8_t* validity,
                                       std::uint8_t* out) noexcept {
  return scalar_impl::compare_between_bitmap<std::uint64_t>({in, n, validity}, lo, hi, out);
}

std::int64_t k1_compare_selvec(CompareOp op, const std::uint64_t* in, std::int64_t n,
                               std::uint64_t comparand, const std::uint8_t* validity,
                               std::uint32_t* out) noexcept {
  return scalar_impl::compare_selvec<std::uint64_t>(op, {in, n, validity}, comparand, out);
}

std::int64_t k1_compare_selvec2(CompareOp op, const std::uint64_t* a, const std::uint64_t* b,
                                std::int64_t n, const std::uint8_t* a_validity,
                                const std::uint8_t* b_validity, std::uint32_t* out) noexcept {
  return scalar_impl::compare_selvec2<std::uint64_t>(op, {a, n, a_validity}, {b, n, b_validity},
                                                     out);
}

std::int64_t k1_compare_between_selvec(const std::uint64_t* in, std::int64_t n, std::uint64_t lo,
                                       std::uint64_t hi, const std::uint8_t* validity,
                                       std::uint32_t* out) noexcept {
  return scalar_impl::compare_between_selvec<std::uint64_t>({in, n, validity}, lo, hi, out);
}

std::int64_t k1_compare_bitmap(CompareOp op, const float* in, std::int64_t n, float comparand,
                               const std::uint8_t* validity, std::uint8_t* out) noexcept {
  return scalar_impl::compare_bitmap<float>(op, {in, n, validity}, comparand, out);
}

std::int64_t k1_compare_bitmap2(CompareOp op, const float* a, const float* b, std::int64_t n,
                                const std::uint8_t* a_validity, const std::uint8_t* b_validity,
                                std::uint8_t* out) noexcept {
  return scalar_impl::compare_bitmap2<float>(op, {a, n, a_validity}, {b, n, b_validity}, out);
}

std::int64_t k1_compare_between_bitmap(const float* in, std::int64_t n, float lo, float hi,
                                       const std::uint8_t* validity, std::uint8_t* out) noexcept {
  return scalar_impl::compare_between_bitmap<float>({in, n, validity}, lo, hi, out);
}

std::int64_t k1_compare_selvec(CompareOp op, const float* in, std::int64_t n, float comparand,
                               const std::uint8_t* validity, std::uint32_t* out) noexcept {
  return scalar_impl::compare_selvec<float>(op, {in, n, validity}, comparand, out);
}

std::int64_t k1_compare_selvec2(CompareOp op, const float* a, const float* b, std::int64_t n,
                                const std::uint8_t* a_validity, const std::uint8_t* b_validity,
                                std::uint32_t* out) noexcept {
  return scalar_impl::compare_selvec2<float>(op, {a, n, a_validity}, {b, n, b_validity}, out);
}

std::int64_t k1_compare_between_selvec(const float* in, std::int64_t n, float lo, float hi,
                                       const std::uint8_t* validity, std::uint32_t* out) noexcept {
  return scalar_impl::compare_between_selvec<float>({in, n, validity}, lo, hi, out);
}

std::int64_t k1_compare_bitmap(CompareOp op, const double* in, std::int64_t n, double comparand,
                               const std::uint8_t* validity, std::uint8_t* out) noexcept {
  return scalar_impl::compare_bitmap<double>(op, {in, n, validity}, comparand, out);
}

std::int64_t k1_compare_bitmap2(CompareOp op, const double* a, const double* b, std::int64_t n,
                                const std::uint8_t* a_validity, const std::uint8_t* b_validity,
                                std::uint8_t* out) noexcept {
  return scalar_impl::compare_bitmap2<double>(op, {a, n, a_validity}, {b, n, b_validity}, out);
}

std::int64_t k1_compare_between_bitmap(const double* in, std::int64_t n, double lo, double hi,
                                       const std::uint8_t* validity, std::uint8_t* out) noexcept {
  return scalar_impl::compare_between_bitmap<double>({in, n, validity}, lo, hi, out);
}

std::int64_t k1_compare_selvec(CompareOp op, const double* in, std::int64_t n, double comparand,
                               const std::uint8_t* validity, std::uint32_t* out) noexcept {
  return scalar_impl::compare_selvec<double>(op, {in, n, validity}, comparand, out);
}

std::int64_t k1_compare_selvec2(CompareOp op, const double* a, const double* b, std::int64_t n,
                                const std::uint8_t* a_validity, const std::uint8_t* b_validity,
                                std::uint32_t* out) noexcept {
  return scalar_impl::compare_selvec2<double>(op, {a, n, a_validity}, {b, n, b_validity}, out);
}

std::int64_t k1_compare_between_selvec(const double* in, std::int64_t n, double lo, double hi,
                                       const std::uint8_t* validity, std::uint32_t* out) noexcept {
  return scalar_impl::compare_between_selvec<double>({in, n, validity}, lo, hi, out);
}

}  // namespace detail::scalar
QUIVER_END_NAMESPACE
