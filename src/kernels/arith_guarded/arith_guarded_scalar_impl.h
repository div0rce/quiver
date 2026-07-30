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

// Wrapping add/sub on the unsigned image (the unsigned-internal idiom, REQ-STD-008).
template <class T>
QUIVER_FORCE_INLINE T wrap_add(T a, T b) noexcept {
  using U = std::make_unsigned_t<T>;
  return static_cast<T>(static_cast<U>(static_cast<U>(a) + static_cast<U>(b)));
}

template <class T>
QUIVER_FORCE_INLINE T wrap_sub(T a, T b) noexcept {
  using U = std::make_unsigned_t<T>;
  return static_cast<T>(static_cast<U>(static_cast<U>(a) - static_cast<U>(b)));
}

// Checked add and sub share one shape: wrap on the unsigned image, then an overflow test that
// differs only in which operands it compares.
template <class T, bool IsAdd>
QUIVER_FORCE_INLINE bool checked_addsub_one(T a, T b, T* r) noexcept {
  const T res = IsAdd ? wrap_add(a, b) : wrap_sub(a, b);
  *r = res;
  if constexpr (std::is_signed_v<T>) {
    // sign trick: add overflows when the result's sign differs from both operands', sub when
    // the operands differ in sign and the result's differs from a's
    return IsAdd ? (((a ^ res) & (b ^ res)) < 0) : (((a ^ b) & (a ^ res)) < 0);
  } else {
    return IsAdd ? (res < a) : (b > a);  // carry / borrow compare
  }
}

// Narrower than 64-bit: the exact product fits a doubled-width type.
template <class T>
QUIVER_FORCE_INLINE bool checked_mul_narrow(T a, T b, T* r) noexcept {
  using U = std::make_unsigned_t<T>;
  using W = std::conditional_t<std::is_signed_v<T>,
                               std::conditional_t<sizeof(T) == 4, std::int64_t, std::int32_t>,
                               std::conditional_t<sizeof(T) == 4, std::uint64_t, std::uint32_t>>;
  const W wide = static_cast<W>(a) * static_cast<W>(b);
  const T res = static_cast<T>(static_cast<U>(static_cast<std::make_unsigned_t<W>>(wide)));
  *r = res;
  return wide != static_cast<W>(res);
}

#if defined(__SIZEOF_INT128__)
// 128-bit exactness; a GNU extension guarded like the K6 checked sums.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

template <class T>
QUIVER_FORCE_INLINE bool checked_mul_wide(T a, T b, T* r) noexcept {
  using U = std::make_unsigned_t<T>;
  using W = std::conditional_t<std::is_signed_v<T>, __int128, unsigned __int128>;
  const W wide = static_cast<W>(a) * static_cast<W>(b);
  const T res = static_cast<T>(static_cast<U>(static_cast<unsigned __int128>(wide)));
  *r = res;
  return wide != static_cast<W>(res);
}

#pragma GCC diagnostic pop
#else
// MSVC tier-2: portable top-half check via division, no builtins.
template <class T>
QUIVER_FORCE_INLINE bool checked_mul_wide(T a, T b, T* r) noexcept {
  using U = std::make_unsigned_t<T>;
  const T res = static_cast<T>(static_cast<U>(static_cast<U>(a) * static_cast<U>(b)));
  *r = res;
  if (a == 0 || b == 0) {
    return false;
  }
  if constexpr (std::is_signed_v<T>) {
    // INT_MIN * -1 must be tested BEFORE res/b: when a==INT_MIN && b==-1 the wrapped res is
    // INT_MIN, and res/b would evaluate INT_MIN/-1 (itself UB) — so short-circuit that case
    // first (|| evaluates left-to-right).
    return (a == std::numeric_limits<T>::min() && b == T{-1}) || (res / b != a);
  } else {
    return res / b != a;
  }
}
#endif

template <class T>
QUIVER_FORCE_INLINE bool checked_mul_one(T a, T b, T* r) noexcept {
  if constexpr (sizeof(T) < 8) {
    return checked_mul_narrow(a, b, r);
  } else {
    return checked_mul_wide(a, b, r);
  }
}

// One checked op: {wrapped result, overflowed?}. Branch-free in the data.
template <class T>
QUIVER_FORCE_INLINE bool checked_one(ArithOp op, T a, T b, T* r) noexcept {
  static_assert(std::is_integral_v<T>, "K10 is integer-only (PRD 04)");
  switch (op) {
  case ArithOp::kAdd:
    return checked_addsub_one<T, true>(a, b, r);
  case ArithOp::kSub:
    return checked_addsub_one<T, false>(a, b, r);
  case ArithOp::kMul:
    return checked_mul_one(a, b, r);
  }
  *r = a;
  return false;  // unreachable for in-contract op values
}

// The left-hand batch a guarded op reads.
template <class T>
struct AgSpan {
  const T* a;
  std::int64_t n;
};

// Where a checked op writes: values, plus the optional per-element overflow bitmap.
template <class T>
struct AgCheckedSink {
  T* out;
  std::uint8_t* overflow_bits;
};

// No bitmap requested: wrapped results at all lanes plus the exact count.
template <class T, class Rhs>
std::int64_t arith_checked_count(ArithOp op, AgSpan<T> in, Rhs rhs, T* out) noexcept {
  std::int64_t count = 0;
  for (std::int64_t i = 0; i < in.n; ++i) {
    T r;
    count += checked_one(op, in.a[i], rhs(i), &r) ? 1 : 0;
    out[i] = r;
  }
  return count;
}

// Bitmap requested: bit assembly mirrors the K1 emit core; bits past the tail are zero by
// construction (ADR-016).
template <class T, class Rhs>
std::int64_t arith_checked_emit(ArithOp op, AgSpan<T> in, Rhs rhs, AgCheckedSink<T> sink) noexcept {
  std::int64_t count = 0;
  std::int64_t byte_idx = 0;
  std::uint8_t byte = 0;
  int k = 0;
  for (std::int64_t i = 0; i < in.n; ++i) {
    T r;
    const bool ov = checked_one(op, in.a[i], rhs(i), &r);
    sink.out[i] = r;
    count += ov ? 1 : 0;
    byte = static_cast<std::uint8_t>(byte | (static_cast<std::uint8_t>(ov) << k));
    if (++k == 8) {
      sink.overflow_bits[byte_idx++] = byte;
      byte = 0;
      k = 0;
    }
  }
  if (k != 0) {
    sink.overflow_bits[byte_idx] = byte;
  }
  return count;
}

// Checked loop (ADR-014).
template <class T, class Rhs>
std::int64_t arith_checked_core(ArithOp op, AgSpan<T> in, Rhs rhs, AgCheckedSink<T> sink) noexcept {
  if (sink.overflow_bits == nullptr) {
    return arith_checked_count(op, in, rhs, sink.out);
  }
  return arith_checked_emit(op, in, rhs, sink);
}

template <class T>
std::int64_t arith_checked(ArithOp op, AgSpan<T> in, const T* b, AgCheckedSink<T> sink) noexcept {
  return arith_checked_core(op, in, [&](std::int64_t i) { return b[i]; }, sink);
}

template <class T>
std::int64_t arith_checked_scalar_rhs(ArithOp op, AgSpan<T> in, T b,
                                      AgCheckedSink<T> sink) noexcept {
  return arith_checked_core(op, in, [&](std::int64_t) { return b; }, sink);
}

// Which limit an overflowing signed op clamps to: for add/mul the sign of the mathematically
// correct result is recoverable from the operands, for sub from a vs b.
template <class T>
QUIVER_FORCE_INLINE T saturate_limit_signed(ArithOp op, T a, T b, T wrapped) noexcept {
  switch (op) {
  case ArithOp::kAdd:
    return (a < 0) ? std::numeric_limits<T>::min() : std::numeric_limits<T>::max();
  case ArithOp::kSub:
    return (a < b) ? std::numeric_limits<T>::min() : std::numeric_limits<T>::max();
  case ArithOp::kMul:
    return ((a < 0) != (b < 0)) ? std::numeric_limits<T>::min() : std::numeric_limits<T>::max();
  }
  return wrapped;  // unreachable for in-contract op values
}

template <class T>
QUIVER_FORCE_INLINE T saturate_limit_unsigned(ArithOp op, T wrapped) noexcept {
  switch (op) {
  case ArithOp::kAdd:
  case ArithOp::kMul:
    return std::numeric_limits<T>::max();
  case ArithOp::kSub:
    return std::numeric_limits<T>::min();  // 0 for unsigned
  }
  return wrapped;  // unreachable for in-contract op values
}

// One saturating op: clamp exactly at the type limits (REQ-K10-002).
template <class T>
QUIVER_FORCE_INLINE T saturate_one(ArithOp op, T a, T b) noexcept {
  T r;
  if (!checked_one(op, a, b, &r)) {
    return r;
  }
  if constexpr (std::is_signed_v<T>) {
    return saturate_limit_signed(op, a, b, r);
  } else {
    return saturate_limit_unsigned(op, r);
  }
}

template <class T>
void arith_saturating(ArithOp op, AgSpan<T> in, const T* b, T* out) noexcept {
  for (std::int64_t i = 0; i < in.n; ++i) {
    out[i] = saturate_one(op, in.a[i], b[i]);
  }
}

template <class T>
void arith_saturating_scalar_rhs(ArithOp op, AgSpan<T> in, T b, T* out) noexcept {
  for (std::int64_t i = 0; i < in.n; ++i) {
    out[i] = saturate_one(op, in.a[i], b);
  }
}

}  // namespace detail::scalar_impl
QUIVER_END_NAMESPACE
