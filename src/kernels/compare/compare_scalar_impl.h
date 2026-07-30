// K1 compare — scalar reference implementation. THIS FILE IS THE FAMILY'S SPECIFICATION
// (Charter T3): every backend must match it bit-for-bit. Intrinsic-free and target-region-
// clean so bench baselines recompile it under ISA regions (REQ-SIMD-006, ADR-011).
//
// Semantics (PRD 04 §5 K1, PRD 08 §5 K1): output bit i = valid(i) ∧ pred(i); two-batch form
// valid(i) = a_valid(i) ∧ b_valid(i); floats use IEEE ordered comparisons (NaN false except
// kNe true; -0.0 == +0.0); between is inclusive both ends; bitmap outputs are fully written
// with tail bits zeroed (ADR-016) and return popcount; selvec outputs are strictly increasing,
// defined output = first count entries within the n-element capacity region (REQ-MEM-008).
// Core loops are branch-free in the data (REQ-KERNEL-003; Survey §3.4).
// Module: MOD-K1-COMPARE | REQs: REQ-K1-001..003, REQ-KERNEL-001..004 | ADR-016, ADR-025
#pragma once

#include <cstdint>

#include "quiver/core.h"
#include "src/kernels/common/kernel_common.h"

QUIVER_BEGIN_NAMESPACE
namespace detail::scalar_impl {

// Branch-free predicate evaluation: C++ comparison operators carry IEEE semantics for floats
// (all ordered comparisons with NaN are false), which composes kNe as !(a == b) → NaN true.
template <class T>
QUIVER_FORCE_INLINE bool compare_one(CompareOp op, T a, T b) noexcept {
  switch (op) {
  case CompareOp::kEq:
    return a == b;
  case CompareOp::kNe:
    return !(a == b);
  case CompareOp::kLt:
    return a < b;
  case CompareOp::kLe:
    return a <= b;
  case CompareOp::kGt:
    return a > b;
  case CompareOp::kGe:
    return a >= b;
  }
  return false;  // unreachable for in-contract op values
}

// Shared bitmap-assembly core: build each output byte from up to 8 predicate lanes, count as
// we go, zero tails by construction. `pred(i)` must be branch-free in the data.
// `bits` predicate results from `base`, LSB-first.
template <class Pred>
QUIVER_FORCE_INLINE std::uint8_t pack_byte(Pred pred, std::int64_t base, int bits) noexcept {
  std::uint8_t byte = 0;
  for (int k = 0; k < bits; ++k) {
    byte = static_cast<std::uint8_t>(byte | (static_cast<std::uint8_t>(pred(base + k)) << k));
  }
  return byte;
}

template <class Pred>
QUIVER_FORCE_INLINE std::int64_t emit_bitmap(std::int64_t n, std::uint8_t* out,
                                             Pred pred) noexcept {
  std::int64_t count = 0;
  const std::int64_t full_bytes = n >> 3;
  for (std::int64_t b = 0; b < full_bytes; ++b) {
    out[b] = pack_byte(pred, b << 3, 8);
    count += std::popcount(out[b]);
  }
  const int tail = static_cast<int>(n & 7);
  if (tail != 0) {
    // bits >= tail are zero by construction (ADR-016)
    out[full_bytes] = pack_byte(pred, full_bytes << 3, tail);
    count += std::popcount(out[full_bytes]);
  }
  return count;
}

// Selvec emission: unconditional store + conditional advance — writes scratch at [count, n)
// within the capacity region, never beyond (REQ-MEM-008); strictly increasing by construction.
template <class Pred>
QUIVER_FORCE_INLINE std::int64_t emit_selvec(std::int64_t n, std::uint32_t* out,
                                             Pred pred) noexcept {
  std::int64_t count = 0;
  for (std::int64_t i = 0; i < n; ++i) {
    out[count] = static_cast<std::uint32_t>(i);
    count += pred(i) ? 1 : 0;
  }
  return count;
}

// A batch under comparison together with the validity that gates it. These three always
// travel as a unit, and the six entry points below each took them as loose parameters.
template <class T>
struct CompareBatch {
  const T* in;
  std::int64_t n;
  const std::uint8_t* validity;
};

// --- The six K1 entry points (validity == nullptr means all-valid, REQ-API-008) --------------

template <class T>
std::int64_t compare_bitmap(CompareOp op, CompareBatch<T> x, T comparand,
                            std::uint8_t* out) noexcept {
  if (x.validity == nullptr) {  // distinct mask-free path (PRD 08 K1)
    return emit_bitmap(x.n, out,
                       [&](std::int64_t i) { return compare_one(op, x.in[i], comparand); });
  }
  return emit_bitmap(x.n, out, [&](std::int64_t i) {
    return bitmap_get(x.validity, i) && compare_one(op, x.in[i], comparand);
  });
}

template <class T>
std::int64_t compare_bitmap2(CompareOp op, CompareBatch<T> x, CompareBatch<T> y,
                             std::uint8_t* out) noexcept {
  return emit_bitmap(x.n, out, [&](std::int64_t i) {
    return is_valid(x.validity, i) && is_valid(y.validity, i) && compare_one(op, x.in[i], y.in[i]);
  });
}

template <class T>
std::int64_t compare_between_bitmap(CompareBatch<T> x, T lo, T hi, std::uint8_t* out) noexcept {
  return emit_bitmap(x.n, out, [&](std::int64_t i) {
    return is_valid(x.validity, i) && (lo <= x.in[i]) && (x.in[i] <= hi);
  });
}

template <class T>
std::int64_t compare_selvec(CompareOp op, CompareBatch<T> x, T comparand,
                            std::uint32_t* out) noexcept {
  return emit_selvec(x.n, out, [&](std::int64_t i) {
    return is_valid(x.validity, i) && compare_one(op, x.in[i], comparand);
  });
}

template <class T>
std::int64_t compare_selvec2(CompareOp op, CompareBatch<T> x, CompareBatch<T> y,
                             std::uint32_t* out) noexcept {
  return emit_selvec(x.n, out, [&](std::int64_t i) {
    return is_valid(x.validity, i) && is_valid(y.validity, i) && compare_one(op, x.in[i], y.in[i]);
  });
}

template <class T>
std::int64_t compare_between_selvec(CompareBatch<T> x, T lo, T hi, std::uint32_t* out) noexcept {
  return emit_selvec(x.n, out, [&](std::int64_t i) {
    return is_valid(x.validity, i) && (lo <= x.in[i]) && (x.in[i] <= hi);
  });
}

}  // namespace detail::scalar_impl
QUIVER_END_NAMESPACE
