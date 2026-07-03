// MOD-TESTKIT generator implementations. Spec shared with bench/harness/distributions.cpp
// (independently maintained by design, REQ-BENCH-015; drift_check asserts identity).
// Module: MOD-TESTKIT | REQs: REQ-INT-002 | PRD 05 §7, PRD 11 §4 (QLM-1)
#include "tests/testkit/generators.h"

#include <array>

#if !defined(_WIN32)
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace quiver_test {

namespace {

// Harmonic CDF for Zipf θ=1.0 over 1,000 values: cdf[k] = H_{k+1}/H_1000. Built once with
// IEEE + and / only (bit-deterministic across platforms).
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
    // Lowest k with cdf[k] >= u (binary search; deterministic comparisons).
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

void fill_bitmap_clustered(Rng& rng, std::uint8_t* bits, std::int64_t n, int selectivity_pct) {
  const std::int64_t bytes = (n + 7) / 8;
  std::memset(bits, 0, static_cast<std::size_t>(bytes));
  std::int64_t i = 0;
  while (i < n) {
    const bool selected = rng.next_below(100) < static_cast<std::uint64_t>(selectivity_pct);
    // Geometric(mean 64) via Bernoulli trials: continue the run while next()%64 != 0.
    std::int64_t len = 1;
    while (rng.next_below(64) != 0) {
      ++len;
    }
    for (std::int64_t j = 0; j < len && i < n; ++j, ++i) {
      if (selected) {
        bits[i >> 3] = static_cast<std::uint8_t>(bits[i >> 3] | (1u << (i & 7)));
      }
    }
  }
}

void fill_bitmap_alternating(std::uint8_t* bits, std::int64_t n) {
  const std::int64_t bytes = (n + 7) / 8;
  std::memset(bits, 0x55, static_cast<std::size_t>(bytes));  // bits 0,2,4,… set (LSB-first)
  const int tail = static_cast<int>(n & 7);
  if (tail != 0) {
    bits[bytes - 1] = static_cast<std::uint8_t>(0x55u & ((1u << tail) - 1u));  // ADR-016 tails
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

GuardedAlloc::GuardedAlloc(std::size_t bytes, Guard placement) {
#if !defined(_WIN32)
  const std::size_t page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
  const std::size_t payload_pages = (bytes + page - 1) / page;
  map_len_ = (payload_pages + 1) * page;
  base_ = mmap(nullptr, map_len_, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (base_ == MAP_FAILED) {
    base_ = nullptr;
    payload_ = nullptr;
    return;
  }
  auto* b = static_cast<unsigned char*>(base_);
  if (placement == Guard::kEnd) {
    // Payload ends flush against the protected trailing page.
    mprotect(b + payload_pages * page, page, PROT_NONE);
    payload_ = b + payload_pages * page - bytes;
  } else {
    // Payload starts flush after the protected leading page.
    mprotect(b, page, PROT_NONE);
    payload_ = b + page;
  }
#else
  // Windows tier-2: VirtualAlloc + PAGE_NOACCESS guard (compiled on that leg only).
  (void)bytes;
  (void)placement;
  payload_ = nullptr;
#endif
}

GuardedAlloc::~GuardedAlloc() {
#if !defined(_WIN32)
  if (base_ != nullptr) {
    munmap(base_, map_len_);
  }
#endif
}

std::uint64_t fnv1a64(const void* data, std::size_t bytes) {
  const auto* p = static_cast<const unsigned char*>(data);
  std::uint64_t h = 0xCBF29CE484222325ull;
  for (std::size_t i = 0; i < bytes; ++i) {
    h ^= p[i];
    h *= 0x100000001B3ull;
  }
  return h;
}

}  // namespace quiver_test
