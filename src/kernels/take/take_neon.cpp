// K5 take / dict_decode — NEON backend. NEON has no gather instruction (Survey §4.1), so
// the REQ-KERNEL-007 evidence gate is recorded as N/A for this ISA: the only technique is
// unrolled independent scalar loads (the scalar reference already implements exactly that
// MLP shape, ≥4 loads in flight — Survey §3.9), so every entry point delegates to the
// scalar core and is bit-identical by construction (REQ-KERNEL-002). The family doc records
// the N/A rationale (REQ-K5-004).
// Module: MOD-K5-TAKE | REQs: REQ-K5-001..004, REQ-SIMD-001..003 | ADR-003
#include "src/kernels/take/take_scalar_impl.h"

#if defined(__aarch64__) || defined(_M_ARM64)

QUIVER_BEGIN_NAMESPACE
namespace detail::neon {

// NOLINTBEGIN(bugprone-macro-parentheses): T/C expand to type names inside declarators.
#define QUIVER_K5_TAKE_DEFINE(T)                                                                   \
  void k5_take(const T* values, std::int64_t values_len, const std::uint32_t* idx,                 \
               std::int64_t idx_len, T* out) noexcept {                                            \
    scalar_impl::take<T>({values, values_len}, {idx, idx_len}, out);                               \
  }

#define QUIVER_K5_DECODE_DEFINE(T, C)                                                              \
  void k5_dict_decode(const T* dict, std::int64_t dict_len, const C* codes, std::int64_t n,        \
                      const std::uint32_t* sel, std::int64_t sel_len, T* out) noexcept {           \
    return sel == nullptr                                                                          \
               ? scalar_impl::dict_decode<T, C>({dict, dict_len}, codes, n, out)                   \
               : scalar_impl::dict_decode_sel<T, C>({dict, dict_len}, codes, {sel, sel_len}, out); \
  }

#define QUIVER_K5_DEFINE(T)                                                                        \
  QUIVER_K5_TAKE_DEFINE(T)                                                                         \
  QUIVER_K5_DECODE_DEFINE(T, std::uint8_t)                                                         \
  QUIVER_K5_DECODE_DEFINE(T, std::uint16_t)                                                        \
  QUIVER_K5_DECODE_DEFINE(T, std::uint32_t)

QUIVER_K5_DEFINE(std::int8_t)
QUIVER_K5_DEFINE(std::int16_t)
QUIVER_K5_DEFINE(std::int32_t)
QUIVER_K5_DEFINE(std::int64_t)
QUIVER_K5_DEFINE(std::uint8_t)
QUIVER_K5_DEFINE(std::uint16_t)
QUIVER_K5_DEFINE(std::uint32_t)
QUIVER_K5_DEFINE(std::uint64_t)
QUIVER_K5_DEFINE(float)
QUIVER_K5_DEFINE(double)
#undef QUIVER_K5_DEFINE
#undef QUIVER_K5_DECODE_DEFINE
#undef QUIVER_K5_TAKE_DEFINE
// NOLINTEND(bugprone-macro-parentheses)

}  // namespace detail::neon
QUIVER_END_NAMESPACE

#endif  // aarch64
