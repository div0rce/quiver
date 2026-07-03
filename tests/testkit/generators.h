// MOD-TESTKIT seeded input generation: deterministic from a uint64 seed, portable across
// platforms (integer-only core; floats built from bit patterns; no libm — REQ-INT-002).
// Implements the QLM-1 axis definitions (PRD 11 §4). The bench harness carries an
// independently maintained implementation of the same spec (REQ-BENCH-015); the drift-check
// conformance runner asserts byte-identical output for identical seeds.
// Module: MOD-TESTKIT | REQs: REQ-INT-002, REQ-TEST-012 | PRD 05 §7
#pragma once

#include <bit>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <type_traits>
#include <vector>

#include "quiver/core.h"

namespace quiver_test {

// SplitMix64 (Steele/Lea/Flood constants) — the testkit's single randomness source.
class Rng {
public:
  explicit Rng(std::uint64_t seed) : state_(seed) {}

  std::uint64_t next() {
    state_ += 0x9E3779B97F4A7C15ull;
    std::uint64_t z = state_;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
  }

  // Modulo mapping: negligible bias at test scale; determinism outranks uniformity here
  // (documented spec shared with the bench-local implementation).
  std::uint64_t next_below(std::uint64_t bound) { return bound == 0 ? 0 : next() % bound; }

  // Uniform double in [0, 1): 53 high bits, IEEE-exact on every platform.
  double next_unit() { return static_cast<double>(next() >> 11) * 0x1.0p-53; }

private:
  std::uint64_t state_;
};

// --- Value distributions (QLM-1 `value_dist` axis) -------------------------------------------

template <quiver::Element T>
void fill_sequential(T* out, std::int64_t n, T start) {
  T v = start;
  for (std::int64_t i = 0; i < n; ++i) {
    out[i] = v;
    v = static_cast<T>(v + T{1});
  }
}

template <quiver::Element T>
void fill_uniform(Rng& rng, T* out, std::int64_t n) {
  for (std::int64_t i = 0; i < n; ++i) {
    if constexpr (std::floating_point<T>) {
      // Finite, portable: unit double scaled to [0, 1e6), then narrowed.
      out[i] = static_cast<T>(rng.next_unit() * 1.0e6);
    } else {
      out[i] = static_cast<T>(rng.next());  // truncation is the spec
    }
  }
}

// Zipf θ=1.0 over 1,000 distinct values (QLM-1): CDF from harmonic partial sums (only
// IEEE +,/ — bit-deterministic), inverse-CDF via binary search. Emits codes 0..999 cast to T.
void fill_zipf_codes(Rng& rng, std::uint32_t* out, std::int64_t n);

template <quiver::Element T>
void fill_zipf(Rng& rng, T* out, std::int64_t n) {
  std::vector<std::uint32_t> codes(static_cast<std::size_t>(n));
  fill_zipf_codes(rng, codes.data(), n);
  for (std::int64_t i = 0; i < n; ++i) {
    out[i] = static_cast<T>(codes[static_cast<std::size_t>(i)]);
  }
}

// Boundary-heavy integer values and float specials (PRD 05 §7): fixed sets for unit tests.
template <quiver::Element T>
std::vector<T> boundary_values() {
  if constexpr (std::floating_point<T>) {
    using Bits = std::conditional_t<sizeof(T) == 4, std::uint32_t, std::uint64_t>;
    const Bits qnan = sizeof(T) == 4 ? Bits{0x7FC00000u} : Bits{0x7FF8000000000000ull};
    const Bits inf = sizeof(T) == 4 ? Bits{0x7F800000u} : Bits{0x7FF0000000000000ull};
    const Bits denorm = Bits{1};
    return {T{0},
            std::bit_cast<T>(static_cast<Bits>(Bits{1} << (sizeof(T) * 8 - 1))),  // -0.0
            std::bit_cast<T>(inf),
            static_cast<T>(-std::bit_cast<T>(inf)),
            std::bit_cast<T>(qnan),
            std::bit_cast<T>(denorm),
            T{1},
            T{-1}};
  } else {
    return {std::numeric_limits<T>::min(),
            static_cast<T>(std::numeric_limits<T>::min() + 1),
            static_cast<T>(-1),
            T{0},
            T{1},
            static_cast<T>(std::numeric_limits<T>::max() - 1),
            std::numeric_limits<T>::max()};
  }
}

// --- Bitmap patterns (QLM-1 `selectivity` × `pattern` axes; REQ-MEM-006 layout) ---------------

// Uniform: each bit set with probability p%; tail bits beyond n zeroed (ADR-016 discipline).
void fill_bitmap_uniform(Rng& rng, std::uint8_t* bits, std::int64_t n, int selectivity_pct);

// Clustered: alternating runs; each run selected with probability p%, run length geometric
// with mean 64 via Bernoulli trials (integer-only). Expected density = p%.
void fill_bitmap_clustered(Rng& rng, std::uint8_t* bits, std::int64_t n, int selectivity_pct);

void fill_bitmap_alternating(std::uint8_t* bits, std::int64_t n);

// Selection vector derived from a bitmap (strictly increasing by construction, ADR-025).
std::int64_t selvec_from_bitmap(const std::uint8_t* bits, std::int64_t n, std::uint32_t* out);

// --- Alignment-offset buffers (QLM-1 `alignment` axis; tests may allocate) --------------------

// 64-byte-aligned arena with slack; element pointers returned at a requested element offset.
template <class T>
class AlignedBuffer {
public:
  explicit AlignedBuffer(std::int64_t count, std::int64_t offset_elems = 0)
      : storage_(static_cast<std::size_t>(count + offset_elems) * sizeof(T) + 128) {
    void* p = storage_.data();
    std::size_t space = storage_.size();
    p = std::align(64, static_cast<std::size_t>(count + offset_elems) * sizeof(T), p, space);
    base_ = static_cast<T*>(p) + offset_elems;
  }
  T* data() { return base_; }
  const T* data() const { return base_; }

private:
  std::vector<unsigned char> storage_;
  T* base_;
};

// FNV-1a 64 over raw bytes — the golden-hash primitive for determinism self-tests
// (identical values are asserted on x86-64, ARM64, and macOS CI: M2 acceptance).
std::uint64_t fnv1a64(const void* data, std::size_t bytes);

}  // namespace quiver_test
