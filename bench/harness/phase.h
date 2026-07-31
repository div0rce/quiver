// MOD-BENCH buffer placement: micro-bench operand/result buffers allocate at fixed, named
// 4 KiB phases, so the memory-pipeline interaction between load and store streams is part of
// the methodology rather than allocator luck (REQ-BENCH-013 fixed-layout school, Survey §7.3).
//
// Why (measured on intel-coffee-lake, ledger runs 20260730-373ec8eec66b and
// 20260731-8f47459a8261): glibc places consecutive 32 KiB vector allocations 16 B apart in
// 4 KiB phase (heap chunks pack at +size+16; ≥128 KiB chunks are mmap'd to page+16), so every
// operand load shares its page offset, to within the conflict granule, with an in-flight store
// to the result buffer. The core then flags a false store→load dependence on nearly every load
// (ld_blocks_partial.address_alias ≈ mem_inst_retired.all_loads: 0.92–0.97 for scalar, avx2,
// and autovec variants alike). The per-hit stall is timing-sensitive: short AVX2 loops settle
// per process into distinct steady states — identical retired instructions and core frequency,
// up to +42% cycles, the delta entirely cycle_activity.stalls_mem_any — which pushed
// BM_arith_guarded avx2 cross-repetition CV to 0.12–0.24 against the 0.05 ledger screen and
// erased every REQ-BENCH-010 verdict pair.
//
// Placement rule: streams that advance at the same bytes-per-element keep a constant phase
// delta δ = (load_phase − store_phase) mod 4096. A load can falsely match a store still in the
// store buffer when δ − Δ lands inside the ±64 B conflict granule, for Δ ∈ (0, 1792] trailing
// store bytes (Coffee Lake: 56 store-buffer entries × 32 B AVX2 stores). Safe band:
// δ ∈ [64, 2240]. Operand loads at +1024 and +2048 against result stores at +0 sit centered in
// it. Rate-mismatched streams (an overflow bitmap advancing at a fraction of the value rate)
// sweep δ and cannot be phase-pinned; their false-conflict crossings proved to be a real
// variance source of their own, so the K10 kernel batches its bitmap stores to widen that
// stream's stride (see arith_guarded_avx2.cpp blocks()). All phases are 64 B multiples, so
// data() stays cache-line aligned; ISA and variant see identical placement, keeping
// REQ-BENCH-002 verdict pairs fair.
// Module: MOD-BENCH | REQs: REQ-BENCH-002/-013 | ADR-008
#pragma once

#include <cstddef>
#include <cstring>
#include <new>
#include <vector>

#if defined(__linux__)
#include <sys/mman.h>
#endif

namespace quiver::bench {

inline constexpr std::size_t kPhasePage = 4096;
inline constexpr std::size_t kLoadPhaseA = 1024;     // first operand stream
inline constexpr std::size_t kLoadPhaseB = 2048;     // second operand stream
inline constexpr std::size_t kStorePhase = 0;        // primary result stream
inline constexpr std::size_t kAuxStorePhase = 3200;  // low-rate secondary result stream

// Page phase alone is not enough: L2 and L3 are physically indexed, so with 4 KiB pages the
// cache-set placement of each buffer still depends on which physical frames the kernel hands
// this process — per-process luck that survives every virtual-address control. All phased
// buffers therefore carve from ONE 2 MiB-aligned arena marked MADV_HUGEPAGE: under a single
// huge page, physical-address bits below 21 equal the virtual ones, making the buffers'
// relative cache-set geometry identical in every process. If the region cannot be huge-backed
// the carving still fixes the virtual layout and the entry degrades to today's behavior.
// Single-threaded by design, like the bench binaries (Google Benchmark threads=1 here).
inline constexpr std::size_t kPhaseArenaBytes = std::size_t{4} << 20;  // 3×512 KiB + bitmap + slack
inline constexpr std::size_t kPhaseArenaAlign = std::size_t{2} << 20;  // one 2 MiB TLB/THP unit

inline std::byte* phased_arena() {
  static std::byte* arena = [] {
    std::byte* raw = nullptr;
#if defined(__linux__)
    // Preferred backing: explicit 2 MiB huge pages (vm.nr_hugepages, part of the machine's
    // REQ-BENCH-013 environment preparation). One PMD mapping pins the buffers' physical
    // relative layout outright. MADV_HUGEPAGE alone is not relied on — kernels routinely
    // decline it (this machine's grants zero THPs), and a silent fallback to 4 KiB frames
    // would quietly reintroduce the per-process cache-set luck this arena exists to remove.
    void* hp = mmap(nullptr, kPhaseArenaBytes, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
    if (hp != MAP_FAILED) {
      raw = static_cast<std::byte*>(hp);
    }
#endif
    if (raw == nullptr) {
      raw = static_cast<std::byte*>(
          ::operator new(kPhaseArenaBytes, std::align_val_t{kPhaseArenaAlign}));
#if defined(__linux__)
      (void)madvise(raw, kPhaseArenaBytes, MADV_HUGEPAGE);
#endif
    }
    // First-touch at setup, outside any timed region, so faults never land in a measurement.
    std::memset(raw, 0, kPhaseArenaBytes);
    return raw;
  }();
  return arena;
}

inline bool phased_arena_owns(const void* p) {
  const auto* base = phased_arena();
  const auto* q = static_cast<const std::byte*>(p);
  return q >= base && q < base + kPhaseArenaBytes;
}

// Bump state: `used` is the high-water mark, `live` the count of outstanding carves. Google
// Benchmark invokes a benchmark body several times while calibrating toward --benchmark_min_time,
// and every invocation allocates its buffers afresh; without reclamation the arena would exhaust
// after a few calibration rounds and the TIMED invocation would silently run on fallback
// allocations, losing the pinned physical layout. Each invocation destroys all its vectors before
// the next begins, so resetting the bump pointer when the last live carve is returned makes every
// invocation replay the identical carve sequence — same addresses, every time.
inline std::size_t& phased_arena_used() {
  static std::size_t used = 0;
  return used;
}
inline std::size_t& phased_arena_live() {
  static std::size_t live = 0;
  return live;
}

inline std::byte* phased_arena_carve(std::size_t bytes, std::size_t phase) {
  std::size_t& used = phased_arena_used();
  const std::size_t start = ((used + kPhasePage - 1) / kPhasePage) * kPhasePage + phase;
  if (start + bytes > kPhaseArenaBytes) {
    return nullptr;  // arena exhausted: caller falls back to a plain phased allocation
  }
  used = start + bytes;
  ++phased_arena_live();
  return phased_arena() + start;
}

inline void phased_arena_release() {
  if (--phased_arena_live() == 0) {
    phased_arena_used() = 0;
  }
}

// Minimal stateful allocator: data() sits `phase` bytes into a 4 KiB boundary, carved from the
// shared arena above (or from a plain 4 KiB-aligned block if the arena is exhausted).
// std::vector keeps the allocator (and therefore the phase) across resize reallocations.
template <class T>
class PhasedAlloc {
public:
  using value_type = T;

  PhasedAlloc() noexcept = default;
  explicit PhasedAlloc(std::size_t phase) noexcept : phase_(phase) {}
  template <class U>
  PhasedAlloc(const PhasedAlloc<U>& other) noexcept : phase_(other.phase()) {}

  T* allocate(std::size_t n) {
    if (std::byte* carved = phased_arena_carve(n * sizeof(T), phase_)) {
      return reinterpret_cast<T*>(carved);
    }
    auto* raw = static_cast<std::byte*>(
        ::operator new(n * sizeof(T) + phase_, std::align_val_t{kPhasePage}));
    return reinterpret_cast<T*>(raw + phase_);
  }
  void deallocate(T* p, std::size_t /*n*/) noexcept {
    if (phased_arena_owns(p)) {
      phased_arena_release();  // last live carve resets the bump pointer for the next invocation
    } else {
      ::operator delete(reinterpret_cast<std::byte*>(p) - phase_, std::align_val_t{kPhasePage});
    }
  }

  std::size_t phase() const noexcept { return phase_; }

  friend bool operator==(const PhasedAlloc& lhs, const PhasedAlloc& rhs) noexcept {
    return lhs.phase_ == rhs.phase_;
  }

private:
  std::size_t phase_ = 0;
};

template <class T>
using PhasedVec = std::vector<T, PhasedAlloc<T>>;

// n value-initialized elements whose data() sits at `phase` bytes into a 4 KiB page.
template <class T>
PhasedVec<T> phased_vec(std::size_t n, std::size_t phase) {
  return PhasedVec<T>(n, PhasedAlloc<T>(phase));
}

}  // namespace quiver::bench
