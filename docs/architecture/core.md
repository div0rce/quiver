# MOD-CORE — vocabulary types and configuration

**Purpose.** The shared vocabulary (Charter Surface B) and build-configuration macros every other module uses; contracts have one home. Spec: PRD [05 §3](../prd/05-internal-architecture.md).

**Responsibilities.** `quiver/core.h`: the ten-type `Element` concept family, `CompareOp`/`MaskOp`/`ArithOp`/`Isa` enums (frozen values — dispatch indexes by `Isa`), the non-owning views `BatchView<T>`/`BitmapView`/`SelVec`/`MinMaxSummary<T>`, `SumType<T>`, `kMaxBatchLen`. `quiver/detail/config.h`: version macros (single source of truth — CMake parses them), namespace macros (ABI epoch `v1`, ADR-007), `QUIVER_ASSERT` (ADR-017), `QUIVER_FORCE_INLINE`/`QUIVER_RESTRICT`/`QUIVER_ASSUME`.

**Non-responsibilities.** No behavior, no kernels, no detection, no dispatch, no allocation helpers.

**Interfaces.** Public-stable: `quiver/core.h` (see [API reference](../api/core.md)). Public-but-unstable: `quiver/detail/config.h`.

**Dependencies / dependents.** Depends on the C++ standard library only; every other module depends on it.

**Lifecycle & state.** None — all-`constexpr` declarations; the views are trivially copyable aggregates with no constructors and no enforced invariants (REQ-CORE-002: views are dumb; contracts live at kernel boundaries).

**Memory model.** All views borrow caller memory (Charter T6); `sizeof(BatchView<T>) == 16` on LP64 (tested).

**Threading model.** Stateless; nothing to synchronize.

**Failure modes.** None at runtime; misuse is a compile-time concept diagnostic. `QUIVER_ASSERT` failure behavior: one stderr line `file:line: assertion: msg`, then `abort()` (REQ-ERR-002/-005).

**Performance notes.** Zero runtime cost; headers are intentionally light.

**Validation.** `tests/unit/test_core.cpp` — concept accept/reject, triviality/layout static asserts, `SumType` mapping, assert-macro death test + no-op test.

**Related requirements.** REQ-CORE-001..004, REQ-API-001/-004/-005, REQ-ERR-002/-005. **Related ADRs:** [ADR-006](../adr/ADR-006-public-api-style.md), [ADR-007](../adr/ADR-007-inline-namespace-abi-versioning.md), [ADR-017](../adr/ADR-017-contract-based-error-model.md).
