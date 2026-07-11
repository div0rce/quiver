// Surface B — vocabulary types, enums, concepts, and the zero-cost convenience helpers.
// Exactly the inventory of PRD 04 §3 (+ the §3.6 convenience additions, ADR-027).
// Module: MOD-CORE | REQs: REQ-CORE-001/-002, REQ-API-001/-004/-005 | ADRs: ADR-006, ADR-007,
// ADR-027
#pragma once

#include <concepts>
#include <cstdint>
#include <ranges>
#include <span>
#include <type_traits>

#include "quiver/detail/config.h"

QUIVER_BEGIN_NAMESPACE

// Batch length cap: selection indices fit uint32_t with headroom; batches are cache-sized
// by design (REQ-API-005).
inline constexpr std::int64_t kMaxBatchLen = 2'147'483'647;

// Exactly the ten supported element types (REQ-API-004); anything else fails to compile.
template <class T>
concept Element = std::same_as<T, std::int8_t> || std::same_as<T, std::int16_t> ||
                  std::same_as<T, std::int32_t> || std::same_as<T, std::int64_t> ||
                  std::same_as<T, std::uint8_t> || std::same_as<T, std::uint16_t> ||
                  std::same_as<T, std::uint32_t> || std::same_as<T, std::uint64_t> ||
                  std::same_as<T, float> || std::same_as<T, double>;

template <class T>
concept IntElement = Element<T> && std::integral<T>;

// Dictionary code types (REQ-API-004).
template <class T>
concept CodeType = std::same_as<T, std::uint8_t> || std::same_as<T, std::uint16_t> ||
                   std::same_as<T, std::uint32_t>;

// Unpack output types — exactly the four unsigned widths (API-K8-001; concept-diagnosed
// per REQ-API-004's instantiation rule).
template <class T>
concept UnpackOut = std::same_as<T, std::uint8_t> || std::same_as<T, std::uint16_t> ||
                    std::same_as<T, std::uint32_t> || std::same_as<T, std::uint64_t>;

// Enum values are frozen numbers: dispatch tables index by Isa (REQ-DISP-002 preference order).
enum class CompareOp : std::uint8_t { kEq, kNe, kLt, kLe, kGt, kGe };
enum class MaskOp : std::uint8_t { kAnd, kOr, kAndNot, kXor };
enum class ArithOp : std::uint8_t { kAdd, kSub, kMul };
enum class Isa : std::uint8_t { kScalar = 0, kNeon = 1, kAvx2 = 2, kAvx512 = 3 };

// Non-owning views: trivially copyable, standard-layout, no constructors or methods —
// contracts live at kernel boundaries, not in the types (REQ-CORE-002, Charter T6).
template <Element T>
struct BatchView {
  const T* data;
  std::int64_t len;
};

// LSB-first, 1 = valid/selected (REQ-MEM-006, Arrow-compatible). bits == nullptr means
// "all valid" only where a parameter is named `validity` (REQ-API-008).
struct BitmapView {
  const std::uint8_t* bits;
};

// Selection semantics: strictly increasing, in-range (ADR-025); `take` relaxes to arbitrary
// in-range indices (REQ-MEM-007).
struct SelVec {
  const std::uint32_t* idx;
  std::int64_t len;
};

// Identity values when no valid element participates: min = numeric max, max = numeric lowest
// (PRD 08 §3.5). Renamed from `Sma` in 0.8 (ADR-027): the old name read as "simple moving
// average", which this never was — it is the fused single-pass min/max/null-count summary.
template <Element T>
struct MinMaxSummary {
  T min;
  T max;
  std::int64_t null_count;
};

template <Element T>
using Sma [[deprecated("renamed to MinMaxSummary in 0.8; the Sma alias is removed at v1.0")]] =
    MinMaxSummary<T>;

// --- Zero-cost convenience helpers (PRD 04 §3.6, ADR-027). No allocation, no behavior change:
// --- pure spelling improvements over the buffer-oriented core.

// "All rows valid" — the named form of BitmapView{nullptr} for `validity` parameters.
inline constexpr BitmapView all_valid{nullptr};

// Bytes a bitmap over `row_count` rows occupies: the ⌈n/8⌉ every caller otherwise rewrites.
constexpr std::size_t bitmap_bytes(std::int64_t row_count) noexcept {
  return static_cast<std::size_t>((row_count + 7) / 8);
}

// A contiguous range of Element values (vector, span, array...) usable as a batch source.
template <class R>
concept BatchRange = std::ranges::contiguous_range<R> && std::ranges::sized_range<R> &&
                     Element<std::remove_cv_t<std::ranges::range_value_t<R>>>;

// A contiguous range of selection indices.
template <class R>
concept IndexRange = std::ranges::contiguous_range<R> && std::ranges::sized_range<R> &&
                     std::same_as<std::remove_cv_t<std::ranges::range_value_t<R>>, std::uint32_t>;

// View builders: `quiver::batch_view(my_vector)` instead of spelling BatchView<T>{data, size}.
template <BatchRange R>
constexpr auto batch_view(const R& values) noexcept {
  using T = std::remove_cv_t<std::ranges::range_value_t<R>>;
  return BatchView<T>{std::ranges::data(values),
                      static_cast<std::int64_t>(std::ranges::size(values))};
}

template <IndexRange R>
constexpr SelVec selection_view(const R& indices) noexcept {
  return SelVec{std::ranges::data(indices), static_cast<std::int64_t>(std::ranges::size(indices))};
}

namespace detail {
template <Element T>
struct SumTypeImpl {
  using type =
      std::conditional_t<std::floating_point<T>, T,
                         std::conditional_t<std::signed_integral<T>, std::int64_t, std::uint64_t>>;
};

// The concrete K5/K6 symbols encode "no selection" (dense) as sel == nullptr, so façades
// taking a SelVec must map a caller's EMPTY selection — idx == nullptr with len == 0, e.g.
// std::vector::data() of an empty vector — to a non-null pointer, keeping "empty selection"
// distinct from "no selection". Without this, an empty SelVec silently selected EVERYTHING
// (defect found by differential fuzzing at M4; tests/regression/reg_empty_selvec.cpp).
inline constexpr std::uint32_t kEmptySelSentinel = 0;
constexpr const std::uint32_t* nonnull_sel(const std::uint32_t* idx) noexcept {
  return idx != nullptr ? idx : &kEmptySelSentinel;
}
}  // namespace detail

// int64_t for signed integers, uint64_t for unsigned, T for float/double (PRD 04 §3).
template <Element T>
using SumType = typename detail::SumTypeImpl<T>::type;

QUIVER_END_NAMESPACE
