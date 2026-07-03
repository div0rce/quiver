// Surface A — K9 arith + K10 arith_guarded: elementwise arithmetic (API-K9-001/002,
// API-K10-001/002). Integers WRAP by contract (two's-complement/modular — explicit in the
// family docs, REQ-K9-001); floats are native IEEE-754. K10 is the never-silent-UB tier
// (Charter §7.4): `arith_checked` reports overflow (count + optional position bitmap,
// ADR-014); `arith_saturating` clamps exactly at the type limits.
// Exact aliasing: out == a.data or b.data permitted; out_validity == a/b validity permitted
// (elementwise forward passes).
// Module: MOD-K9-ARITH / MOD-K10-ARITH-GUARDED | REQs: REQ-K9-*, REQ-K10-* | ADR-006/-014
#pragma once

#include "quiver/core.h"
#include "quiver/detail/config.h"
#include "quiver/detail/extern_decls.h"
#include "quiver/mask.h"

QUIVER_BEGIN_NAMESPACE

// --- K9: wrapping / IEEE elementwise arithmetic ---------------------------------------------

template <Element T>
void arith(ArithOp op, BatchView<T> a, BatchView<T> b, T* out) noexcept {
  QUIVER_ASSERT(a.len == b.len, "arith: a.len == b.len [API-K9-001]");
  QUIVER_ASSERT(a.len >= 0 && a.len <= kMaxBatchLen, "arith: 0 <= n <= kMaxBatchLen [REQ-API-005]");
  detail::k9_arith(op, a.data, b.data, a.len, out);
}

template <Element T>
void arith(ArithOp op, BatchView<T> a, T b, T* out) noexcept {
  QUIVER_ASSERT(a.len >= 0 && a.len <= kMaxBatchLen, "arith: 0 <= n <= kMaxBatchLen [REQ-API-005]");
  detail::k9_arith_scalar_rhs(op, a.data, b, a.len, out);
}

// Validity overload — DEFINED as the composition: values at all lanes (invalid lanes hold
// defined-behavior garbage the validity bit masks) plus mask_combine(kAnd) on validities.
// Implemented via K4's public API: the documented cross-family exception (PRD 02 §6).
template <Element T>
void arith(ArithOp op, BatchView<T> a, BatchView<T> b, BitmapView a_validity, BitmapView b_validity,
           T* out, std::uint8_t* out_validity) noexcept {
  QUIVER_ASSERT(a_validity.bits != nullptr && b_validity.bits != nullptr,
                "arith: validities non-null in the validity overload [API-K9-002]");
  arith(op, a, b, out);
  mask_combine(MaskOp::kAnd, a_validity, b_validity, a.len, out_validity);
}

// --- K10: overflow-guarded integer arithmetic (never silent UB) ------------------------------

// Wrapped results at every lane; bit i of `overflow_bits` (nullable; capacity ceil(n/8),
// tail-zeroed) set iff lane i overflowed; returns the overflow count (ADR-014). Positions
// compose with K2/K3 for extraction.
template <IntElement T>
std::int64_t arith_checked(ArithOp op, BatchView<T> a, BatchView<T> b, T* out,
                           std::uint8_t* overflow_bits) noexcept {
  QUIVER_ASSERT(a.len == b.len, "arith_checked: a.len == b.len [API-K10-001]");
  QUIVER_ASSERT(a.len >= 0 && a.len <= kMaxBatchLen,
                "arith_checked: 0 <= n <= kMaxBatchLen [REQ-API-005]");
  return detail::k10_arith_checked(op, a.data, b.data, a.len, out, overflow_bits);
}

template <IntElement T>
std::int64_t arith_checked(ArithOp op, BatchView<T> a, T b, T* out,
                           std::uint8_t* overflow_bits) noexcept {
  QUIVER_ASSERT(a.len >= 0 && a.len <= kMaxBatchLen,
                "arith_checked: 0 <= n <= kMaxBatchLen [REQ-API-005]");
  return detail::k10_arith_checked_scalar_rhs(op, a.data, b, a.len, out, overflow_bits);
}

// Clamps exactly at numeric_limits<T>::min()/max() (REQ-K10-002).
template <IntElement T>
void arith_saturating(ArithOp op, BatchView<T> a, BatchView<T> b, T* out) noexcept {
  QUIVER_ASSERT(a.len == b.len, "arith_saturating: a.len == b.len [API-K10-002]");
  QUIVER_ASSERT(a.len >= 0 && a.len <= kMaxBatchLen,
                "arith_saturating: 0 <= n <= kMaxBatchLen [REQ-API-005]");
  detail::k10_arith_saturating(op, a.data, b.data, a.len, out);
}

template <IntElement T>
void arith_saturating(ArithOp op, BatchView<T> a, T b, T* out) noexcept {
  QUIVER_ASSERT(a.len >= 0 && a.len <= kMaxBatchLen,
                "arith_saturating: 0 <= n <= kMaxBatchLen [REQ-API-005]");
  detail::k10_arith_saturating_scalar_rhs(op, a.data, b, a.len, out);
}

QUIVER_END_NAMESPACE
