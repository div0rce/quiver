// K6 reduce / SMA — scalar reference implementation (the family specification, Charter T3).
//
// Semantics (PRD 04 §5 K6, PRD 08 §3/§5 K6): an element participates iff selected ∧ valid
// (sel == nullptr means no selection; validity == nullptr means all-valid). Empty
// participation yields identities: min = numeric_limits<T>::max(), max = lowest(), sums = 0,
// Sma = {max(), lowest(), null_count}. Integer sums accumulate wrapping in SumType<T>
// (unsigned-internal idiom, REQ-STD-008). FLOAT SUMS ARE STRICT SEQUENTIAL LEFT FOLDS in
// participation order — the scalar backend IS the charter's strict-order recourse
// (ADR-013 as amended: scalar A=1; Charter §7.4). Float min/max propagate NaN as the
// canonical quiet NaN of T so results stay bit-identical across ISAs; -0.0 and +0.0 compare
// equal and the first-encountered is kept (deterministic). sum_checked reports exact
// mathematical unrepresentability via 128-bit accumulation (64-bit inputs only can overflow;
// narrow types are proven safe: 2^31 elements × 2^32 max < 2^63). SMA is one pass:
// null_count counts participating-position invalids (selected-but-invalid).
// Module: MOD-K6-REDUCE | REQs: REQ-K6-001..005 | ADR-013, ADR-014-adjacent (no silent UB)
#pragma once

#include <bit>
#include <cstdint>
#include <limits>
#include <type_traits>

#include "quiver/core.h"
#include "src/kernels/common/kernel_common.h"

QUIVER_BEGIN_NAMESPACE
namespace detail::scalar_impl {

// Canonical quiet NaN per PRD 08 §3.3 (payload-normalized for cross-ISA bit identity).
template <class T>
QUIVER_FORCE_INLINE T canonical_qnan() noexcept {
  if constexpr (sizeof(T) == 4) {
    return std::bit_cast<T>(std::uint32_t{0x7FC00000u});
  } else {
    return std::bit_cast<T>(std::uint64_t{0x7FF8000000000000ull});
  }
}

// Participation-ordered visitation: calls visit(value_index, is_valid) for each participating
// position — every kernel in this family is a fold over this order (the order IS the spec).
template <class Visit>
QUIVER_FORCE_INLINE void for_each_participant(std::int64_t n, const std::uint8_t* validity,
                                              const std::uint32_t* sel, std::int64_t sel_len,
                                              Visit visit) noexcept {
  if (sel == nullptr) {
    for (std::int64_t i = 0; i < n; ++i) {
      visit(i, is_valid(validity, i));
    }
  } else {
    for (std::int64_t j = 0; j < sel_len; ++j) {
      const std::int64_t i = static_cast<std::int64_t>(sel[j]);
      visit(i, is_valid(validity, i));
    }
  }
}

template <class T>
T reduce_min(const T* in, std::int64_t n, const std::uint8_t* validity,
             const std::uint32_t* sel, std::int64_t sel_len) noexcept {
  T best = std::numeric_limits<T>::max();  // identity (PRD 08 §3.5)
  bool saw_nan = false;
  for_each_participant(n, validity, sel, sel_len, [&](std::int64_t i, bool valid) {
    if (valid) {
      const T v = in[i];
      if constexpr (std::is_floating_point_v<T>) {
        saw_nan = saw_nan || (v != v);
      }
      best = (v < best) ? v : best;  // ties/±0.0: first-encountered kept (deterministic)
    }
  });
  if constexpr (std::is_floating_point_v<T>) {
    if (saw_nan) {
      return canonical_qnan<T>();  // NaN propagation, payload-normalized (PRD 08 §3.3)
    }
  }
  return best;
}

template <class T>
T reduce_max(const T* in, std::int64_t n, const std::uint8_t* validity,
             const std::uint32_t* sel, std::int64_t sel_len) noexcept {
  T best = std::numeric_limits<T>::lowest();
  bool saw_nan = false;
  for_each_participant(n, validity, sel, sel_len, [&](std::int64_t i, bool valid) {
    if (valid) {
      const T v = in[i];
      if constexpr (std::is_floating_point_v<T>) {
        saw_nan = saw_nan || (v != v);
      }
      best = (v > best) ? v : best;
    }
  });
  if constexpr (std::is_floating_point_v<T>) {
    if (saw_nan) {
      return canonical_qnan<T>();
    }
  }
  return best;
}

template <class T>
SumType<T> reduce_sum_wrap(const T* in, std::int64_t n, const std::uint8_t* validity,
                           const std::uint32_t* sel, std::int64_t sel_len) noexcept {
  using S = SumType<T>;
  if constexpr (std::is_floating_point_v<T>) {
    // STRICT sequential left fold — the charter's strict-order recourse (ADR-013, scalar A=1).
    S acc = S{0};
    for_each_participant(n, validity, sel, sel_len, [&](std::int64_t i, bool valid) {
      if (valid) {
        acc += in[i];
      }
    });
    return acc;
  } else {
    using U = std::make_unsigned_t<S>;
    U acc = 0;  // wrapping accumulation, unsigned-internal (REQ-STD-008)
    for_each_participant(n, validity, sel, sel_len, [&](std::int64_t i, bool valid) {
      if (valid) {
        // Sign-extend to S first (defined), then reinterpret as U (wrapping add is the spec).
        acc = static_cast<U>(acc + static_cast<U>(static_cast<S>(in[i])));
      }
    });
    return static_cast<S>(acc);
  }
}

// Exact representability check via 128-bit accumulation (GCC/Clang tier-1; MSVC tier-2 uses
// the same path via __int128 when available — enforced by the toolchain matrix, PRD 03 §7).
template <class T>
bool reduce_sum_checked(const T* in, std::int64_t n, const std::uint8_t* validity,
                        const std::uint32_t* sel, std::int64_t sel_len,
                        SumType<T>* out_sum) noexcept {
  using S = SumType<T>;
  static_assert(std::is_integral_v<T>, "checked sums are integer-only (PRD 04 K6)");
#if defined(__SIZEOF_INT128__)
  if constexpr (std::is_signed_v<T>) {
    __int128 acc = 0;
    for_each_participant(n, validity, sel, sel_len, [&](std::int64_t i, bool valid) {
      if (valid) {
        acc += static_cast<__int128>(in[i]);
      }
    });
    *out_sum = static_cast<S>(acc);  // wrapped value on overflow (API-K6-003)
    return acc > static_cast<__int128>(std::numeric_limits<S>::max()) ||
           acc < static_cast<__int128>(std::numeric_limits<S>::min());
  } else {
    // Unsigned: track 128-bit magnitude; overflow iff it exceeds the 64-bit range.
    unsigned __int128 acc = 0;
    for_each_participant(n, validity, sel, sel_len, [&](std::int64_t i, bool valid) {
      if (valid) {
        acc += static_cast<unsigned __int128>(in[i]);
      }
    });
    *out_sum = static_cast<S>(acc);
    return acc > static_cast<unsigned __int128>(std::numeric_limits<S>::max());
  }
#else
  // No 128-bit type (MSVC x64 tier-2): carry-tracked 64-bit accumulation.
  if constexpr (std::is_signed_v<T>) {
    S acc = 0;
    bool overflowed = false;
    for_each_participant(n, validity, sel, sel_len, [&](std::int64_t i, bool valid) {
      if (valid) {
        const S v = static_cast<S>(in[i]);
        using U = std::make_unsigned_t<S>;
        const S r = static_cast<S>(static_cast<U>(acc) + static_cast<U>(v));
        overflowed = overflowed || (((acc ^ r) & (v ^ r)) < 0);
        acc = r;
      }
    });
    *out_sum = acc;
    return overflowed;
  } else {
    S acc = 0;
    bool overflowed = false;
    for_each_participant(n, validity, sel, sel_len, [&](std::int64_t i, bool valid) {
      if (valid) {
        const S v = static_cast<S>(in[i]);
        const S r = static_cast<S>(acc + v);
        overflowed = overflowed || (r < acc);
        acc = r;
      }
    });
    *out_sum = acc;
    return overflowed;
  }
#endif
}

template <class T>
Sma<T> compute_sma(const T* in, std::int64_t n, const std::uint8_t* validity,
                   const std::uint32_t* sel, std::int64_t sel_len) noexcept {
  Sma<T> sma{std::numeric_limits<T>::max(), std::numeric_limits<T>::lowest(), 0};
  bool saw_nan = false;
  for_each_participant(n, validity, sel, sel_len, [&](std::int64_t i, bool valid) {
    if (!valid) {
      ++sma.null_count;  // selected-but-invalid (PRD 04 K6-004)
      return;
    }
    const T v = in[i];
    if constexpr (std::is_floating_point_v<T>) {
      saw_nan = saw_nan || (v != v);
    }
    sma.min = (v < sma.min) ? v : sma.min;
    sma.max = (v > sma.max) ? v : sma.max;
  });
  if constexpr (std::is_floating_point_v<T>) {
    if (saw_nan) {
      sma.min = canonical_qnan<T>();
      sma.max = canonical_qnan<T>();
    }
  }
  return sma;
}

}  // namespace detail::scalar_impl
QUIVER_END_NAMESPACE
