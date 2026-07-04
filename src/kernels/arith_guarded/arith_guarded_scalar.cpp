// K10 arith_guarded — concrete scalar backends (ADR-006): thin instantiations of the
// arith_guarded_scalar_impl.h reference.
// GENERATED-BY-HAND-STABLE: regenerate only with a documented spec change (REQ-KERNEL-005).
// Module: MOD-K10-ARITH-GUARDED | REQs: REQ-KERNEL-001/-005 | ADR-006, ADR-014
#include "src/kernels/arith_guarded/arith_guarded_scalar_impl.h"

QUIVER_BEGIN_NAMESPACE
namespace detail::scalar {

// NOLINTBEGIN(bugprone-macro-parentheses): T expands to type names inside declarators.
#define QUIVER_K10_DEFINE(T)                                                                       \
  std::int64_t k10_arith_checked(ArithOp op, const T* a, const T* b, std::int64_t n, T* out,       \
                                 std::uint8_t* overflow_bits) noexcept {                           \
    return scalar_impl::arith_checked<T>(op, a, b, n, out, overflow_bits);                         \
  }                                                                                                \
  std::int64_t k10_arith_checked_scalar_rhs(ArithOp op, const T* a, T b, std::int64_t n, T* out,   \
                                            std::uint8_t* overflow_bits) noexcept {                \
    return scalar_impl::arith_checked_scalar_rhs<T>(op, a, b, n, out, overflow_bits);              \
  }                                                                                                \
  void k10_arith_saturating(ArithOp op, const T* a, const T* b, std::int64_t n, T* out) noexcept { \
    scalar_impl::arith_saturating<T>(op, a, b, n, out);                                            \
  }                                                                                                \
  void k10_arith_saturating_scalar_rhs(ArithOp op, const T* a, T b, std::int64_t n,                \
                                       T* out) noexcept {                                          \
    scalar_impl::arith_saturating_scalar_rhs<T>(op, a, b, n, out);                                 \
  }

QUIVER_K10_DEFINE(std::int8_t)
QUIVER_K10_DEFINE(std::int16_t)
QUIVER_K10_DEFINE(std::int32_t)
QUIVER_K10_DEFINE(std::int64_t)
QUIVER_K10_DEFINE(std::uint8_t)
QUIVER_K10_DEFINE(std::uint16_t)
QUIVER_K10_DEFINE(std::uint32_t)
QUIVER_K10_DEFINE(std::uint64_t)
#undef QUIVER_K10_DEFINE
// NOLINTEND(bugprone-macro-parentheses)

}  // namespace detail::scalar
QUIVER_END_NAMESPACE
