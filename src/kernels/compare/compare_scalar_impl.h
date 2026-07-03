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
template <class Pred>
QUIVER_FORCE_INLINE std::int64_t emit_bitmap(std::int64_t n, std::uint8_t* out,
                                             Pred pred) noexcept {
  std::int64_t count = 0;
  const std::int64_t full_bytes = n >> 3;
  for (std::int64_t b = 0; b < full_bytes; ++b) {
    const std::int64_t base = b << 3;
    std::uint8_t byte = 0;
    for (int k = 0; k < 8; ++k) {
      byte = static_cast<std::uint8_t>(byte | (static_cast<std::uint8_t>(pred(base + k)) << k));
    }
    out[b] = byte;
    count += std::popcount(byte);
  }
  const int tail = static_cast<int>(n & 7);
  if (tail != 0) {
    std::uint8_t byte = 0;
    for (int k = 0; k < tail; ++k) {
      byte = static_cast<std::uint8_t>(
          byte | (static_cast<std::uint8_t>(pred((full_bytes << 3) + k)) << k));
    }
    out[full_bytes] = byte;  // bits >= tail are zero by construction (ADR-016)
    count += std::popcount(byte);
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

// --- The six K1 entry points (validity == nullptr means all-valid, REQ-API-008) --------------

template <class T>
std::int64_t compare_bitmap(CompareOp op, const T* in, std::int64_t n, T comparand,
                            const std::uint8_t* validity, std::uint8_t* out) noexcept {
  if (validity == nullptr) {  // distinct mask-free path (PRD 08 K1)
    return emit_bitmap(n, out, [&](std::int64_t i) { return compare_one(op, in[i], comparand); });
  }
  return emit_bitmap(n, out, [&](std::int64_t i) {
    return bitmap_get(validity, i) && compare_one(op, in[i], comparand);
  });
}

template <class T>
std::int64_t compare_bitmap2(CompareOp op, const T* a, const T* b, std::int64_t n,
                             const std::uint8_t* a_validity, const std::uint8_t* b_validity,
                             std::uint8_t* out) noexcept {
  return emit_bitmap(n, out, [&](std::int64_t i) {
    return is_valid(a_validity, i) && is_valid(b_validity, i) && compare_one(op, a[i], b[i]);
  });
}

template <class T>
std::int64_t compare_between_bitmap(const T* in, std::int64_t n, T lo, T hi,
                                    const std::uint8_t* validity, std::uint8_t* out) noexcept {
  return emit_bitmap(n, out, [&](std::int64_t i) {
    return is_valid(validity, i) && (lo <= in[i]) && (in[i] <= hi);
  });
}

template <class T>
std::int64_t compare_selvec(CompareOp op, const T* in, std::int64_t n, T comparand,
                            const std::uint8_t* validity, std::uint32_t* out) noexcept {
  return emit_selvec(n, out, [&](std::int64_t i) {
    return is_valid(validity, i) && compare_one(op, in[i], comparand);
  });
}

template <class T>
std::int64_t compare_selvec2(CompareOp op, const T* a, const T* b, std::int64_t n,
                             const std::uint8_t* a_validity, const std::uint8_t* b_validity,
                             std::uint32_t* out) noexcept {
  return emit_selvec(n, out, [&](std::int64_t i) {
    return is_valid(a_validity, i) && is_valid(b_validity, i) && compare_one(op, a[i], b[i]);
  });
}

template <class T>
std::int64_t compare_between_selvec(const T* in, std::int64_t n, T lo, T hi,
                                    const std::uint8_t* validity, std::uint32_t* out) noexcept {
  return emit_selvec(n, out, [&](std::int64_t i) {
    return is_valid(validity, i) && (lo <= in[i]) && (in[i] <= hi);
  });
}

}  // namespace detail::scalar_impl
QUIVER_END_NAMESPACE
