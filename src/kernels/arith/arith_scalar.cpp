// K9 arith — concrete scalar backends (ADR-006): thin instantiations of the
// arith_scalar_impl.h reference (value kernels; the validity overload composes in the
// façade via K4's public API — the documented cross-family exception, PRD 02 §6).
// GENERATED-BY-HAND-STABLE: regenerate only with a documented spec change (REQ-KERNEL-005).
// Module: MOD-K9-ARITH | REQs: REQ-KERNEL-001/-005 | ADR-006
#include "src/kernels/arith/arith_scalar_impl.h"

QUIVER_BEGIN_NAMESPACE
namespace detail::scalar {

// NOLINTBEGIN(bugprone-macro-parentheses): T expands to type names inside declarators.
#define QUIVER_K9_DEFINE(T)                                                                        \
  void k9_arith(ArithOp op, const T* a, const T* b, std::int64_t n, T* out) noexcept {             \
    scalar_impl::arith<T>(op, a, b, n, out);                                                       \
  }                                                                                                \
  void k9_arith_scalar_rhs(ArithOp op, const T* a, T b, std::int64_t n, T* out) noexcept {         \
    scalar_impl::arith_scalar_rhs<T>(op, a, b, n, out);                                            \
  }

QUIVER_K9_DEFINE(std::int8_t)
QUIVER_K9_DEFINE(std::int16_t)
QUIVER_K9_DEFINE(std::int32_t)
QUIVER_K9_DEFINE(std::int64_t)
QUIVER_K9_DEFINE(std::uint8_t)
QUIVER_K9_DEFINE(std::uint16_t)
QUIVER_K9_DEFINE(std::uint32_t)
QUIVER_K9_DEFINE(std::uint64_t)
QUIVER_K9_DEFINE(float)
QUIVER_K9_DEFINE(double)
#undef QUIVER_K9_DEFINE
// NOLINTEND(bugprone-macro-parentheses)

}  // namespace detail::scalar
QUIVER_END_NAMESPACE
