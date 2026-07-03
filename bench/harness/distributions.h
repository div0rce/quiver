// MOD-BENCH bench-local input distributions implementing the QLM-1 axes (PRD 11 §4).
// DELIBERATELY duplicated from tests/testkit/generators.h (REQ-BENCH-015: benchmark code
// never depends on test code); tests/testkit/drift_check.cpp asserts byte-identical output
// for identical seeds. Any spec change updates BOTH implementations in one PR.
// This header is free of Google Benchmark so the drift check can compile it standalone.
// Module: MOD-BENCH | REQs: REQ-BENCH-007, REQ-BENCH-015 | ADR-008
#pragma once

#include <cstdint>
#include <type_traits>

namespace quiver::bench {

// SplitMix64 — identical spec to the testkit Rng.
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

  std::uint64_t next_below(std::uint64_t bound) { return bound == 0 ? 0 : next() % bound; }

  double next_unit() { return static_cast<double>(next() >> 11) * 0x1.0p-53; }

private:
  std::uint64_t state_;
};

template <class T>
void fill_sequential(T* out, std::int64_t n, T start) {
  T v = start;
  for (std::int64_t i = 0; i < n; ++i) {
    out[i] = v;
    v = static_cast<T>(v + T{1});
  }
}

template <class T>
void fill_uniform(Rng& rng, T* out, std::int64_t n) {
  for (std::int64_t i = 0; i < n; ++i) {
    if constexpr (std::is_floating_point_v<T>) {
      out[i] = static_cast<T>(rng.next_unit() * 1.0e6);
    } else {
      out[i] = static_cast<T>(rng.next());
    }
  }
}

void fill_zipf_codes(Rng& rng, std::uint32_t* out, std::int64_t n);
void fill_bitmap_uniform(Rng& rng, std::uint8_t* bits, std::int64_t n, int selectivity_pct);
void fill_bitmap_clustered(Rng& rng, std::uint8_t* bits, std::int64_t n, int selectivity_pct);
void fill_bitmap_alternating(std::uint8_t* bits, std::int64_t n);
std::int64_t selvec_from_bitmap(const std::uint8_t* bits, std::int64_t n, std::uint32_t* out);

}  // namespace quiver::bench
