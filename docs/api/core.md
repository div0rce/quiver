# API reference — `quiver/core.h` (Surface B)

The vocabulary types (API-CORE-001). Contracts mirror PRD [04 §3](../prd/04-public-api.md); this page is the consumer-facing form. Introduced: v0.1. Stability: 0.x-fluid until the v1.0 freeze (REQ-API-009).

## Constants and concepts

| Symbol | Definition |
|---|---|
| `kMaxBatchLen` | `2'147'483'647` — every batch/selection length is `0 ≤ n ≤ kMaxBatchLen` (REQ-API-005) |
| `Element<T>` | exactly: `int8/16/32/64_t`, `uint8/16/32/64_t`, `float`, `double` (REQ-API-004) |
| `IntElement<T>` | `Element<T>` ∧ integral |
| `CodeType<T>` | exactly: `uint8_t`, `uint16_t`, `uint32_t` (dictionary codes) |
| `UnpackOut<T>` | exactly: `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t` (K8 unpack output width) |
| `SumType<T>` | `int64_t` for signed integers, `uint64_t` for unsigned, `T` for floats |

## Enums (values frozen)

`CompareOp{kEq,kNe,kLt,kLe,kGt,kGe}` · `MaskOp{kAnd,kOr,kAndNot,kXor}` · `ArithOp{kAdd,kSub,kMul}` · `Isa{kScalar=0,kNeon=1,kAvx2=2,kAvx512=3}` (the `Isa` order is the dispatch preference order, REQ-DISP-002).

## Views (non-owning, trivially copyable, pass by value)

All views **borrow** caller memory: no ownership transfer, no retention beyond any call (REQ-API-003, REQ-MEM-009). They are aggregates with no constructors and no enforced invariants — contracts live at kernel boundaries (REQ-CORE-002).

| Type | Fields | Contract summary |
|---|---|---|
| `BatchView<T>` | `const T* data; int64_t len` | `data != nullptr` when `len > 0`; 16 bytes on LP64 |
| `BitmapView` | `const uint8_t* bits` | LSB-first, 1 = valid/selected, `⌈n/8⌉` bytes for the accompanying `n` (REQ-MEM-006, Arrow-compatible); `bits == nullptr` = "all valid" **only** for parameters named `validity` (REQ-API-008) |
| `SelVec` | `const uint32_t* idx; int64_t len` | selection semantics: strictly increasing, in-range ([ADR-025](../adr/ADR-025-selection-vector-semantics.md)); `take` relaxes to arbitrary in-range indices |
| `Sma<T>` | `T min; T max; int64_t null_count` | identity values (`min = max()`, `max = lowest()`) when nothing participates |

**Allocation/exceptions/threading:** these are plain data — no allocation, nothing throws, freely shareable across threads (immutably).

**Validation:** `tests/unit/test_core.cpp` (concept accept/reject, layout, `SumType` mapping). **Benchmarks:** none — no behavior (PRD 05 §3).
