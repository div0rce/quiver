// K2 filter — concrete scalar backends: one symbol per admissible template-parameter
// combination (ADR-006), each a thin instantiation of the filter_scalar_impl.h reference.
// GENERATED-BY-HAND-STABLE: regenerate only with a documented spec change (REQ-KERNEL-005).
// Module: MOD-K2-FILTER | REQs: REQ-KERNEL-001/-005 | ADR-006
#include "src/kernels/filter/filter_scalar_impl.h"

QUIVER_BEGIN_NAMESPACE
namespace detail::scalar {

std::int64_t k2_filter_bitmap(const std::int8_t* in, std::int64_t n, const std::uint8_t* selection,
                              std::int8_t* out) noexcept {
  return scalar_impl::filter_bitmap<std::int8_t>(in, n, selection, out);
}

std::int64_t k2_filter_selvec(const std::int8_t* in, const std::uint32_t* sel, std::int64_t sel_len,
                              std::int8_t* out) noexcept {
  return scalar_impl::filter_selvec<std::int8_t>(in, sel, sel_len, out);
}

std::int64_t k2_filter_bitmap(const std::int16_t* in, std::int64_t n, const std::uint8_t* selection,
                              std::int16_t* out) noexcept {
  return scalar_impl::filter_bitmap<std::int16_t>(in, n, selection, out);
}

std::int64_t k2_filter_selvec(const std::int16_t* in, const std::uint32_t* sel,
                              std::int64_t sel_len, std::int16_t* out) noexcept {
  return scalar_impl::filter_selvec<std::int16_t>(in, sel, sel_len, out);
}

std::int64_t k2_filter_bitmap(const std::int32_t* in, std::int64_t n, const std::uint8_t* selection,
                              std::int32_t* out) noexcept {
  return scalar_impl::filter_bitmap<std::int32_t>(in, n, selection, out);
}

std::int64_t k2_filter_selvec(const std::int32_t* in, const std::uint32_t* sel,
                              std::int64_t sel_len, std::int32_t* out) noexcept {
  return scalar_impl::filter_selvec<std::int32_t>(in, sel, sel_len, out);
}

std::int64_t k2_filter_bitmap(const std::int64_t* in, std::int64_t n, const std::uint8_t* selection,
                              std::int64_t* out) noexcept {
  return scalar_impl::filter_bitmap<std::int64_t>(in, n, selection, out);
}

std::int64_t k2_filter_selvec(const std::int64_t* in, const std::uint32_t* sel,
                              std::int64_t sel_len, std::int64_t* out) noexcept {
  return scalar_impl::filter_selvec<std::int64_t>(in, sel, sel_len, out);
}

std::int64_t k2_filter_bitmap(const std::uint8_t* in, std::int64_t n, const std::uint8_t* selection,
                              std::uint8_t* out) noexcept {
  return scalar_impl::filter_bitmap<std::uint8_t>(in, n, selection, out);
}

std::int64_t k2_filter_selvec(const std::uint8_t* in, const std::uint32_t* sel,
                              std::int64_t sel_len, std::uint8_t* out) noexcept {
  return scalar_impl::filter_selvec<std::uint8_t>(in, sel, sel_len, out);
}

std::int64_t k2_filter_bitmap(const std::uint16_t* in, std::int64_t n,
                              const std::uint8_t* selection, std::uint16_t* out) noexcept {
  return scalar_impl::filter_bitmap<std::uint16_t>(in, n, selection, out);
}

std::int64_t k2_filter_selvec(const std::uint16_t* in, const std::uint32_t* sel,
                              std::int64_t sel_len, std::uint16_t* out) noexcept {
  return scalar_impl::filter_selvec<std::uint16_t>(in, sel, sel_len, out);
}

std::int64_t k2_filter_bitmap(const std::uint32_t* in, std::int64_t n,
                              const std::uint8_t* selection, std::uint32_t* out) noexcept {
  return scalar_impl::filter_bitmap<std::uint32_t>(in, n, selection, out);
}

std::int64_t k2_filter_selvec(const std::uint32_t* in, const std::uint32_t* sel,
                              std::int64_t sel_len, std::uint32_t* out) noexcept {
  return scalar_impl::filter_selvec<std::uint32_t>(in, sel, sel_len, out);
}

std::int64_t k2_filter_bitmap(const std::uint64_t* in, std::int64_t n,
                              const std::uint8_t* selection, std::uint64_t* out) noexcept {
  return scalar_impl::filter_bitmap<std::uint64_t>(in, n, selection, out);
}

std::int64_t k2_filter_selvec(const std::uint64_t* in, const std::uint32_t* sel,
                              std::int64_t sel_len, std::uint64_t* out) noexcept {
  return scalar_impl::filter_selvec<std::uint64_t>(in, sel, sel_len, out);
}

std::int64_t k2_filter_bitmap(const float* in, std::int64_t n, const std::uint8_t* selection,
                              float* out) noexcept {
  return scalar_impl::filter_bitmap<float>(in, n, selection, out);
}

std::int64_t k2_filter_selvec(const float* in, const std::uint32_t* sel, std::int64_t sel_len,
                              float* out) noexcept {
  return scalar_impl::filter_selvec<float>(in, sel, sel_len, out);
}

std::int64_t k2_filter_bitmap(const double* in, std::int64_t n, const std::uint8_t* selection,
                              double* out) noexcept {
  return scalar_impl::filter_bitmap<double>(in, n, selection, out);
}

std::int64_t k2_filter_selvec(const double* in, const std::uint32_t* sel, std::int64_t sel_len,
                              double* out) noexcept {
  return scalar_impl::filter_selvec<double>(in, sel, sel_len, out);
}

}  // namespace detail::scalar
QUIVER_END_NAMESPACE
