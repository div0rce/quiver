// K6 reduce — concrete scalar backends: one symbol per admissible template-parameter
// combination (ADR-006), each a thin instantiation of the reduce_scalar_impl.h reference.
// GENERATED-BY-HAND-STABLE: regenerate only with a documented spec change (REQ-KERNEL-005).
// Module: MOD-K6-REDUCE | REQs: REQ-KERNEL-001/-005 | ADR-006
#include "src/kernels/reduce/reduce_scalar_impl.h"

QUIVER_BEGIN_NAMESPACE
namespace detail::scalar {

std::int8_t k6_reduce_min(const std::int8_t* in, std::int64_t n, const std::uint8_t* validity,
                          const std::uint32_t* sel, std::int64_t sel_len) noexcept {
  return scalar_impl::reduce_min<std::int8_t>(in, n, validity, sel, sel_len);
}

std::int8_t k6_reduce_max(const std::int8_t* in, std::int64_t n, const std::uint8_t* validity,
                          const std::uint32_t* sel, std::int64_t sel_len) noexcept {
  return scalar_impl::reduce_max<std::int8_t>(in, n, validity, sel, sel_len);
}

std::int64_t k6_reduce_sum_wrap(const std::int8_t* in, std::int64_t n, const std::uint8_t* validity,
                                const std::uint32_t* sel, std::int64_t sel_len) noexcept {
  return scalar_impl::reduce_sum_wrap<std::int8_t>(in, n, validity, sel, sel_len);
}

Sma<std::int8_t> k6_compute_sma(const std::int8_t* in, std::int64_t n, const std::uint8_t* validity,
                                const std::uint32_t* sel, std::int64_t sel_len) noexcept {
  return scalar_impl::compute_sma<std::int8_t>(in, n, validity, sel, sel_len);
}

std::int16_t k6_reduce_min(const std::int16_t* in, std::int64_t n, const std::uint8_t* validity,
                           const std::uint32_t* sel, std::int64_t sel_len) noexcept {
  return scalar_impl::reduce_min<std::int16_t>(in, n, validity, sel, sel_len);
}

std::int16_t k6_reduce_max(const std::int16_t* in, std::int64_t n, const std::uint8_t* validity,
                           const std::uint32_t* sel, std::int64_t sel_len) noexcept {
  return scalar_impl::reduce_max<std::int16_t>(in, n, validity, sel, sel_len);
}

std::int64_t k6_reduce_sum_wrap(const std::int16_t* in, std::int64_t n,
                                const std::uint8_t* validity, const std::uint32_t* sel,
                                std::int64_t sel_len) noexcept {
  return scalar_impl::reduce_sum_wrap<std::int16_t>(in, n, validity, sel, sel_len);
}

Sma<std::int16_t> k6_compute_sma(const std::int16_t* in, std::int64_t n,
                                 const std::uint8_t* validity, const std::uint32_t* sel,
                                 std::int64_t sel_len) noexcept {
  return scalar_impl::compute_sma<std::int16_t>(in, n, validity, sel, sel_len);
}

std::int32_t k6_reduce_min(const std::int32_t* in, std::int64_t n, const std::uint8_t* validity,
                           const std::uint32_t* sel, std::int64_t sel_len) noexcept {
  return scalar_impl::reduce_min<std::int32_t>(in, n, validity, sel, sel_len);
}

std::int32_t k6_reduce_max(const std::int32_t* in, std::int64_t n, const std::uint8_t* validity,
                           const std::uint32_t* sel, std::int64_t sel_len) noexcept {
  return scalar_impl::reduce_max<std::int32_t>(in, n, validity, sel, sel_len);
}

std::int64_t k6_reduce_sum_wrap(const std::int32_t* in, std::int64_t n,
                                const std::uint8_t* validity, const std::uint32_t* sel,
                                std::int64_t sel_len) noexcept {
  return scalar_impl::reduce_sum_wrap<std::int32_t>(in, n, validity, sel, sel_len);
}

Sma<std::int32_t> k6_compute_sma(const std::int32_t* in, std::int64_t n,
                                 const std::uint8_t* validity, const std::uint32_t* sel,
                                 std::int64_t sel_len) noexcept {
  return scalar_impl::compute_sma<std::int32_t>(in, n, validity, sel, sel_len);
}

std::int64_t k6_reduce_min(const std::int64_t* in, std::int64_t n, const std::uint8_t* validity,
                           const std::uint32_t* sel, std::int64_t sel_len) noexcept {
  return scalar_impl::reduce_min<std::int64_t>(in, n, validity, sel, sel_len);
}

std::int64_t k6_reduce_max(const std::int64_t* in, std::int64_t n, const std::uint8_t* validity,
                           const std::uint32_t* sel, std::int64_t sel_len) noexcept {
  return scalar_impl::reduce_max<std::int64_t>(in, n, validity, sel, sel_len);
}

std::int64_t k6_reduce_sum_wrap(const std::int64_t* in, std::int64_t n,
                                const std::uint8_t* validity, const std::uint32_t* sel,
                                std::int64_t sel_len) noexcept {
  return scalar_impl::reduce_sum_wrap<std::int64_t>(in, n, validity, sel, sel_len);
}

Sma<std::int64_t> k6_compute_sma(const std::int64_t* in, std::int64_t n,
                                 const std::uint8_t* validity, const std::uint32_t* sel,
                                 std::int64_t sel_len) noexcept {
  return scalar_impl::compute_sma<std::int64_t>(in, n, validity, sel, sel_len);
}

std::uint8_t k6_reduce_min(const std::uint8_t* in, std::int64_t n, const std::uint8_t* validity,
                           const std::uint32_t* sel, std::int64_t sel_len) noexcept {
  return scalar_impl::reduce_min<std::uint8_t>(in, n, validity, sel, sel_len);
}

std::uint8_t k6_reduce_max(const std::uint8_t* in, std::int64_t n, const std::uint8_t* validity,
                           const std::uint32_t* sel, std::int64_t sel_len) noexcept {
  return scalar_impl::reduce_max<std::uint8_t>(in, n, validity, sel, sel_len);
}

std::uint64_t k6_reduce_sum_wrap(const std::uint8_t* in, std::int64_t n,
                                 const std::uint8_t* validity, const std::uint32_t* sel,
                                 std::int64_t sel_len) noexcept {
  return scalar_impl::reduce_sum_wrap<std::uint8_t>(in, n, validity, sel, sel_len);
}

Sma<std::uint8_t> k6_compute_sma(const std::uint8_t* in, std::int64_t n,
                                 const std::uint8_t* validity, const std::uint32_t* sel,
                                 std::int64_t sel_len) noexcept {
  return scalar_impl::compute_sma<std::uint8_t>(in, n, validity, sel, sel_len);
}

std::uint16_t k6_reduce_min(const std::uint16_t* in, std::int64_t n, const std::uint8_t* validity,
                            const std::uint32_t* sel, std::int64_t sel_len) noexcept {
  return scalar_impl::reduce_min<std::uint16_t>(in, n, validity, sel, sel_len);
}

std::uint16_t k6_reduce_max(const std::uint16_t* in, std::int64_t n, const std::uint8_t* validity,
                            const std::uint32_t* sel, std::int64_t sel_len) noexcept {
  return scalar_impl::reduce_max<std::uint16_t>(in, n, validity, sel, sel_len);
}

std::uint64_t k6_reduce_sum_wrap(const std::uint16_t* in, std::int64_t n,
                                 const std::uint8_t* validity, const std::uint32_t* sel,
                                 std::int64_t sel_len) noexcept {
  return scalar_impl::reduce_sum_wrap<std::uint16_t>(in, n, validity, sel, sel_len);
}

Sma<std::uint16_t> k6_compute_sma(const std::uint16_t* in, std::int64_t n,
                                  const std::uint8_t* validity, const std::uint32_t* sel,
                                  std::int64_t sel_len) noexcept {
  return scalar_impl::compute_sma<std::uint16_t>(in, n, validity, sel, sel_len);
}

std::uint32_t k6_reduce_min(const std::uint32_t* in, std::int64_t n, const std::uint8_t* validity,
                            const std::uint32_t* sel, std::int64_t sel_len) noexcept {
  return scalar_impl::reduce_min<std::uint32_t>(in, n, validity, sel, sel_len);
}

std::uint32_t k6_reduce_max(const std::uint32_t* in, std::int64_t n, const std::uint8_t* validity,
                            const std::uint32_t* sel, std::int64_t sel_len) noexcept {
  return scalar_impl::reduce_max<std::uint32_t>(in, n, validity, sel, sel_len);
}

std::uint64_t k6_reduce_sum_wrap(const std::uint32_t* in, std::int64_t n,
                                 const std::uint8_t* validity, const std::uint32_t* sel,
                                 std::int64_t sel_len) noexcept {
  return scalar_impl::reduce_sum_wrap<std::uint32_t>(in, n, validity, sel, sel_len);
}

Sma<std::uint32_t> k6_compute_sma(const std::uint32_t* in, std::int64_t n,
                                  const std::uint8_t* validity, const std::uint32_t* sel,
                                  std::int64_t sel_len) noexcept {
  return scalar_impl::compute_sma<std::uint32_t>(in, n, validity, sel, sel_len);
}

std::uint64_t k6_reduce_min(const std::uint64_t* in, std::int64_t n, const std::uint8_t* validity,
                            const std::uint32_t* sel, std::int64_t sel_len) noexcept {
  return scalar_impl::reduce_min<std::uint64_t>(in, n, validity, sel, sel_len);
}

std::uint64_t k6_reduce_max(const std::uint64_t* in, std::int64_t n, const std::uint8_t* validity,
                            const std::uint32_t* sel, std::int64_t sel_len) noexcept {
  return scalar_impl::reduce_max<std::uint64_t>(in, n, validity, sel, sel_len);
}

std::uint64_t k6_reduce_sum_wrap(const std::uint64_t* in, std::int64_t n,
                                 const std::uint8_t* validity, const std::uint32_t* sel,
                                 std::int64_t sel_len) noexcept {
  return scalar_impl::reduce_sum_wrap<std::uint64_t>(in, n, validity, sel, sel_len);
}

Sma<std::uint64_t> k6_compute_sma(const std::uint64_t* in, std::int64_t n,
                                  const std::uint8_t* validity, const std::uint32_t* sel,
                                  std::int64_t sel_len) noexcept {
  return scalar_impl::compute_sma<std::uint64_t>(in, n, validity, sel, sel_len);
}

float k6_reduce_min(const float* in, std::int64_t n, const std::uint8_t* validity,
                    const std::uint32_t* sel, std::int64_t sel_len) noexcept {
  return scalar_impl::reduce_min<float>(in, n, validity, sel, sel_len);
}

float k6_reduce_max(const float* in, std::int64_t n, const std::uint8_t* validity,
                    const std::uint32_t* sel, std::int64_t sel_len) noexcept {
  return scalar_impl::reduce_max<float>(in, n, validity, sel, sel_len);
}

float k6_reduce_sum_wrap(const float* in, std::int64_t n, const std::uint8_t* validity,
                         const std::uint32_t* sel, std::int64_t sel_len) noexcept {
  return scalar_impl::reduce_sum_wrap<float>(in, n, validity, sel, sel_len);
}

Sma<float> k6_compute_sma(const float* in, std::int64_t n, const std::uint8_t* validity,
                          const std::uint32_t* sel, std::int64_t sel_len) noexcept {
  return scalar_impl::compute_sma<float>(in, n, validity, sel, sel_len);
}

double k6_reduce_min(const double* in, std::int64_t n, const std::uint8_t* validity,
                     const std::uint32_t* sel, std::int64_t sel_len) noexcept {
  return scalar_impl::reduce_min<double>(in, n, validity, sel, sel_len);
}

double k6_reduce_max(const double* in, std::int64_t n, const std::uint8_t* validity,
                     const std::uint32_t* sel, std::int64_t sel_len) noexcept {
  return scalar_impl::reduce_max<double>(in, n, validity, sel, sel_len);
}

double k6_reduce_sum_wrap(const double* in, std::int64_t n, const std::uint8_t* validity,
                          const std::uint32_t* sel, std::int64_t sel_len) noexcept {
  return scalar_impl::reduce_sum_wrap<double>(in, n, validity, sel, sel_len);
}

Sma<double> k6_compute_sma(const double* in, std::int64_t n, const std::uint8_t* validity,
                           const std::uint32_t* sel, std::int64_t sel_len) noexcept {
  return scalar_impl::compute_sma<double>(in, n, validity, sel, sel_len);
}

bool k6_reduce_sum_checked(const std::int8_t* in, std::int64_t n, const std::uint8_t* validity,
                           const std::uint32_t* sel, std::int64_t sel_len,
                           std::int64_t* out_sum) noexcept {
  return scalar_impl::reduce_sum_checked<std::int8_t>(in, n, validity, sel, sel_len, out_sum);
}

bool k6_reduce_sum_checked(const std::int16_t* in, std::int64_t n, const std::uint8_t* validity,
                           const std::uint32_t* sel, std::int64_t sel_len,
                           std::int64_t* out_sum) noexcept {
  return scalar_impl::reduce_sum_checked<std::int16_t>(in, n, validity, sel, sel_len, out_sum);
}

bool k6_reduce_sum_checked(const std::int32_t* in, std::int64_t n, const std::uint8_t* validity,
                           const std::uint32_t* sel, std::int64_t sel_len,
                           std::int64_t* out_sum) noexcept {
  return scalar_impl::reduce_sum_checked<std::int32_t>(in, n, validity, sel, sel_len, out_sum);
}

bool k6_reduce_sum_checked(const std::int64_t* in, std::int64_t n, const std::uint8_t* validity,
                           const std::uint32_t* sel, std::int64_t sel_len,
                           std::int64_t* out_sum) noexcept {
  return scalar_impl::reduce_sum_checked<std::int64_t>(in, n, validity, sel, sel_len, out_sum);
}

bool k6_reduce_sum_checked(const std::uint8_t* in, std::int64_t n, const std::uint8_t* validity,
                           const std::uint32_t* sel, std::int64_t sel_len,
                           std::uint64_t* out_sum) noexcept {
  return scalar_impl::reduce_sum_checked<std::uint8_t>(in, n, validity, sel, sel_len, out_sum);
}

bool k6_reduce_sum_checked(const std::uint16_t* in, std::int64_t n, const std::uint8_t* validity,
                           const std::uint32_t* sel, std::int64_t sel_len,
                           std::uint64_t* out_sum) noexcept {
  return scalar_impl::reduce_sum_checked<std::uint16_t>(in, n, validity, sel, sel_len, out_sum);
}

bool k6_reduce_sum_checked(const std::uint32_t* in, std::int64_t n, const std::uint8_t* validity,
                           const std::uint32_t* sel, std::int64_t sel_len,
                           std::uint64_t* out_sum) noexcept {
  return scalar_impl::reduce_sum_checked<std::uint32_t>(in, n, validity, sel, sel_len, out_sum);
}

bool k6_reduce_sum_checked(const std::uint64_t* in, std::int64_t n, const std::uint8_t* validity,
                           const std::uint32_t* sel, std::int64_t sel_len,
                           std::uint64_t* out_sum) noexcept {
  return scalar_impl::reduce_sum_checked<std::uint64_t>(in, n, validity, sel, sel_len, out_sum);
}

}  // namespace detail::scalar
QUIVER_END_NAMESPACE
