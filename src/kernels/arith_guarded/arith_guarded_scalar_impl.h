// K10 arith_guarded — scalar reference implementation (the family specification, Charter
// T3). Never silent UB (Charter §7.4).
//
// arith_checked (ADR-014): wrapped (modular) results at EVERY lane; bit i of the nullable
// `overflow_bits` set iff lane i overflowed (tail-zeroed per ADR-016); returns the exact
// overflow count — all accumulated branchlessly (REQ-KERNEL-003 posture). Detection:
// add/sub via the sign trick (((a^r) & (b^r)) < 0 signed; carry compare unsigned); mul via
// wide multiplication where a wider type exists, and the top-half consistency check for
// 64-bit (the scalar 64-bit path is the documented REQ-K10-003 concession).
//
// arith_saturating: clamps exactly at numeric_limits<T>::min()/max() (REQ-K10-002; the
// INT_MIN edge cases are enumerated in the unit suite).
// Module: MOD-K10-ARITH-GUARDED | REQs: REQ-K10-001..003 | ADR-014, ADR-016
#pragma once

#include <cstdint>
#include <limits>
#include <type_traits>

#include "quiver/core.h"
#include "src/kernels/common/kernel_common.h"

QUIVER_BEGIN_NAMESPACE
namespace detail::scalar_impl {

// One checked op: {wrapped result, overflowed?}. Branch-free in the data.
template <class T>
QUIVER_FORCE_INLINE bool checked_one(ArithOp op, T a, T b, T* r) noexcept {
  static_assert(std::is_integral_v<T>, "K10 is integer-only (PRD 04)");
  using U = std::make_unsigned_t<T>;
  const U ua = static_cast<U>(a);
  const U ub = static_cast<U>(b);
  if constexpr (std::is_signed_v<T>) {
    switch (op) {
    case ArithOp::kAdd: {
      const T res = static_cast<T>(static_cast<U>(ua + ub));
      *r = res;
      return ((a ^ res) & (b ^ res)) < 0;  // sign trick
    }
    case ArithOp::kSub: {
      const T res = static_cast<T>(static_cast<U>(ua - ub));
      *r = res;
      return ((a ^ b) & (a ^ res)) < 0;
    }
    case ArithOp::kMul: {
      if constexpr (sizeof(T) < 8) {
        using W = std::conditional_t<sizeof(T) == 4, std::int64_t, std::int32_t>;
        const W wide = static_cast<W>(a) * static_cast<W>(b);
        const T res = static_cast<T>(static_cast<U>(static_cast<std::make_unsigned_t<W>>(wide)));
        *r = res;
        return wide != static_cast<W>(res);
      } else {
#if defined(__SIZEOF_INT128__)
        // 128-bit exactness; a GNU extension guarded like the K6 checked sums.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        const __int128 wide = static_cast<__int128>(a) * static_cast<__int128>(b);
        const T res = static_cast<T>(static_cast<U>(static_cast<unsigned __int128>(wide)));
        *r = res;
        return wide != static_cast<__int128>(res);
#pragma GCC diagnostic pop
#else
        // MSVC tier-2: portable top-half check via __builtin-free logic.
        const U res_u = static_cast<U>(ua * ub);
        const T res = static_cast<T>(res_u);
        *r = res;
        if (a == 0 || b == 0) {
          return false;
        }
        // INT_MIN * -1 must be tested BEFORE res/b: when a==INT_MIN && b==-1 the wrapped
        // res is INT_MIN, and res/b would evaluate INT_MIN/-1 (itself UB) — so short-circuit
        // that case first (|| evaluates left-to-right).
        return (a == std::numeric_limits<T>::min() && b == T{-1}) || (res / b != a);
#endif
      }
    }
    }
  } else {
    switch (op) {
    case ArithOp::kAdd: {
      const T res = static_cast<T>(static_cast<U>(ua + ub));
      *r = res;
      return res < a;  // carry compare
    }
    case ArithOp::kSub: {
      const T res = static_cast<T>(static_cast<U>(ua - ub));
      *r = res;
      return b > a;
    }
    case ArithOp::kMul: {
      if constexpr (sizeof(T) < 8) {
        using W = std::conditional_t<sizeof(T) == 4, std::uint64_t, std::uint32_t>;
        const W wide = static_cast<W>(a) * static_cast<W>(b);
        const T res = static_cast<T>(wide);
        *r = res;
        return wide != static_cast<W>(res);
      } else {
#if defined(__SIZEOF_INT128__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        const unsigned __int128 wide =
            static_cast<unsigned __int128>(a) * static_cast<unsigned __int128>(b);
        const T res = static_cast<T>(wide);
        *r = res;
        return wide != static_cast<unsigned __int128>(res);
#pragma GCC diagnostic pop
#else
        const T res = static_cast<T>(ua * ub);
        *r = res;
        return b != 0 && res / b != a;
#endif
      }
    }
    }
  }
  *r = a;
  return false;  // unreachable for in-contract op values
}

// Bitmap-emitting checked loop (ADR-014): wrapped results at all lanes, exact count,
// optional position bitmap (tail-zeroed). Bit assembly mirrors the K1 emit core.
template <class T, class Rhs>
std::int64_t arith_checked_core(ArithOp op, const T* a, Rhs rhs, std::int64_t n, T* out,
                                std::uint8_t* overflow_bits) noexcept {
  std::int64_t count = 0;
  if (overflow_bits == nullptr) {
    for (std::int64_t i = 0; i < n; ++i) {
      T r;
      count += checked_one(op, a[i], rhs(i), &r) ? 1 : 0;
      out[i] = r;
    }
    return count;
  }
  std::int64_t byte_idx = 0;
  std::uint8_t byte = 0;
  int k = 0;
  for (std::int64_t i = 0; i < n; ++i) {
    T r;
    const bool ov = checked_one(op, a[i], rhs(i), &r);
    out[i] = r;
    count += ov ? 1 : 0;
    byte = static_cast<std::uint8_t>(byte | (static_cast<std::uint8_t>(ov) << k));
    if (++k == 8) {
      overflow_bits[byte_idx++] = byte;
      byte = 0;
      k = 0;
    }
  }
  if (k != 0) {
    overflow_bits[byte_idx] = byte;  // bits >= tail are zero by construction (ADR-016)
  }
  return count;
}

template <class T>
std::int64_t arith_checked(ArithOp op, const T* a, const T* b, std::int64_t n, T* out,
                           std::uint8_t* overflow_bits) noexcept {
  return arith_checked_core(op, a, [&](std::int64_t i) { return b[i]; }, n, out, overflow_bits);
}

template <class T>
std::int64_t arith_checked_scalar_rhs(ArithOp op, const T* a, T b, std::int64_t n, T* out,
                                      std::uint8_t* overflow_bits) noexcept {
  return arith_checked_core(op, a, [&](std::int64_t) { return b; }, n, out, overflow_bits);
}

// One saturating op: clamp exactly at the type limits (REQ-K10-002).
template <class T>
QUIVER_FORCE_INLINE T saturate_one(ArithOp op, T a, T b) noexcept {
  T r;
  const bool ov = checked_one(op, a, b, &r);
  if (!ov) {
    return r;
  }
  if constexpr (std::is_signed_v<T>) {
    // Overflow direction: for add/mul, the sign of the mathematically correct result is
    // recoverable from the operands; for sub, from a vs b.
    switch (op) {
    case ArithOp::kAdd:
      return (a < 0) ? std::numeric_limits<T>::min() : std::numeric_limits<T>::max();
    case ArithOp::kSub:
      return (a < b) ? std::numeric_limits<T>::min() : std::numeric_limits<T>::max();
    case ArithOp::kMul:
      return ((a < 0) != (b < 0)) ? std::numeric_limits<T>::min() : std::numeric_limits<T>::max();
    }
    return r;  // unreachable for in-contract op values
  } else {
    switch (op) {
    case ArithOp::kAdd:
    case ArithOp::kMul:
      return std::numeric_limits<T>::max();
    case ArithOp::kSub:
      return std::numeric_limits<T>::min();  // 0 for unsigned
    }
    return r;  // unreachable for in-contract op values
  }
}

template <class T>
void arith_saturating(ArithOp op, const T* a, const T* b, std::int64_t n, T* out) noexcept {
  for (std::int64_t i = 0; i < n; ++i) {
    out[i] = saturate_one(op, a[i], b[i]);
  }
}

template <class T>
void arith_saturating_scalar_rhs(ArithOp op, const T* a, T b, std::int64_t n, T* out) noexcept {
  for (std::int64_t i = 0; i < n; ++i) {
    out[i] = saturate_one(op, a[i], b);
  }
}

}  // namespace detail::scalar_impl
QUIVER_END_NAMESPACE
