// K5 take / dict_decode — scalar reference implementation (the family specification, T3).
// Semantics (PRD 04 §5 K5): take permits arbitrary order and duplicates, every index
// in-bounds (debug-asserted with a full scan under QUIVER_ENABLE_ASSERTS, zero release cost
// — REQ-K5-002/REQ-ERR-004); dict_decode bounds-checks codes the same way; the fused form
// touches ONLY selected code positions and packs its output (REQ-K5-003). Unrolled
// independent loads expose memory-level parallelism (PRD 08 §5 K5; Survey §3.9).
// Module: MOD-K5-TAKE | REQs: REQ-K5-001..003 | ADR-025
#pragma once

#include <cstdint>

#include "quiver/core.h"
#include "src/kernels/common/kernel_common.h"

QUIVER_BEGIN_NAMESPACE
namespace detail::scalar_impl {

// A value source and its bound. A gather or decode is meaningless without knowing how far the
// source extends, so the two travel together.
template <class T>
struct Source {
  const T* values;
  std::int64_t len;
};

// A run of positions: gather indices, or a selection over codes.
struct IndexRun {
  const std::uint32_t* idx;
  std::int64_t len;
};

template <class T>
void take(Source<T> src, IndexRun indices, T* out) noexcept {
  const T* values = src.values;
  const std::int64_t values_len = src.len;
  const std::uint32_t* idx = indices.idx;
  const std::int64_t idx_len = indices.len;
#if defined(QUIVER_ENABLE_ASSERTS)
  for (std::int64_t j = 0; j < idx_len; ++j) {
    QUIVER_ASSERT(idx[j] < static_cast<std::uint64_t>(values_len),
                  "take: index out of bounds [REQ-K5-002]");
  }
#else
  (void)values_len;
#endif
  // 4x-unrolled independent loads: each iteration's loads carry no dependence on the last,
  // letting the OoO core overlap misses (MLP, Survey §3.9).
  std::int64_t j = 0;
  for (; j + 4 <= idx_len; j += 4) {
    const T v0 = values[idx[j + 0]];
    const T v1 = values[idx[j + 1]];
    const T v2 = values[idx[j + 2]];
    const T v3 = values[idx[j + 3]];
    out[j + 0] = v0;
    out[j + 1] = v1;
    out[j + 2] = v2;
    out[j + 3] = v3;
  }
  for (; j < idx_len; ++j) {
    out[j] = values[idx[j]];
  }
}

template <class T, class C>
void dict_decode(Source<T> dictionary, const C* codes, std::int64_t n, T* out) noexcept {
  const T* dict = dictionary.values;
  const std::int64_t dict_len = dictionary.len;
#if defined(QUIVER_ENABLE_ASSERTS)
  for (std::int64_t i = 0; i < n; ++i) {
    QUIVER_ASSERT(static_cast<std::uint64_t>(codes[i]) < static_cast<std::uint64_t>(dict_len),
                  "dict_decode: code out of bounds [REQ-K5-002]");
  }
#else
  (void)dict_len;
#endif
  std::int64_t i = 0;
  for (; i + 4 <= n; i += 4) {
    const T v0 = dict[codes[i + 0]];
    const T v1 = dict[codes[i + 1]];
    const T v2 = dict[codes[i + 2]];
    const T v3 = dict[codes[i + 3]];
    out[i + 0] = v0;
    out[i + 1] = v1;
    out[i + 2] = v2;
    out[i + 3] = v3;
  }
  for (; i < n; ++i) {
    out[i] = dict[codes[i]];
  }
}

// Fused decode: out[j] = dict[codes[sel[j]]], packed; unselected code positions are never
// read (REQ-K5-003 — validated by the guard-page decode test).
template <class T, class C>
void dict_decode_sel(Source<T> dictionary, const C* codes, IndexRun selection, T* out) noexcept {
  const T* dict = dictionary.values;
  const std::int64_t dict_len = dictionary.len;
  const std::uint32_t* sel = selection.idx;
  const std::int64_t sel_len = selection.len;
  for (std::int64_t j = 0; j < sel_len; ++j) {
    const C code = codes[sel[j]];
    QUIVER_ASSERT(static_cast<std::uint64_t>(code) < static_cast<std::uint64_t>(dict_len),
                  "dict_decode: code out of bounds [REQ-K5-002]");
    (void)dict_len;
    out[j] = dict[code];
  }
}

}  // namespace detail::scalar_impl
QUIVER_END_NAMESPACE
