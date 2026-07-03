// K4 mask — concrete scalar backends: one symbol per admissible template-parameter
// combination (ADR-006), each a thin instantiation of the mask_scalar_impl.h reference.
// GENERATED-BY-HAND-STABLE: regenerate only with a documented spec change (REQ-KERNEL-005).
// Module: MOD-K4-MASK | REQs: REQ-KERNEL-001/-005 | ADR-006
#include "src/kernels/mask/mask_scalar_impl.h"

QUIVER_BEGIN_NAMESPACE
namespace detail::scalar {

void k4_mask_combine(MaskOp op, const std::uint8_t* a, const std::uint8_t* b, std::int64_t n,
                     std::uint8_t* out) noexcept {
  return scalar_impl::mask_combine(op, a, b, n, out);
}

void k4_mask_not(const std::uint8_t* a, std::int64_t n, std::uint8_t* out) noexcept {
  return scalar_impl::mask_not(a, n, out);
}

std::int64_t k4_mask_popcount(const std::uint8_t* a, std::int64_t n) noexcept {
  return scalar_impl::mask_popcount(a, n);
}

bool k4_mask_all(const std::uint8_t* a, std::int64_t n) noexcept {
  return scalar_impl::mask_all(a, n);
}

bool k4_mask_any(const std::uint8_t* a, std::int64_t n) noexcept {
  return scalar_impl::mask_any(a, n);
}

bool k4_mask_none(const std::uint8_t* a, std::int64_t n) noexcept {
  return scalar_impl::mask_none(a, n);
}

}  // namespace detail::scalar
QUIVER_END_NAMESPACE
