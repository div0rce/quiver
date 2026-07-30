// MOD-TESTKIT generator implementations. Spec shared with bench/harness/distributions.cpp
// (independently maintained by design, REQ-BENCH-015; drift_check asserts identity).
// Module: MOD-TESTKIT | REQs: REQ-INT-002 | PRD 05 §7, PRD 11 §4 (QLM-1)
#include "tests/testkit/generators.h"

#include <array>

#if !defined(_WIN32)
#include <sys/mman.h>
#include <unistd.h>
#else
// NOMINMAX / WIN32_LEAN_AND_MEAN keep windows.h from defining min/max macros, which would
// break std::numeric_limits<>::max() at every later use site.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
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

namespace {

// Platform primitives, one job each, so the guard-placement policy below is written once
// instead of once per platform.

std::size_t guard_page_size() {
#if !defined(_WIN32)
  return static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
#else
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  return static_cast<std::size_t>(si.dwPageSize);
#endif
}

void* reserve_readwrite(std::size_t len) {
#if !defined(_WIN32)
  void* p = mmap(nullptr, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  return p == MAP_FAILED ? nullptr : p;
#else
  return VirtualAlloc(nullptr, len, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#endif
}

void release_mapping(void* base, std::size_t len) {
#if !defined(_WIN32)
  munmap(base, len);
#else
  (void)len;
  VirtualFree(base, 0, MEM_RELEASE);
#endif
}

// True on success. If protection fails the allocation is USELESS but not obviously broken: an
// over-read would land on ordinary readable memory and the test would pass while proving
// nothing. The caller fails loudly instead — callers assert data() != nullptr.
bool protect_no_access(void* at, std::size_t len) {
#if !defined(_WIN32)
  return mprotect(at, len, PROT_NONE) == 0;
#else
  DWORD prev = 0;
  return VirtualProtect(at, len, PAGE_NOACCESS, &prev) != 0;
#endif
}

}  // namespace

// One page is reserved beyond the payload and protected; the payload then sits flush against
// it, so any over-read or over-write past the requested bytes faults (REQ-TEST-006,
// REQ-MEM-008). The POSIX and Windows legs enforce the identical bound.
GuardedAlloc::GuardedAlloc(std::size_t bytes, Guard placement) {
  const std::size_t page = guard_page_size();
  const std::size_t payload_pages = (bytes + page - 1) / page;
  map_len_ = (payload_pages + 1) * page;
  base_ = reserve_readwrite(map_len_);
  if (base_ == nullptr) {
    payload_ = nullptr;
    return;
  }
  auto* b = static_cast<unsigned char*>(base_);
  const bool at_end = placement == Guard::kEnd;
  unsigned char* const guard = at_end ? b + payload_pages * page : b;
  if (!protect_no_access(guard, page)) {
    release_mapping(base_, map_len_);
    base_ = nullptr;
    payload_ = nullptr;
    return;
  }
  payload_ = at_end ? b + payload_pages * page - bytes : b + page;
}

GuardedAlloc::~GuardedAlloc() {
#if !defined(_WIN32)
  if (base_ != nullptr) {
    munmap(base_, map_len_);
  }
#else
  if (base_ != nullptr) {
    VirtualFree(base_, 0, MEM_RELEASE);  // size must be 0 with MEM_RELEASE
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
