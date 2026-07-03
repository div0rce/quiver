// K3 select — concrete scalar backends: one symbol per admissible template-parameter
// combination (ADR-006), each a thin instantiation of the select_scalar_impl.h reference.
// GENERATED-BY-HAND-STABLE: regenerate only with a documented spec change (REQ-KERNEL-005).
// Module: MOD-K3-SELECT | REQs: REQ-KERNEL-001/-005 | ADR-006
#include "src/kernels/select/select_scalar_impl.h"

QUIVER_BEGIN_NAMESPACE
namespace detail::scalar {

std::int64_t k3_bitmap_to_selvec(const std::uint8_t* selection, std::int64_t n,
                                 std::uint32_t* out) noexcept {
  return scalar_impl::bitmap_to_selvec(selection, n, out);
}

void k3_selvec_to_bitmap(const std::uint32_t* sel, std::int64_t sel_len, std::int64_t n,
                         std::uint8_t* out) noexcept {
  return scalar_impl::selvec_to_bitmap(sel, sel_len, n, out);
}

}  // namespace detail::scalar
QUIVER_END_NAMESPACE
