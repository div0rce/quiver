// K5 take — concrete scalar backends: one symbol per admissible template-parameter
// combination (ADR-006), each a thin instantiation of the take_scalar_impl.h reference.
// GENERATED-BY-HAND-STABLE: regenerate only with a documented spec change (REQ-KERNEL-005).
// Module: MOD-K5-TAKE | REQs: REQ-KERNEL-001/-005 | ADR-006
#include "src/kernels/take/take_scalar_impl.h"

QUIVER_BEGIN_NAMESPACE
namespace detail::scalar {

void k5_take(const std::int8_t* values, std::int64_t values_len, const std::uint32_t* idx, std::int64_t idx_len, std::int8_t* out) noexcept {
  return scalar_impl::take<std::int8_t>(values, values_len, idx, idx_len, out);
}

void k5_dict_decode(const std::int8_t* dict, std::int64_t dict_len, const std::uint8_t* codes, std::int64_t n, const std::uint32_t* sel, std::int64_t sel_len, std::int8_t* out) noexcept {
  return sel == nullptr ? scalar_impl::dict_decode<std::int8_t, std::uint8_t>(dict, dict_len, codes, n, out) : scalar_impl::dict_decode_sel<std::int8_t, std::uint8_t>(dict, dict_len, codes, sel, sel_len, out);
}

void k5_dict_decode(const std::int8_t* dict, std::int64_t dict_len, const std::uint16_t* codes, std::int64_t n, const std::uint32_t* sel, std::int64_t sel_len, std::int8_t* out) noexcept {
  return sel == nullptr ? scalar_impl::dict_decode<std::int8_t, std::uint16_t>(dict, dict_len, codes, n, out) : scalar_impl::dict_decode_sel<std::int8_t, std::uint16_t>(dict, dict_len, codes, sel, sel_len, out);
}

void k5_dict_decode(const std::int8_t* dict, std::int64_t dict_len, const std::uint32_t* codes, std::int64_t n, const std::uint32_t* sel, std::int64_t sel_len, std::int8_t* out) noexcept {
  return sel == nullptr ? scalar_impl::dict_decode<std::int8_t, std::uint32_t>(dict, dict_len, codes, n, out) : scalar_impl::dict_decode_sel<std::int8_t, std::uint32_t>(dict, dict_len, codes, sel, sel_len, out);
}

void k5_take(const std::int16_t* values, std::int64_t values_len, const std::uint32_t* idx, std::int64_t idx_len, std::int16_t* out) noexcept {
  return scalar_impl::take<std::int16_t>(values, values_len, idx, idx_len, out);
}

void k5_dict_decode(const std::int16_t* dict, std::int64_t dict_len, const std::uint8_t* codes, std::int64_t n, const std::uint32_t* sel, std::int64_t sel_len, std::int16_t* out) noexcept {
  return sel == nullptr ? scalar_impl::dict_decode<std::int16_t, std::uint8_t>(dict, dict_len, codes, n, out) : scalar_impl::dict_decode_sel<std::int16_t, std::uint8_t>(dict, dict_len, codes, sel, sel_len, out);
}

void k5_dict_decode(const std::int16_t* dict, std::int64_t dict_len, const std::uint16_t* codes, std::int64_t n, const std::uint32_t* sel, std::int64_t sel_len, std::int16_t* out) noexcept {
  return sel == nullptr ? scalar_impl::dict_decode<std::int16_t, std::uint16_t>(dict, dict_len, codes, n, out) : scalar_impl::dict_decode_sel<std::int16_t, std::uint16_t>(dict, dict_len, codes, sel, sel_len, out);
}

void k5_dict_decode(const std::int16_t* dict, std::int64_t dict_len, const std::uint32_t* codes, std::int64_t n, const std::uint32_t* sel, std::int64_t sel_len, std::int16_t* out) noexcept {
  return sel == nullptr ? scalar_impl::dict_decode<std::int16_t, std::uint32_t>(dict, dict_len, codes, n, out) : scalar_impl::dict_decode_sel<std::int16_t, std::uint32_t>(dict, dict_len, codes, sel, sel_len, out);
}

void k5_take(const std::int32_t* values, std::int64_t values_len, const std::uint32_t* idx, std::int64_t idx_len, std::int32_t* out) noexcept {
  return scalar_impl::take<std::int32_t>(values, values_len, idx, idx_len, out);
}

void k5_dict_decode(const std::int32_t* dict, std::int64_t dict_len, const std::uint8_t* codes, std::int64_t n, const std::uint32_t* sel, std::int64_t sel_len, std::int32_t* out) noexcept {
  return sel == nullptr ? scalar_impl::dict_decode<std::int32_t, std::uint8_t>(dict, dict_len, codes, n, out) : scalar_impl::dict_decode_sel<std::int32_t, std::uint8_t>(dict, dict_len, codes, sel, sel_len, out);
}

void k5_dict_decode(const std::int32_t* dict, std::int64_t dict_len, const std::uint16_t* codes, std::int64_t n, const std::uint32_t* sel, std::int64_t sel_len, std::int32_t* out) noexcept {
  return sel == nullptr ? scalar_impl::dict_decode<std::int32_t, std::uint16_t>(dict, dict_len, codes, n, out) : scalar_impl::dict_decode_sel<std::int32_t, std::uint16_t>(dict, dict_len, codes, sel, sel_len, out);
}

void k5_dict_decode(const std::int32_t* dict, std::int64_t dict_len, const std::uint32_t* codes, std::int64_t n, const std::uint32_t* sel, std::int64_t sel_len, std::int32_t* out) noexcept {
  return sel == nullptr ? scalar_impl::dict_decode<std::int32_t, std::uint32_t>(dict, dict_len, codes, n, out) : scalar_impl::dict_decode_sel<std::int32_t, std::uint32_t>(dict, dict_len, codes, sel, sel_len, out);
}

void k5_take(const std::int64_t* values, std::int64_t values_len, const std::uint32_t* idx, std::int64_t idx_len, std::int64_t* out) noexcept {
  return scalar_impl::take<std::int64_t>(values, values_len, idx, idx_len, out);
}

void k5_dict_decode(const std::int64_t* dict, std::int64_t dict_len, const std::uint8_t* codes, std::int64_t n, const std::uint32_t* sel, std::int64_t sel_len, std::int64_t* out) noexcept {
  return sel == nullptr ? scalar_impl::dict_decode<std::int64_t, std::uint8_t>(dict, dict_len, codes, n, out) : scalar_impl::dict_decode_sel<std::int64_t, std::uint8_t>(dict, dict_len, codes, sel, sel_len, out);
}

void k5_dict_decode(const std::int64_t* dict, std::int64_t dict_len, const std::uint16_t* codes, std::int64_t n, const std::uint32_t* sel, std::int64_t sel_len, std::int64_t* out) noexcept {
  return sel == nullptr ? scalar_impl::dict_decode<std::int64_t, std::uint16_t>(dict, dict_len, codes, n, out) : scalar_impl::dict_decode_sel<std::int64_t, std::uint16_t>(dict, dict_len, codes, sel, sel_len, out);
}

void k5_dict_decode(const std::int64_t* dict, std::int64_t dict_len, const std::uint32_t* codes, std::int64_t n, const std::uint32_t* sel, std::int64_t sel_len, std::int64_t* out) noexcept {
  return sel == nullptr ? scalar_impl::dict_decode<std::int64_t, std::uint32_t>(dict, dict_len, codes, n, out) : scalar_impl::dict_decode_sel<std::int64_t, std::uint32_t>(dict, dict_len, codes, sel, sel_len, out);
}

void k5_take(const std::uint8_t* values, std::int64_t values_len, const std::uint32_t* idx, std::int64_t idx_len, std::uint8_t* out) noexcept {
  return scalar_impl::take<std::uint8_t>(values, values_len, idx, idx_len, out);
}

void k5_dict_decode(const std::uint8_t* dict, std::int64_t dict_len, const std::uint8_t* codes, std::int64_t n, const std::uint32_t* sel, std::int64_t sel_len, std::uint8_t* out) noexcept {
  return sel == nullptr ? scalar_impl::dict_decode<std::uint8_t, std::uint8_t>(dict, dict_len, codes, n, out) : scalar_impl::dict_decode_sel<std::uint8_t, std::uint8_t>(dict, dict_len, codes, sel, sel_len, out);
}

void k5_dict_decode(const std::uint8_t* dict, std::int64_t dict_len, const std::uint16_t* codes, std::int64_t n, const std::uint32_t* sel, std::int64_t sel_len, std::uint8_t* out) noexcept {
  return sel == nullptr ? scalar_impl::dict_decode<std::uint8_t, std::uint16_t>(dict, dict_len, codes, n, out) : scalar_impl::dict_decode_sel<std::uint8_t, std::uint16_t>(dict, dict_len, codes, sel, sel_len, out);
}

void k5_dict_decode(const std::uint8_t* dict, std::int64_t dict_len, const std::uint32_t* codes, std::int64_t n, const std::uint32_t* sel, std::int64_t sel_len, std::uint8_t* out) noexcept {
  return sel == nullptr ? scalar_impl::dict_decode<std::uint8_t, std::uint32_t>(dict, dict_len, codes, n, out) : scalar_impl::dict_decode_sel<std::uint8_t, std::uint32_t>(dict, dict_len, codes, sel, sel_len, out);
}

void k5_take(const std::uint16_t* values, std::int64_t values_len, const std::uint32_t* idx, std::int64_t idx_len, std::uint16_t* out) noexcept {
  return scalar_impl::take<std::uint16_t>(values, values_len, idx, idx_len, out);
}

void k5_dict_decode(const std::uint16_t* dict, std::int64_t dict_len, const std::uint8_t* codes, std::int64_t n, const std::uint32_t* sel, std::int64_t sel_len, std::uint16_t* out) noexcept {
  return sel == nullptr ? scalar_impl::dict_decode<std::uint16_t, std::uint8_t>(dict, dict_len, codes, n, out) : scalar_impl::dict_decode_sel<std::uint16_t, std::uint8_t>(dict, dict_len, codes, sel, sel_len, out);
}

void k5_dict_decode(const std::uint16_t* dict, std::int64_t dict_len, const std::uint16_t* codes, std::int64_t n, const std::uint32_t* sel, std::int64_t sel_len, std::uint16_t* out) noexcept {
  return sel == nullptr ? scalar_impl::dict_decode<std::uint16_t, std::uint16_t>(dict, dict_len, codes, n, out) : scalar_impl::dict_decode_sel<std::uint16_t, std::uint16_t>(dict, dict_len, codes, sel, sel_len, out);
}

void k5_dict_decode(const std::uint16_t* dict, std::int64_t dict_len, const std::uint32_t* codes, std::int64_t n, const std::uint32_t* sel, std::int64_t sel_len, std::uint16_t* out) noexcept {
  return sel == nullptr ? scalar_impl::dict_decode<std::uint16_t, std::uint32_t>(dict, dict_len, codes, n, out) : scalar_impl::dict_decode_sel<std::uint16_t, std::uint32_t>(dict, dict_len, codes, sel, sel_len, out);
}

void k5_take(const std::uint32_t* values, std::int64_t values_len, const std::uint32_t* idx, std::int64_t idx_len, std::uint32_t* out) noexcept {
  return scalar_impl::take<std::uint32_t>(values, values_len, idx, idx_len, out);
}

void k5_dict_decode(const std::uint32_t* dict, std::int64_t dict_len, const std::uint8_t* codes, std::int64_t n, const std::uint32_t* sel, std::int64_t sel_len, std::uint32_t* out) noexcept {
  return sel == nullptr ? scalar_impl::dict_decode<std::uint32_t, std::uint8_t>(dict, dict_len, codes, n, out) : scalar_impl::dict_decode_sel<std::uint32_t, std::uint8_t>(dict, dict_len, codes, sel, sel_len, out);
}

void k5_dict_decode(const std::uint32_t* dict, std::int64_t dict_len, const std::uint16_t* codes, std::int64_t n, const std::uint32_t* sel, std::int64_t sel_len, std::uint32_t* out) noexcept {
  return sel == nullptr ? scalar_impl::dict_decode<std::uint32_t, std::uint16_t>(dict, dict_len, codes, n, out) : scalar_impl::dict_decode_sel<std::uint32_t, std::uint16_t>(dict, dict_len, codes, sel, sel_len, out);
}

void k5_dict_decode(const std::uint32_t* dict, std::int64_t dict_len, const std::uint32_t* codes, std::int64_t n, const std::uint32_t* sel, std::int64_t sel_len, std::uint32_t* out) noexcept {
  return sel == nullptr ? scalar_impl::dict_decode<std::uint32_t, std::uint32_t>(dict, dict_len, codes, n, out) : scalar_impl::dict_decode_sel<std::uint32_t, std::uint32_t>(dict, dict_len, codes, sel, sel_len, out);
}

void k5_take(const std::uint64_t* values, std::int64_t values_len, const std::uint32_t* idx, std::int64_t idx_len, std::uint64_t* out) noexcept {
  return scalar_impl::take<std::uint64_t>(values, values_len, idx, idx_len, out);
}

void k5_dict_decode(const std::uint64_t* dict, std::int64_t dict_len, const std::uint8_t* codes, std::int64_t n, const std::uint32_t* sel, std::int64_t sel_len, std::uint64_t* out) noexcept {
  return sel == nullptr ? scalar_impl::dict_decode<std::uint64_t, std::uint8_t>(dict, dict_len, codes, n, out) : scalar_impl::dict_decode_sel<std::uint64_t, std::uint8_t>(dict, dict_len, codes, sel, sel_len, out);
}

void k5_dict_decode(const std::uint64_t* dict, std::int64_t dict_len, const std::uint16_t* codes, std::int64_t n, const std::uint32_t* sel, std::int64_t sel_len, std::uint64_t* out) noexcept {
  return sel == nullptr ? scalar_impl::dict_decode<std::uint64_t, std::uint16_t>(dict, dict_len, codes, n, out) : scalar_impl::dict_decode_sel<std::uint64_t, std::uint16_t>(dict, dict_len, codes, sel, sel_len, out);
}

void k5_dict_decode(const std::uint64_t* dict, std::int64_t dict_len, const std::uint32_t* codes, std::int64_t n, const std::uint32_t* sel, std::int64_t sel_len, std::uint64_t* out) noexcept {
  return sel == nullptr ? scalar_impl::dict_decode<std::uint64_t, std::uint32_t>(dict, dict_len, codes, n, out) : scalar_impl::dict_decode_sel<std::uint64_t, std::uint32_t>(dict, dict_len, codes, sel, sel_len, out);
}

void k5_take(const float* values, std::int64_t values_len, const std::uint32_t* idx, std::int64_t idx_len, float* out) noexcept {
  return scalar_impl::take<float>(values, values_len, idx, idx_len, out);
}

void k5_dict_decode(const float* dict, std::int64_t dict_len, const std::uint8_t* codes, std::int64_t n, const std::uint32_t* sel, std::int64_t sel_len, float* out) noexcept {
  return sel == nullptr ? scalar_impl::dict_decode<float, std::uint8_t>(dict, dict_len, codes, n, out) : scalar_impl::dict_decode_sel<float, std::uint8_t>(dict, dict_len, codes, sel, sel_len, out);
}

void k5_dict_decode(const float* dict, std::int64_t dict_len, const std::uint16_t* codes, std::int64_t n, const std::uint32_t* sel, std::int64_t sel_len, float* out) noexcept {
  return sel == nullptr ? scalar_impl::dict_decode<float, std::uint16_t>(dict, dict_len, codes, n, out) : scalar_impl::dict_decode_sel<float, std::uint16_t>(dict, dict_len, codes, sel, sel_len, out);
}

void k5_dict_decode(const float* dict, std::int64_t dict_len, const std::uint32_t* codes, std::int64_t n, const std::uint32_t* sel, std::int64_t sel_len, float* out) noexcept {
  return sel == nullptr ? scalar_impl::dict_decode<float, std::uint32_t>(dict, dict_len, codes, n, out) : scalar_impl::dict_decode_sel<float, std::uint32_t>(dict, dict_len, codes, sel, sel_len, out);
}

void k5_take(const double* values, std::int64_t values_len, const std::uint32_t* idx, std::int64_t idx_len, double* out) noexcept {
  return scalar_impl::take<double>(values, values_len, idx, idx_len, out);
}

void k5_dict_decode(const double* dict, std::int64_t dict_len, const std::uint8_t* codes, std::int64_t n, const std::uint32_t* sel, std::int64_t sel_len, double* out) noexcept {
  return sel == nullptr ? scalar_impl::dict_decode<double, std::uint8_t>(dict, dict_len, codes, n, out) : scalar_impl::dict_decode_sel<double, std::uint8_t>(dict, dict_len, codes, sel, sel_len, out);
}

void k5_dict_decode(const double* dict, std::int64_t dict_len, const std::uint16_t* codes, std::int64_t n, const std::uint32_t* sel, std::int64_t sel_len, double* out) noexcept {
  return sel == nullptr ? scalar_impl::dict_decode<double, std::uint16_t>(dict, dict_len, codes, n, out) : scalar_impl::dict_decode_sel<double, std::uint16_t>(dict, dict_len, codes, sel, sel_len, out);
}

void k5_dict_decode(const double* dict, std::int64_t dict_len, const std::uint32_t* codes, std::int64_t n, const std::uint32_t* sel, std::int64_t sel_len, double* out) noexcept {
  return sel == nullptr ? scalar_impl::dict_decode<double, std::uint32_t>(dict, dict_len, codes, n, out) : scalar_impl::dict_decode_sel<double, std::uint32_t>(dict, dict_len, codes, sel, sel_len, out);
}

}  // namespace detail::scalar
QUIVER_END_NAMESPACE
