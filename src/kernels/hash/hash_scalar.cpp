// K7 hash — concrete scalar backends: one symbol per admissible template-parameter
// combination (ADR-006), each a thin instantiation of the hash_scalar_impl.h reference.
// GENERATED-BY-HAND-STABLE: regenerate only with a documented spec change (REQ-KERNEL-005).
// Module: MOD-K7-HASH | REQs: REQ-KERNEL-001/-005 | ADR-006, ADR-012
#include "src/kernels/hash/hash_scalar_impl.h"

QUIVER_BEGIN_NAMESPACE
namespace detail::scalar {

// NOLINTBEGIN(bugprone-macro-parentheses): T expands to type names inside declarators.
#define QUIVER_K7_DEFINE(T)                                                                        \
  void k7_hash64(const T* in, std::int64_t n, std::uint64_t seed, std::uint64_t* out) noexcept {   \
    scalar_impl::hash64<T>(in, n, seed, out);                                                      \
  }

QUIVER_K7_DEFINE(std::int8_t)
QUIVER_K7_DEFINE(std::int16_t)
QUIVER_K7_DEFINE(std::int32_t)
QUIVER_K7_DEFINE(std::int64_t)
QUIVER_K7_DEFINE(std::uint8_t)
QUIVER_K7_DEFINE(std::uint16_t)
QUIVER_K7_DEFINE(std::uint32_t)
QUIVER_K7_DEFINE(std::uint64_t)
QUIVER_K7_DEFINE(float)
QUIVER_K7_DEFINE(double)
#undef QUIVER_K7_DEFINE
// NOLINTEND(bugprone-macro-parentheses)

void k7_hash64_combine(const std::uint64_t* a, const std::uint64_t* b, std::int64_t n,
                       std::uint64_t* out) noexcept {
  scalar_impl::hash64_combine(a, b, n, out);
}

}  // namespace detail::scalar
QUIVER_END_NAMESPACE
