# Getting started

Quiver is a small C++23 library of fast analytical building blocks (compare, filter, mask, gather, reduce, hash, unpack, checked arithmetic). As of v0.6.0 all ten operation groups are implemented on scalar, AVX2, and NEON, with the AVX-512 versions correctness-validated under Intel's CPU emulator. This page takes you from cloning the repo to running a first working pipeline. See the [project README](https://github.com/div0rce/quiver#readme) for the wider picture.

## Build and link

```sh
git clone https://github.com/div0rce/quiver && cd quiver
cmake --preset release && cmake --build --preset release -j
```

Consume Quiver three ways: an installed CMake package (`find_package(Quiver CONFIG)`), a source subproject (`add_subdirectory(quiver)` or `FetchContent`, then `target_link_libraries(app PRIVATE quiver::quiver)`), or the single-file drop-in. All three are covered in the [vendoring guide](vendoring.md). Requirements: CMake 3.28 or newer, GCC 13+ / Clang 17+ / AppleClang 16+ ([building guide](building.md)).

## First pipeline: filter → gather → reduce

```cpp
#include "quiver/quiver.h"

// Caller owns every buffer (Quiver never allocates): values, a validity bitmap,
// and scratch for the selection + outputs.
std::int64_t select_and_sum(const std::int64_t* values, std::int64_t n,
                            const std::uint8_t* validity, std::int64_t threshold,
                            std::uint8_t* sel_bits, std::int64_t* filtered) {
  // 1. Predicate -> selection bitmap (branch-free; NULLs excluded by validity).
  quiver::compare_bitmap(quiver::CompareOp::kGt,
                         quiver::BatchView<std::int64_t>{values, n}, threshold,
                         quiver::BitmapView{validity}, sel_bits);
  // 2. Compact the survivors (in-place aliasing would also be legal here).
  const std::int64_t count = quiver::filter(quiver::BatchView<std::int64_t>{values, n},
                                            quiver::BitmapView{sel_bits}, filtered);
  // 3. Reduce the dense result.
  return quiver::reduce_sum_wrap(quiver::BatchView<std::int64_t>{filtered, count},
                                 quiver::BitmapView{nullptr});
}
```

Key contracts to know before writing more: batch lengths are `0 ≤ n ≤ 2³¹−1`; bitmaps are LSB-first with 1 = valid/selected and zeroed tail bits; selection vectors are strictly increasing `uint32_t`; count-returning compaction outputs need capacity for `n` elements (the defined output is the first *count*); everything is `noexcept` and allocation-free. Full reference: [API pages](../api/README.md), [PRD 04](../prd/04-public-api.md)/[06](../prd/06-memory-model.md).

## Choosing an ISA explicitly

Dispatch picks the best backend automatically. For benchmarking or diagnostics, cap it with `QUIVER_ISA=scalar` (environment) or `quiver::set_isa_override(...)` ([dispatch reference](../api/dispatch.md)).
