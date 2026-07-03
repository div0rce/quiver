# Getting started

Quiver v0.1 ships the six Tier A kernel families (compare, filter, sel_convert, mask_algebra, take/dict_decode, reduce/SMA) with scalar backends and the full dispatch/testing infrastructure. SIMD backends land per milestone (AVX2 → NEON → AVX-512); the performance ledger arrives with v0.3.

## Build and link

```sh
git clone https://github.com/div0rce/quiver && cd quiver
cmake --preset release && cmake --build --preset release -j
```

Consume via `add_subdirectory(quiver)` + `target_link_libraries(app PRIVATE quiver::quiver)` (installed-package and amalgamation modes arrive at M8). Requirements: CMake ≥ 3.28, GCC ≥ 13 / Clang ≥ 17 / AppleClang ≥ 16 ([building guide](building.md)).

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
