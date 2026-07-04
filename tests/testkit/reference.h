// MOD-TESTKIT naive reference oracles — the *second*, independently written oracle beside
// each family's scalar `_impl.h` reference (dual-oracle scheme, REQ-TEST-002). Written
// deliberately simply (per-element loops over bit_get; no shared code with the library
// beyond the public types). A scalar/naive disagreement is a specification bug and blocks
// everything downstream (PRD 12 §2).
// Module: MOD-TESTKIT | REQs: REQ-TEST-002 | PRD 05 §7, PRD 12 §2
#pragma once

#include <cstdint>
#include <cstring>
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

// ADR-013 SIMD dense float-sum policy oracle (REQ-TEST-004), parameterized per backend
// (AVX2: f32 {w=8, a=4}, f64 {w=4, a=4}). Mirrors the backend arithmetic operation-for-
// operation so comparisons are bit-exact for non-NaN results: w*a-element blocks add
// elementwise into a*w scalar accumulators (invalid lanes add -0.0, the exact masked
// neutral); accumulators combine in the ADR-013 frozen pairwise order ((0+2),(1+3), then +),
// lanewise; lanes fold strictly low->high starting from +0.0; the tail (n mod w*a) adds
// sequentially, valid-only. Dense shape only — selected shapes use the strict fold
// (sum_expected) on every backend. NaN results must be compared as a CLASS (both-NaN ⇒
// match): IEEE add propagates the payload of whichever operand the hardware sees first, and
// C++ does not pin FP operand order, so payloads are not reproducible by any oracle.
template <class T>
T sum_blocked_expected(const T* in, std::int64_t n, const std::uint8_t* validity, int w, int a) {
  static_assert(std::is_floating_point_v<T>, "blocked-sum policy applies to floats only");
  std::vector<T> acc(static_cast<std::size_t>(w) * static_cast<std::size_t>(a), T{0});
  const std::int64_t block = static_cast<std::int64_t>(w) * a;
  std::int64_t i = 0;
  for (; i + block <= n; i += block) {
    for (int k = 0; k < a; ++k) {
      for (int lane = 0; lane < w; ++lane) {
        const std::int64_t idx = i + static_cast<std::int64_t>(k) * w + lane;
        acc[static_cast<std::size_t>(k * w + lane)] += valid(validity, idx) ? in[idx] : T{-0.0};
      }
    }
  }
  for (int step = a / 2; step >= 1; step /= 2) {  // frozen combine: (0+2),(1+3), then +
    for (int k = 0; k < step; ++k) {
      for (int lane = 0; lane < w; ++lane) {
        acc[static_cast<std::size_t>(k * w + lane)] +=
            acc[static_cast<std::size_t>((k + step) * w + lane)];
      }
    }
  }
  T s = T{0};
  for (int lane = 0; lane < w; ++lane) {
    s += acc[static_cast<std::size_t>(lane)];
  }
  for (; i < n; ++i) {
    if (valid(validity, i)) {
      s += in[i];
    }
  }
  return s;
}

// --- K7: independent qhash64 transcription (ADR-012's formula, written from the ADR text,
// --- not from the library source — the dual-oracle discipline) -------------------------------

inline std::uint64_t qhash64_fmix(std::uint64_t x) {
  x ^= x >> 33;
  x *= 0xFF51AFD7ED558CCDull;
  x ^= x >> 33;
  x *= 0xC4CEB9FE1A85EC53ull;
  x ^= x >> 33;
  return x;
}

template <class T>
std::uint64_t qhash64_expected(T v, std::uint64_t seed) {
  std::uint64_t key;
  if constexpr (std::is_same_v<T, float>) {
    float c = v;
    if (c == 0.0f) {
      c = 0.0f;
    }
    std::uint32_t bits;
    std::memcpy(&bits, &c, 4);
    key = bits;
  } else if constexpr (std::is_same_v<T, double>) {
    double c = v;
    if (c == 0.0) {
      c = 0.0;
    }
    std::memcpy(&key, &c, 8);
  } else {
    key = static_cast<std::uint64_t>(static_cast<std::make_unsigned_t<T>>(v));
  }
  return qhash64_fmix(key ^ (seed + 0x9E3779B97F4A7C15ull));
}

inline std::uint64_t qhash64_combine_expected(std::uint64_t a, std::uint64_t b) {
  return qhash64_fmix(a ^ (b + 0x9E3779B97F4A7C15ull + (a << 6) + (a >> 2)));
}

// --- K8: independent unpack oracle straight from the ADR-026 sentence: value i occupies
// --- bits [i*w, (i+1)*w); bit j lives at byte j/8, bit j%8 -----------------------------------

template <class Out>
Out unpack_value_expected(const std::uint8_t* packed, std::int64_t i, int w) {
  std::uint64_t v = 0;
  for (int k = 0; k < w; ++k) {
    const std::int64_t j = i * w + k;
    const std::uint64_t bit = (packed[j / 8] >> (j % 8)) & 1u;
    v |= bit << k;
  }
  return static_cast<Out>(v);
}

// Bit-setter for building packed streams in tests (the inverse of the oracle).
inline void pack_value(std::uint8_t* packed, std::int64_t i, int w, std::uint64_t v) {
  for (int k = 0; k < w; ++k) {
    const std::int64_t j = i * w + k;
    if ((v >> k) & 1u) {
      packed[j / 8] = static_cast<std::uint8_t>(packed[j / 8] | (1u << (j % 8)));
    }
  }
}

// --- K9/K10: wide-integer arithmetic oracles --------------------------------------------------

template <class T>
T arith_wrap_expected(quiver::ArithOp op, T a, T b) {
  if constexpr (std::is_floating_point_v<T>) {
    switch (op) {
    case quiver::ArithOp::kAdd:
      return a + b;
    case quiver::ArithOp::kSub:
      return a - b;
    default:
      return a * b;
    }
  } else {
    // Promote sub-int unsigned operands past int: u16*u16 would otherwise multiply as
    // SIGNED int (integer promotion) and overflow is UB.
    using U = std::make_unsigned_t<T>;
    using P = std::conditional_t<(sizeof(U) < sizeof(unsigned int)), unsigned int, U>;
    const P x = static_cast<U>(a);
    const P y = static_cast<U>(b);
    switch (op) {
    case quiver::ArithOp::kAdd:
      return static_cast<T>(static_cast<U>(x + y));
    case quiver::ArithOp::kSub:
      return static_cast<T>(static_cast<U>(x - y));
    default:
      return static_cast<T>(static_cast<U>(x * y));
    }
  }
}

// Overflow oracle via long double / __int128-free wide math: use the (2x-width or 128-bit)
// exact value where available; for 64-bit rely on the division-based check.
template <class T>
bool arith_overflows_expected(quiver::ArithOp op, T a, T b) {
  static_assert(std::is_integral_v<T>);
  if constexpr (sizeof(T) < 8) {
    using W = std::conditional_t<std::is_signed_v<T>, std::int64_t, std::uint64_t>;
    const W wide = static_cast<W>(a) + W{0};
    W exact;
    switch (op) {
    case quiver::ArithOp::kAdd:
      exact = wide + static_cast<W>(b);
      break;
    case quiver::ArithOp::kSub:
      exact = wide - static_cast<W>(b);
      break;
    default:
      exact = wide * static_cast<W>(b);
      break;
    }
    return exact < static_cast<W>(std::numeric_limits<T>::min()) ||
           exact > static_cast<W>(std::numeric_limits<T>::max());
  } else {
    // 64-bit: check per op with division/boundary logic (independent of the kernel's tricks).
    if constexpr (std::is_signed_v<T>) {
      switch (op) {
      case quiver::ArithOp::kAdd:
        return (b > 0 && a > std::numeric_limits<T>::max() - b) ||
               (b < 0 && a < std::numeric_limits<T>::min() - b);
      case quiver::ArithOp::kSub:
        return (b < 0 && a > std::numeric_limits<T>::max() + b) ||
               (b > 0 && a < std::numeric_limits<T>::min() + b);
      default:
        if (a == 0 || b == 0) {
          return false;
        }
        if (a == T{-1}) {
          return b == std::numeric_limits<T>::min();
        }
        if (b == T{-1}) {
          return a == std::numeric_limits<T>::min();
        }
        return a > 0 ? (b > 0 ? a > std::numeric_limits<T>::max() / b
                              : b < std::numeric_limits<T>::min() / a)
                     : (b > 0 ? a < std::numeric_limits<T>::min() / b
                              : a < std::numeric_limits<T>::max() / b);
      }
    } else {
      switch (op) {
      case quiver::ArithOp::kAdd:
        return a > std::numeric_limits<T>::max() - b;
      case quiver::ArithOp::kSub:
        return b > a;
      default:
        return b != 0 && a > std::numeric_limits<T>::max() / b;
      }
    }
  }
}

template <class T>
T arith_saturate_expected(quiver::ArithOp op, T a, T b) {
  if (!arith_overflows_expected(op, a, b)) {
    return arith_wrap_expected(op, a, b);
  }
  if constexpr (std::is_signed_v<T>) {
    switch (op) {
    case quiver::ArithOp::kAdd:
      return a < 0 ? std::numeric_limits<T>::min() : std::numeric_limits<T>::max();
    case quiver::ArithOp::kSub:
      return a < b ? std::numeric_limits<T>::min() : std::numeric_limits<T>::max();
    default:
      return (a < 0) != (b < 0) ? std::numeric_limits<T>::min() : std::numeric_limits<T>::max();
    }
  } else {
    return op == quiver::ArithOp::kSub ? std::numeric_limits<T>::min()
                                       : std::numeric_limits<T>::max();
  }
}

}  // namespace quiver_test::ref
