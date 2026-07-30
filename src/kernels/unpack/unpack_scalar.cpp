// K8 unpack — concrete scalar backends (ADR-006): thin instantiations of the
// unpack_scalar_impl.h reference. The single `base` parameter serves both public APIs
// (`unpack` passes base = 0; `unpack_for` its base) — one concrete symbol per Out type.
// GENERATED-BY-HAND-STABLE: regenerate only with a documented spec change (REQ-KERNEL-005).
// Module: MOD-K8-UNPACK | REQs: REQ-KERNEL-001/-005 | ADR-006, ADR-026
#include "src/kernels/unpack/unpack_scalar_impl.h"

QUIVER_BEGIN_NAMESPACE
namespace detail::scalar {

// NOLINTBEGIN(bugprone-macro-parentheses): T expands to type names inside declarators.
#define QUIVER_K8_DEFINE(T)                                                                        \
  void k8_unpack(const std::uint8_t* packed, std::int64_t n, int bit_width, T base,                \
                 T* out) noexcept {                                                                \
    scalar_impl::unpack<T>({packed, n, bit_width}, base, out);                                     \
  }

QUIVER_K8_DEFINE(std::uint8_t)
QUIVER_K8_DEFINE(std::uint16_t)
QUIVER_K8_DEFINE(std::uint32_t)
QUIVER_K8_DEFINE(std::uint64_t)
#undef QUIVER_K8_DEFINE
// NOLINTEND(bugprone-macro-parentheses)

}  // namespace detail::scalar
QUIVER_END_NAMESPACE
