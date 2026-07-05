# MOD-DISPATCH — runtime dispatch, override, version

**Purpose.** Route each public kernel call to the best available backend with negligible overhead (Charter §6.3). Spec: PRD [07](../prd/07-runtime-dispatch.md); protocol details: [dispatch state machine](../internals/dispatch-state-machine.md).

**Responsibilities.** Per-entry atomic function-pointer resolution with a policy epoch; policy computation (CPU features ∩ `QUIVER_ISA` environment cap ∩ programmatic override); Surface C (`active_isa`, `cpu_supports`, `set_isa_override`/`clear_isa_override`, `warmup`, `version`).

**Non-responsibilities.** No kernel logic; no feature detection (delegates to MOD-CPU); no per-call data-dependent heuristics (backend choice is resolution-time only, REQ-KERNEL-007).

**Interfaces.** Public-stable: `quiver/dispatch.h` ([API reference](../api/dispatch.md)). Internal: `src/dispatch/dispatch_internal.h` (entry/row types, `dispatch_get`/`resolve`).

**Using the override (benchmarking/diagnostics):**

```cpp
--8<-- "tests/unit/test_dispatch.cpp:override"
```

**`QUIVER_ISA` environment variable.** Read exactly once, at first policy computation; values `scalar`, `neon`, `avx2`, `avx512` (case-sensitive) act as an initial cap; unrecognized or CPU-unsupported values are ignored (REQ-DISP-005).

**Lifecycle & state.** No initialization required — all state is `constinit` (REQ-CORE-004): the entry table, one policy epoch (`atomic<uint32_t>`, starts at 1), one override slot (`atomic<int8_t>`), one feature mask (`atomic<uint8_t>`, bit 7 = detection-complete), and one env-cap slot (`atomic<int8_t>`; see the M1 gate record's deviation note). First call detects features and reads the environment; `warmup()` forces this eagerly.

**Threading model.** All Surface C functions are thread-safe; resolution races are benign by design (duplicate resolution is idempotent). Calls concurrent with an override change may execute under either policy — both are correct (REQ-DISP-009). Overrides are not meant for steady-state concurrent mutation.

**Failure modes.** Unknown CPUs resolve to scalar (never a crash); invalid `QUIVER_ISA` values are ignored; `set_isa_override` on an unsupported tier returns `false` with no state change; a null-scalar backend row is structurally excluded (asserted).

**Compile-time pinning (`QUIVER_PIN_ISA`, REQ-DISP-013).** Setting the build option `QUIVER_PIN_ISA` to `scalar`, `neon`, `avx2`, or `avx512` produces a build in which every entry statically resolves to the pinned tier: backend TUs above the pin are not compiled (scalar always compiles as the base fallback, REQ-DISP-001; AVX2 stays as AVX-512's fallback), and the dispatch rows null the excluded slots to match. In a pinned build `active_isa()` returns the pin, `QUIVER_ISA` is ignored, and `set_isa_override` accepts only `kScalar` (to force the base fallback) and the pin itself — higher tiers are absent so an override to them could never resolve. A pinned build run on hardware lacking the pinned tier is the embedder's contract violation: caught by a first-touch debug assertion (`cpu_supports(pin)`), undefined behavior in release. Use it to shrink a build for a known deployment target or to A/B a single tier; leave it unset for portable binaries. Verified by the pinned-build CI leg (`QUIVER_PIN_ISA=avx2`, REQ-CI-008).

**Performance notes.** Steady-state overhead: ≤ 3 atomic loads (one acquire, two relaxed) + 1 predictable branch + 1 indirect call (REQ-DISP-003); measured by `bench_dispatch` from M3. A pinned build removes the environment/override policy width, so the cap folds to a constant (asserts off).

**Validation.** `tests/unit/test_dispatch.cpp`: env matrix (one child process per value), override round-trip, monotonicity, synthetic-entry resolution incl. null-row skipping and epoch retraction, warmup idempotence, and the TSan-validated concurrent-resolution test.

**Related requirements.** REQ-DISP-001..013, REQ-INT-001. **Related ADRs:** [ADR-004](../adr/ADR-004-lazy-atomic-per-entry-dispatch-with-policy-epoch.md), [ADR-005](../adr/ADR-005-first-party-cpu-feature-detection.md).
