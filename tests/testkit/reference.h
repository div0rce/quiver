// MOD-TESTKIT naive reference oracles — the *second*, independently written oracle beside
// each family's scalar `_impl.h` reference (dual-oracle scheme, REQ-TEST-002). Written
// deliberately simply (per-element loops over bit_get; no shared code with the library
// beyond the public types). A scalar/naive disagreement is a specification bug and blocks
// everything downstream (PRD 12 §2).
// Module: MOD-TESTKIT | REQs: REQ-TEST-002 | PRD 05 §7, PRD 12 §2
#pragma once

#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>

#include "quiver/core.h"

namespace quiver_test::ref {

// --- LSB-first bit primitives (REQ-MEM-006 layout) ---------------------------------------

inline bool bit_get(const std::uint8_t* bits, std::int64_t i) {
  return ((bits[i >> 3] >> (i & 7)) & 1u) != 0;
}

inline void bit_set(std::uint8_t* bits, std::int64_t i) {
  bits[i >> 3] = static_cast<std::uint8_t>(bits[i >> 3] | (1u << (i & 7)));
}

inline bool valid(const std::uint8_t* validity, std::int64_t i) {
  return validity == nullptr || bit_get(validity, i);
}

inline std::int64_t popcount_bits(const std::uint8_t* bits, std::int64_t n) {
  std::int64_t c = 0;
  for (std::int64_t i = 0; i < n; ++i) {
    c += bit_get(bits, i) ? 1 : 0;
  }
  return c;
}

// Tail-bit check (ADR-016): true iff every bit at position >= n in the final byte is zero.
inline bool tail_bits_zero(const std::uint8_t* bits, std::int64_t n) {
  const int tail = static_cast<int>(n & 7);
  if (tail == 0 || n == 0) {
    return true;
  }
  const std::uint8_t last = bits[(n - 1) >> 3];
  return (last & static_cast<std::uint8_t>(~((1u << tail) - 1u))) == 0;
}

// --- K1: predicate evaluation --------------------------------------------------------------

template <class T>
bool compare_one(quiver::CompareOp op, T a, T b) {
  switch (op) {
  case quiver::CompareOp::kEq:
    return a == b;
  case quiver::CompareOp::kNe:
    return !(a == b);
  case quiver::CompareOp::kLt:
    return a < b;
  case quiver::CompareOp::kLe:
    return a <= b;
  case quiver::CompareOp::kGt:
    return a > b;
  case quiver::CompareOp::kGe:
    return a >= b;
  }
  return false;
}

// Emits the expected bitmap into `out` (pre-zeroed by the caller) and returns the count.
template <class Pred>
std::int64_t predicate_bitmap(std::int64_t n, std::uint8_t* out, Pred pred) {
  std::int64_t count = 0;
  for (std::int64_t i = 0; i < n; ++i) {
    if (pred(i)) {
      bit_set(out, i);
      ++count;
    }
  }
  return count;
}

// --- K2/K3: compaction and conversion -------------------------------------------------------

template <class T>
std::vector<T> filter_expected(const T* in, std::int64_t n, const std::uint8_t* selection) {
  std::vector<T> out;
  for (std::int64_t i = 0; i < n; ++i) {
    if (bit_get(selection, i)) {
      out.push_back(in[i]);
    }
  }
  return out;
}

inline std::vector<std::uint32_t> selvec_expected(const std::uint8_t* selection, std::int64_t n) {
  std::vector<std::uint32_t> out;
  for (std::int64_t i = 0; i < n; ++i) {
    if (bit_get(selection, i)) {
      out.push_back(static_cast<std::uint32_t>(i));
    }
  }
  return out;
}

// --- K6: reductions (participation = selected AND valid; identities on empty) ---------------

struct Participation {
  const std::uint8_t* validity;
  const std::uint32_t* sel;
  std::int64_t sel_len;
};

template <class T, class Fold>
void fold_participants(const T* in, std::int64_t n, const Participation& p, Fold fold) {
  if (p.sel == nullptr) {
    for (std::int64_t i = 0; i < n; ++i) {
      fold(in[i], valid(p.validity, i));
    }
  } else {
    for (std::int64_t j = 0; j < p.sel_len; ++j) {
      fold(in[p.sel[j]], valid(p.validity, static_cast<std::int64_t>(p.sel[j])));
    }
  }
}

template <class T>
T min_expected(const T* in, std::int64_t n, const Participation& p) {
  T best = std::numeric_limits<T>::max();
  bool nan = false;
  fold_participants(in, n, p, [&](T v, bool ok) {
    if (ok) {
      if constexpr (std::is_floating_point_v<T>) {
        nan = nan || v != v;
      }
      if (v < best) {
        best = v;
      }
    }
  });
  if constexpr (std::is_floating_point_v<T>) {
    if (nan) {
      return std::numeric_limits<T>::quiet_NaN();
    }
  }
  return best;
}

template <class T>
T max_expected(const T* in, std::int64_t n, const Participation& p) {
  T best = std::numeric_limits<T>::lowest();
  bool nan = false;
  fold_participants(in, n, p, [&](T v, bool ok) {
    if (ok) {
      if constexpr (std::is_floating_point_v<T>) {
        nan = nan || v != v;
      }
      if (v > best) {
        best = v;
      }
    }
  });
  if constexpr (std::is_floating_point_v<T>) {
    if (nan) {
      return std::numeric_limits<T>::quiet_NaN();
    }
  }
  return best;
}

// Strict participation-order left fold — exactly the ADR-013 scalar (A=1) policy, so float
// expectations are bit-exact against the scalar backend (REQ-TEST-004).
template <class T>
quiver::SumType<T> sum_expected(const T* in, std::int64_t n, const Participation& p) {
  using S = quiver::SumType<T>;
  if constexpr (std::is_floating_point_v<T>) {
    S acc = S{0};
    fold_participants(in, n, p, [&](T v, bool ok) {
      if (ok) {
        acc += v;
      }
    });
    return acc;
  } else {
    using U = std::make_unsigned_t<S>;
    U acc = 0;
    fold_participants(in, n, p, [&](T v, bool ok) {
      if (ok) {
        acc = static_cast<U>(acc + static_cast<U>(static_cast<S>(v)));
      }
    });
    return static_cast<S>(acc);
  }
}

}  // namespace quiver_test::ref
