// K9 arith — scalar reference implementation (the family specification, Charter T3).
// Semantics (PRD 04 §5 K9): elementwise add/sub/mul; integers WRAP by contract (computed in
// unsigned arithmetic internally — no signed-overflow UB path, REQ-K9-001/REQ-STD-008);
// floats are native IEEE-754. The validity overload is defined as values-at-all-lanes plus
// mask_combine(kAnd) on validities and lives in the FAÇADE via K4's public API (the
// documented cross-family exception, PRD 02 §6) — the concrete symbols below are the value
// kernels only. Exact aliasing out == a/b permitted (elementwise forward pass, ADR-023
// delta in API-K9).
// Module: MOD-K9-ARITH | REQs: REQ-K9-001..002, REQ-KERNEL-001..003 | ADR-023-adjacent
#pragma once

#include <cstdint>
#include <type_traits>

#include "quiver/core.h"
#include "src/kernels/common/kernel_common.h"

QUIVER_BEGIN_NAMESPACE
namespace detail::scalar_impl {

template <class T>
QUIVER_FORCE_INLINE T arith_one(ArithOp op, T a, T b) noexcept {
  if constexpr (std::is_floating_point_v<T>) {
    switch (op) {
    case ArithOp::kAdd:
      return a + b;
    case ArithOp::kSub:
      return a - b;
    case ArithOp::kMul:
      return a * b;
    }
    return a;  // unreachable for in-contract op values
  } else {
    // Compute in an unsigned type at least as wide as int: sub-int unsigned operands would
    // otherwise promote to SIGNED int, whose overflow (u16*u16) is UB — the exact UB class
    // this function exists to avoid (REQ-K9-001).
    using U = std::make_unsigned_t<T>;
    using P = std::conditional_t<(sizeof(U) < sizeof(unsigned int)), unsigned int, U>;
    const P ua = static_cast<U>(a);
    const P ub = static_cast<U>(b);
    switch (op) {
    case ArithOp::kAdd:
      return static_cast<T>(static_cast<U>(ua + ub));
    case ArithOp::kSub:
      return static_cast<T>(static_cast<U>(ua - ub));
    case ArithOp::kMul:
      return static_cast<T>(static_cast<U>(ua * ub));
    }
    return a;  // unreachable for in-contract op values
  }
}

template <class T>
void arith(ArithOp op, const T* a, const T* b, std::int64_t n, T* out) noexcept {
  for (std::int64_t i = 0; i < n; ++i) {
    out[i] = arith_one(op, a[i], b[i]);
  }
}

template <class T>
void arith_scalar_rhs(ArithOp op, const T* a, T b, std::int64_t n, T* out) noexcept {
  for (std::int64_t i = 0; i < n; ++i) {
    out[i] = arith_one(op, a[i], b);
  }
}

}  // namespace detail::scalar_impl
QUIVER_END_NAMESPACE
