# API reference — `quiver/dispatch.h` (Surface C)

ISA query/override and version introspection (API-DISP-001..005). Contracts mirror PRD [04 §4](../prd/04-public-api.md); lifecycle semantics in the [dispatch architecture page](../architecture/dispatch.md). Introduced: v0.1. All functions are `noexcept`, allocation-free, and thread-safe.

| API | Signature | Contract |
|---|---|---|
| API-DISP-001 | `Isa active_isa() noexcept` | The tier dispatch would select now: highest CPU-supported, capped by `QUIVER_ISA` and any active override. |
| API-DISP-002 | `bool cpu_supports(Isa) noexcept` | CPU **and** OS can execute the tier (XGETBV state checks on x86). `kScalar` is always true; ARM never reports `kAvx*`, x86 never reports `kNeon`. Out-of-range enum values return `false`. |
| API-DISP-003 | `bool set_isa_override(Isa) noexcept` / `void clear_isa_override() noexcept` | Caps dispatch at the tier; `false` + no state change if unsupported (`kScalar` always accepted). Takes effect via the policy epoch; calls concurrent with a change may run under either policy — both correct (REQ-DISP-009). For benchmarking/diagnostics. |
| API-DISP-004 | `void warmup() noexcept` | Eagerly resolves every dispatch entry under the current policy (and forces feature detection + the once-only `QUIVER_ISA` read). Idempotent. |
| API-DISP-005 | `Version version() noexcept` / `const char* version_string() noexcept` | Compile-embedded version; the string has static storage duration. Matches the CMake project version by construction (both derive from `config.h`). |

## Example (from the test suite)

```cpp
--8<-- "tests/unit/test_dispatch.cpp:override"
```

## Environment variable

`QUIVER_ISA ∈ {scalar, neon, avx2, avx512}` (case-sensitive), read exactly once at first policy computation; unrecognized or unsupported values are ignored (REQ-DISP-005).

**Validation:** `tests/unit/test_dispatch.cpp` (env matrix via per-process probes, override round-trip, monotonicity, TSan concurrency). **Benchmarks:** `bench_dispatch` measures the dispatch-overhead contract (REQ-DISP-003) from M3.
