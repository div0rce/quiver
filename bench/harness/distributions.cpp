// MOD-BENCH distribution implementations — same spec as tests/testkit/generators.cpp,
// independently maintained (REQ-BENCH-015); drift_check enforces byte identity.
// Module: MOD-BENCH | REQs: REQ-BENCH-007, REQ-BENCH-015 | PRD 11 §4 (QLM-1)
#include "bench/harness/distributions.h"

#include <array>
#include <cstring>

namespace quiver::bench {

namespace {

const std::array<double, 1000>& zipf_cdf() {
  static const std::array<double, 1000> table = [] {
    std::array<double, 1000> t{};
    double h = 0.0;
    for (int k = 0; k < 1000; ++k) {
      h += 1.0 / static_cast<double>(k + 1);
      t[static_cast<std::size_t>(k)] = h;
    }
    const double total = t[999];
    for (auto& v : t) {
      v /= total;
    }
    return t;
  }();
  return table;
}

}  // namespace

void fill_zipf_codes(Rng& rng, std::uint32_t* out, std::int64_t n) {
  const auto& cdf = zipf_cdf();
  for (std::int64_t i = 0; i < n; ++i) {
    const double u = rng.next_unit();
    std::size_t lo = 0;
    std::size_t hi = cdf.size() - 1;
    while (lo < hi) {
      const std::size_t mid = (lo + hi) / 2;
      if (cdf[mid] >= u) {
        hi = mid;
      } else {
        lo = mid + 1;
      }
    }
    out[i] = static_cast<std::uint32_t>(lo);
  }
}

void fill_bitmap_uniform(Rng& rng, std::uint8_t* bits, std::int64_t n, int selectivity_pct) {
  const std::int64_t bytes = (n + 7) / 8;
  std::memset(bits, 0, static_cast<std::size_t>(bytes));
  for (std::int64_t i = 0; i < n; ++i) {
    if (rng.next_below(100) < static_cast<std::uint64_t>(selectivity_pct)) {
      bits[i >> 3] = static_cast<std::uint8_t>(bits[i >> 3] | (1u << (i & 7)));
    }
  }
}

namespace {
// Geometric(mean 64) run length via Bernoulli trials: continue the run while next()%64 != 0.
std::int64_t clustered_run_length(Rng& rng) {
  std::int64_t len = 1;
  while (rng.next_below(64) != 0) {
    ++len;
  }
  return len;
}

// Set every bit in [from, to).
void set_bit_range(std::uint8_t* bits, std::int64_t from, std::int64_t to) {
  for (std::int64_t i = from; i < to; ++i) {
    bits[i >> 3] = static_cast<std::uint8_t>(bits[i >> 3] | (1u << (i & 7)));
  }
}

}  // namespace

void fill_bitmap_clustered(Rng& rng, std::uint8_t* bits, std::int64_t n, int selectivity_pct) {
  const std::int64_t bytes = (n + 7) / 8;
  std::memset(bits, 0, static_cast<std::size_t>(bytes));
  std::int64_t i = 0;
  while (i < n) {
    const bool selected = rng.next_below(100) < static_cast<std::uint64_t>(selectivity_pct);
    const std::int64_t len = clustered_run_length(rng);
    const std::int64_t end = (i + len < n) ? i + len : n;
    if (selected) {
      set_bit_range(bits, i, end);
    }
    i = end;
  }
}

void fill_bitmap_alternating(std::uint8_t* bits, std::int64_t n) {
  const std::int64_t bytes = (n + 7) / 8;
  std::memset(bits, 0x55, static_cast<std::size_t>(bytes));
  const int tail = static_cast<int>(n & 7);
  if (tail != 0) {
    bits[bytes - 1] = static_cast<std::uint8_t>(0x55u & ((1u << tail) - 1u));
  }
}

std::int64_t selvec_from_bitmap(const std::uint8_t* bits, std::int64_t n, std::uint32_t* out) {
  std::int64_t count = 0;
  for (std::int64_t i = 0; i < n; ++i) {
    if ((bits[i >> 3] >> (i & 7)) & 1u) {
      out[count++] = static_cast<std::uint32_t>(i);
    }
  }
  return count;
}

}  // namespace quiver::bench
