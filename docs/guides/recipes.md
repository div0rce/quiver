# Recipes

Task-oriented, complete, runnable. Every listing below is extracted verbatim from a compiled,
CI-tested program in [`examples/`](https://github.com/div0rce/quiver/tree/main/examples)
(REQ-DOC-006: documentation never free-types C++), so if a recipe and the repository disagree, the
repository has already won. Build them all with `cmake --preset bench && cmake --build --preset
bench` or compile any one against the two-file drop-in.

## Filter a column

Compare into a selection bitmap, then compact the surviving values. The two calls at the heart of
every WHERE clause:

```cpp
--8<-- "examples/01_minimal_filter.cpp:filter"
```

## Compare, gather, reduce: a small pipeline

The same predicate as a selection vector (row indices instead of bits), a gather through it, and a
null-aware sum. This is the compare → take → reduce composition analytical code repeats endlessly:

```cpp
--8<-- "examples/02_filter_take_reduce.cpp:pipeline"
```

## Inspect and force the ISA tier

Which backend is running, how to pin one at run time, and the guarantee that makes pinning safe to
test with: results are identical across ISA tiers, bit for bit:

```cpp
--8<-- "examples/03_isa_override.cpp:override"
```

## Work with nulls (validity bitmaps)

Validity bitmaps are LSB-first, one bit per row, 1 = valid. They combine with mask algebra and
flow through compare and reduce so null lanes never contribute:

```cpp
--8<-- "examples/04_nullable_pipeline.cpp:nullable"
```

## Where next

- The full per-operation contracts: [API reference](../api/core.md).
- Consuming Quiver in your build (vcpkg/Conan skeletons, `FetchContent`, drop-in):
  [vendoring](vendoring.md).
- What the operations cost, with committed evidence: [Performance](../benchmarks/README.md).
