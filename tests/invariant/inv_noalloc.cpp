// Invariant: no kernel allocates — global new/delete counters wrapped around every Tier A
// public API call (REQ-MEM-003; the symbol-scan half of the check runs in repo lint).
// Covers: REQ-MEM-003, REQ-TEST-005
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <vector>

#include <gtest/gtest.h>

#include "quiver/quiver.h"
#include "tests/testkit/generators.h"

// Sanitizers provide their own operator new/delete (interceptors or runtime definitions);
// replacing them causes alloc-dealloc mismatches (ASan) or duplicate symbols (TSan). The
// no-allocation invariant is validated in non-sanitized builds plus the repo-lint symbol
// scan (REQ-MEM-003); under sanitizers the test self-skips.
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__) || defined(__SANITIZE_MEMORY__)
#define QUIVER_NOALLOC_DISABLED 1
#endif
#if defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer) ||                         \
    __has_feature(memory_sanitizer)
#define QUIVER_NOALLOC_DISABLED 1
#endif
#endif

namespace {
std::atomic<std::int64_t> g_allocs{0};
}

#if !defined(QUIVER_NOALLOC_DISABLED)
// Global replacement operators count every allocation in the test binary.
void* operator new(std::size_t size) {
  g_allocs.fetch_add(1, std::memory_order_relaxed);
  if (void* p = std::malloc(size)) {
    return p;
  }
  std::abort();  // test binary: allocation failure is fatal (library never reaches here)
}
void operator delete(void* p) noexcept {
  std::free(p);
}
void operator delete(void* p, std::size_t) noexcept {
  std::free(p);
}
void* operator new[](std::size_t size) {
  return operator new(size);
}
void operator delete[](void* p) noexcept {
  std::free(p);
}
void operator delete[](void* p, std::size_t) noexcept {
  std::free(p);
}
#endif  // !QUIVER_NOALLOC_DISABLED

namespace {

TEST(InvNoAlloc, KernelCallsNeverAllocate) {
#if defined(QUIVER_NOALLOC_DISABLED)
  GTEST_SKIP() << "sanitizer builds own the allocator; no-alloc validated in plain builds "
                  "(REQ-MEM-003)";
#endif
  constexpr std::int64_t n = 4096;
  quiver_test::Rng rng(0x4A11);
  std::vector<std::int64_t> v(n);
  quiver_test::fill_uniform(rng, v.data(), n);
  std::vector<std::uint8_t> bits((n + 7) / 8);
  std::vector<std::uint8_t> bits2((n + 7) / 8);
  quiver_test::fill_bitmap_uniform(rng, bits2.data(), n, 50);
  std::vector<std::uint32_t> idx(n);
  std::vector<std::int64_t> out(n);
  quiver::warmup();  // resolve outside the measured window

  const std::int64_t before = g_allocs.load();
  quiver::compare_bitmap(quiver::CompareOp::kGt, quiver::BatchView<std::int64_t>{v.data(), n},
                         std::int64_t{0}, quiver::BitmapView{nullptr}, bits.data());
  quiver::mask_combine(quiver::MaskOp::kAnd, quiver::BitmapView{bits.data()},
                       quiver::BitmapView{bits2.data()}, n, bits.data());
  const std::int64_t cnt = quiver::bitmap_to_selvec(quiver::BitmapView{bits.data()}, n, idx.data());
  quiver::filter(quiver::BatchView<std::int64_t>{v.data(), n}, quiver::BitmapView{bits.data()},
                 out.data());
  quiver::take(quiver::BatchView<std::int64_t>{v.data(), n}, quiver::SelVec{idx.data(), cnt},
               out.data());
  (void)quiver::reduce_sum_wrap(quiver::BatchView<std::int64_t>{v.data(), n},
                                quiver::BitmapView{bits.data()});
  (void)quiver::compute_min_max(quiver::BatchView<std::int64_t>{v.data(), n},
                            quiver::BitmapView{bits.data()});
  const std::int64_t after = g_allocs.load();
  EXPECT_EQ(after, before) << "a kernel call allocated (REQ-MEM-003)";
}

}  // namespace
