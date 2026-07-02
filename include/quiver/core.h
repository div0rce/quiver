// Surface B — vocabulary types, enums, and concepts. Exactly the inventory of PRD 04 §3.
// Module: MOD-CORE | REQs: REQ-CORE-001/-002, REQ-API-001/-004/-005 | ADRs: ADR-006, ADR-007
#pragma once

#include <concepts>
#include <cstdint>
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
// (PRD 08 §3.5).
template <Element T>
struct Sma {
  T min;
  T max;
  std::int64_t null_count;
};

namespace detail {
template <Element T>
struct SumTypeImpl {
  using type =
      std::conditional_t<std::floating_point<T>, T,
                         std::conditional_t<std::signed_integral<T>, std::int64_t, std::uint64_t>>;
};
}  // namespace detail

// int64_t for signed integers, uint64_t for unsigned, T for float/double (PRD 04 §3).
template <Element T>
using SumType = typename detail::SumTypeImpl<T>::type;

QUIVER_END_NAMESPACE
