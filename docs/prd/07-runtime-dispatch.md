# 07 — Runtime Dispatch

## 1. Purpose

Full specification of MOD-DISPATCH and MOD-CPU behavior: how a public kernel call reaches the best backend, how policy (CPU features ∩ override ∩ environment) is computed, and the complete concurrency story. Upstream authority: Charter §6.3 (ISA matrix, runtime dispatch with conservative baseline), Survey §2.3 (ClickHouse cpuid-dispatch pattern), §9 #11.

## 2. Requirements

| ID | Requirement |
|---|---|
| REQ-DISP-001 | Every public kernel API shall route through a dispatch entry selecting among compiled backends `{scalar, neon | avx2, avx512}`; the scalar backend always exists and defines semantics (Charter T3). |
| REQ-DISP-002 | Backend selection order shall be: highest `Isa` value supported by CPU **and** ≤ the active policy cap. `Isa` enum order (`kScalar < kNeon < kAvx2 < kAvx512`) is the preference order; x86 never reports `kNeon`, ARM never reports `kAvx*`. |
| REQ-DISP-003 | Steady-state per-call overhead shall be ≤ 3 atomic loads (one `acquire`, two `relaxed`) + 1 predictable branch + 1 indirect call, measured by `bench_dispatch` (target: < 2 ns amortized on reference machines; informational, not a gate). |
| REQ-DISP-004 | Feature reporting shall be monotone: `cpu_supports(kAvx512) ⇒ cpu_supports(kAvx2)`; AVX-512 requires F+BW+DQ+VL and OS ZMM/opmask state via XGETBV; AVX2 requires OSXSAVE+YMM state ([05 §4](05-internal-architecture.md)). |
| REQ-DISP-005 | The `QUIVER_ISA` environment variable (values: `scalar`, `neon`, `avx2`, `avx512`, case-sensitive) shall be read exactly once, at first policy computation, and acts as an initial override cap. Unrecognized or unsupported values shall be ignored (policy = hardware default); this behavior shall be documented and tested. Rationale: a library shall not print or abort on environment noise. |
| REQ-DISP-006 | `set_isa_override(isa)` shall return `false` and change nothing if `!cpu_supports(isa)` (except `kScalar`, always accepted); on success it caps policy and bumps the policy epoch. `clear_isa_override()` restores the hardware+env policy and bumps the epoch. |
| REQ-DISP-007 | Dispatch state shall consist only of `constinit` statics: the entry table, one `std::atomic<std::uint32_t>` policy epoch, one `std::atomic<std::int8_t>` override slot, one `std::atomic<std::uint8_t>` cached feature mask whose bit 7 is the detection-complete flag (so an all-false-features CPU is representable distinctly from "not yet detected"). No dynamic initialization (REQ-CORE-004), no allocation, no locks. |
| REQ-DISP-008 | Resolution shall be idempotent and race-benign: concurrent first calls may each resolve; all resolvers compute the same function pointer for the same epoch; entries store `{fn, fn_epoch}`; publication order is `fn.store(relaxed)` **then** `fn_epoch.store(release)`; readers `fn_epoch.load(acquire)`, and only on epoch match `fn.load(relaxed)` — the release/acquire pair on `fn_epoch` makes the earlier `fn` store visible (protocol normative in §6). |
| REQ-DISP-009 | Calls concurrent with an epoch bump may execute under the old or new policy; both shall be correct executions (semantic equivalence of backends, REQ-API-006, makes this sound). Overrides are diagnostics/benchmarking tools, documented as not for steady-state concurrent mutation. |
| REQ-DISP-010 | `warmup()` shall resolve every entry under the current epoch before returning; after `warmup()`, no first-call resolution cost remains (until the next epoch bump). |
| REQ-DISP-011 | Within the AVX-512 backend, optional sub-features (VBMI2, VPOPCNTDQ) shall be handled at **resolution time** by selecting among AVX-512-internal variants — never by per-call branching (Survey §2.3 multi-version pattern). |
| REQ-DISP-012 | On ARM64, dispatch shall statically collapse: `kNeon` is the baseline, tables resolve directly to NEON backends (scalar remains selectable via override for baseline benchmarking); no x86 backends are compiled. |
| REQ-DISP-013 | **Compile-time pinning (Charter §6.3).** The build option `QUIVER_PIN_ISA` (REQ-BUILD-006; values `scalar`, `neon`, `avx2`, `avx512`; default unset) shall produce a build in which every dispatch entry statically resolves to the pinned tier's backend: backends above the pin are not compiled, backends below remain compiled only as the pinned tier's internal fallbacks are needed (scalar always compiles, REQ-DISP-001); `active_isa()` returns the pin; `set_isa_override` accepts only `kScalar` and the pin; the epoch machinery compiles to constants. Running a pinned build on unsupporting hardware is the embedder's contract violation — detected by a first-call debug assertion (`cpu_supports(pin)`), UB in release ([16](16-error-handling.md)). Verified by the M8 pinned consumption test (REQ-CI-008). |

## 3. Public interfaces

Surface C contracts are specified in [04 §4](04-public-api.md) (API-DISP-001..005). Internal interface (`dispatch_internal.h`): the entry type, the registration macro each kernel family TU uses to contribute its backend table row, and the resolver.

## 4. Internal structure

```text
Title: Dispatch data flow
Purpose: REQ-DISP-001/007/008 visualization

  public façade (header)                    dispatch_tables.cpp (constinit)
  ┌─────────────────────────┐              ┌──────────────────────────────────┐
  │ compare_bitmap<int32_t> │──index────►  │ Entry { atomic<Fn> fn;           │
  │  (detail switch)        │              │         atomic<u32> fn_epoch; }  │
  └─────────────────────────┘              │ Backends[entry][4] (static const)│
        fast path: epoch match → call fn   └──────────────────────────────────┘
        slow path: resolve(entry):
          features = cached_or_detect()          (MOD-CPU, once)
          cap      = min(env_cap, override_cap)
          fn       = highest non-null Backends[entry][isa ≤ cap ∩ features]
          entry.store({fn, current_epoch}, release)
```

One `Entry` exists per (API × element type × output representation) concrete symbol — the exact entry inventory is emitted from the same X-macro list that generates `detail/extern_decls.h`, guaranteeing table/declaration consistency by construction.

## 5. Lifecycle and state model

States of an `Entry`: `Unresolved(epoch=0)` → `Resolved(epoch=E)` → (epoch bump) → effectively `Stale` → re-`Resolved(epoch=E')`. Global states: `FeatureMask ∈ {Undetected, Detected(mask)}` (monotone, set once); `Override ∈ {None, Cap(isa)}`; `Epoch ∈ u32` (monotonically increasing; wraparound infeasible: bump only via API-DISP-003 calls). Valid transitions are exactly those listed; anything else is a bug (invariant tests).

Initialization: none required — `constinit` tables; the first kernel call performs detection + resolution lazily. Shutdown: none (no resources). Configuration: env var (once) + override API.

## 6. Threading contract (the whole story)

- Feature detection: racy duplicate detection is benign (pure function, same result); the winner's `compare_exchange` publishes; losers discard.
- Entry resolution: multiple threads may resolve the same entry concurrently; all compute identical `(fn, fn_epoch)` for a given epoch observation; last store wins; every stored value is valid. **Normative protocol:** hot path = ① `policy_epoch.load(relaxed)` → ② `entry.fn_epoch.load(acquire)`; if ② == ①, ③ `entry.fn.load(relaxed)` and call — the acquire on ② synchronizes with the resolver's `fn_epoch.store(release)`, which was preceded by `fn.store(relaxed)`, so ③ can never observe a stale or null pointer after an epoch match (this holds on ARM's weak model, not just x86-TSO). On mismatch, the slow path re-resolves and republishes in the same order. A stale `policy_epoch` read at ① merely executes under the previous policy (REQ-DISP-009). This exact protocol shall be TSan-validated (`test_dispatch` concurrent section) and documented with this argument in `docs/internals/dispatch-state-machine.md`.
- Epoch bump during flight: REQ-DISP-009.
- No locks anywhere; no thread creation anywhere in the library (Charter T1/§7.3).

## 7. Failure modes

| Condition | Behavior |
|---|---|
| Unknown/exotic CPU | all-false features → scalar backend (never a crash; Charter conservative-baseline) |
| Invalid `QUIVER_ISA` value | ignored (REQ-DISP-005), covered by test |
| `set_isa_override` unsupported tier | returns `false`, no change |
| Backend missing for a tier (e.g., `QUIVER_DISABLE_AVX512` build) | resolution skips null rows — the table's null-skipping is the single mechanism for partial builds |
| Impossible: entry resolves to null | structurally impossible (scalar row is never null — static_assert on table construction) |

## 8. Performance contract

Hot path: REQ-DISP-003. Cold path: first call per entry ≈ detection (once) + table scan (≤ 4 rows). Code footprint: one 16-byte `Entry` per concrete symbol (~10 types × ~30 symbols ≈ 300 entries ≈ 5 KiB — dcache-irrelevant). The dispatch-overhead question — "does the epoch check cost anything measurable vs a plain global function pointer?" — is a standing benchmark hypothesis (`bench_dispatch`, master prompt Part 8 philosophy).

## 9. ADRs

**ADR-004 — Lazy atomic per-entry dispatch with policy epoch.** *Status:* Accepted. *Context:* Charter §6.3 requires runtime dispatch + benchmarking requires forcing specific backends ([10](10-benchmark-architecture.md)); Charter forbids allocation/locks in the library. *Problem:* dispatch that is near-zero-overhead, override-capable mid-process, and initialization-order-safe. *Alternatives:* (1) GCC/Clang `ifunc` — rejected: no override capability, ELF-only, complicates macOS/MSVC and amalgamation; (2) eager init via static constructor — rejected: dynamic initialization prohibited (REQ-CORE-004), order fiasco risk in static-lib consumers; (3) per-call `if (feature)` branching — rejected: per-call overhead scales with tiers, pollutes branch predictor; (4) resolve-once atomic pointer *without* epoch — rejected: overrides could not retract already-resolved entries, making forced-variant benchmarking unsound; (5) **lazy atomic entries + epoch** (selected). *Tradeoffs:* one extra epoch load per call (measured, `bench_dispatch`) buys sound overrides and testability. *Consequences:* the epoch protocol must be TSan-proven; documented as above. *Reconsideration:* if `bench_dispatch` shows the epoch check costing > 1% on any 4K-element kernel invocation on a reference machine, evaluate collapsing to alternative 4 with process-start-only overrides (PRD amendment). *Related:* REQ-DISP-003/006/007/008/009, API-DISP-003.

**ADR-005 — First-party CPU feature detection.** *Status:* Accepted. *Context:* zero-dependency pledge (Charter T4); need OS-state-correct AVX detection. *Alternatives:* (1) libcpuid/other library — rejected: dependency; (2) compiler builtins `__builtin_cpu_supports` — rejected: no VBMI2/OS-state granularity guarantees across toolchains, MSVC absent; (3) **first-party CPUID/XGETBV + getauxval + sysctl** (selected; ~150 lines, SDM/ARM-ARM-cited). *Consequences:* platform table maintained in `docs/internals/cpu-detection.md`. *Reconsideration:* new OS/arch targets. *Related:* REQ-DISP-004, REQ-INT-001.

## 10. Acceptance criteria

All REQ-DISP tests pass on x86-64 (incl. under SDE where AVX-512 CPUID is exposed) and ARM64; TSan concurrent-resolution test clean; `bench_dispatch` runs and publishes overhead numbers; env-var matrix test (unset/valid/invalid/unsupported) passes; override round-trip (set → observe backend via `active_isa` + differential timing → clear) validated.

## 11. Test and benchmark matrix (module-level)

| Artifact | Covers |
|---|---|
| `tests/unit/test_dispatch.cpp` | REQ-DISP-002/004/005/006/010/012, API-DISP-001..005 |
| TSan section of the same | REQ-DISP-008/009 |
| `tests/invariant/inv_determinism.cpp` (per-ISA sections) | REQ-DISP-001 semantic-equivalence prerequisite |
| `bench/micro/bench_dispatch.cpp` | REQ-DISP-003, ADR-004 reconsideration trigger |

## 12. Documentation requirements

`docs/architecture/dispatch.md` (user-facing: override semantics, env var, warmup guidance); `docs/internals/dispatch-state-machine.md` (the §5/§6 protocol with the memory-order argument, for reviewers).

## 13. Traceability

Charter §6.3, §7.1 Surface C, T3/T4 → REQ-DISP-001..012 → ADR-004/005 → MOD-DISPATCH/MOD-CPU ([05 §4–5](05-internal-architecture.md)) → tests/bench (§11) → milestone M1 (framework), M4+ (per-ISA rows), M8 (hardening). Survey authority: §2.3 (runtime dispatch pattern, multi-version kernels), §9 #11 (conservative baseline pattern).
