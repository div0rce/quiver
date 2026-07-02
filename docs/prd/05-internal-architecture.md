# 05 — Internal Architecture: Non-Kernel Module Specifications

## 1. Purpose

Complete engineering contracts for every non-kernel module. Kernel families (MOD-K1…K10) are specified in [08-kernel-design.md](08-kernel-design.md) against the common contract defined there; MOD-CI is specified in [13-ci-architecture.md](13-ci-architecture.md). Each module below is specified against the full template (purpose, responsibilities, non-responsibilities, dependencies, public interfaces, internal structure, lifecycle, state model, invariants, failure modes, performance contract, threading contract, memory contract, test matrix, benchmark matrix, documentation requirements, acceptance criteria). Requirement IDs introduced here: REQ-CORE-*, REQ-INT-*.

Shared defaults (apply unless a module states a delta): *Lifecycle* — no initialization, configuration, or shutdown; stateless code. *Threading* — thread-compatible, no synchronization. *Memory* — no allocation; operates on caller memory only. *Failure modes* — contract violations assert in debug, UB in release ([16](16-error-handling.md)).

---

## 2. Requirements introduced in this chapter

| ID | Requirement |
|---|---|
| REQ-CORE-001 | `core.h` + `detail/config.h` shall define exactly the Surface B inventory of [04 §3](04-public-api.md) plus the macro set of §3 below; nothing else. |
| REQ-CORE-002 | All vocabulary structs shall be trivially copyable, standard-layout, and free of constructors, methods, and invariants enforced at construction (views are dumb; contracts live at kernel boundaries). |
| REQ-CORE-003 | `QUIVER_ASSERT(cond, msg)` shall compile to nothing unless `QUIVER_ENABLE_ASSERTS` is defined; when enabled, failure writes one line to stderr (`file:line: assertion: msg`) and calls `std::abort()`. |
| REQ-CORE-004 | No global object with dynamic initialization shall exist anywhere in the shipped library; all globals shall be `constinit` (enforced by clang-tidy + review; [17](17-coding-standards.md)). |
| REQ-INT-001 | MOD-CPU shall expose exactly one internal function `CpuFeatures detect_cpu_features() noexcept` returning a value-type feature record; it shall be callable concurrently and shall not cache (caching is MOD-DISPATCH's job). |
| REQ-INT-002 | MOD-TESTKIT generators shall be fully deterministic from a `uint64_t` seed (SplitMix64-based), portable across platforms, and shall print the seed in every failure message. |
| REQ-INT-003 | MOD-BENCH shall validate each benchmark's kernel output against the scalar reference once per process before timing loops; a mismatch aborts the benchmark binary with a diagnostic (Survey §7.5 “incorrect code” pitfall). |
| REQ-INT-004 | MOD-LEDGER shall be implemented in Python ≥ 3.11 using the standard library only (Charter T4 applies to the shipped library; the ledger runner is a dev tool but inherits the zero-third-party discipline to stay runnable anywhere). |
| REQ-INT-005 | MOD-AMALG output shall be byte-identical across repeated runs on an identical tree (ADR-018 determinism clause). |
| REQ-INT-006 | MOD-EXAMPLES shall compile with exceptions and RTTI enabled and `-Wall -Wextra -Werror`, proving consumability under ordinary consumer settings (complements REQ-BUILD-004). |

---

## 3. MOD-CORE — vocabulary types and configuration

- **Purpose:** the shared vocabulary (Surface B) and build-configuration macros every other module uses; exists so contracts have one home (Charter §7.1).
- **Responsibilities:** define types/enums/concepts of [04 §3](04-public-api.md); define `QUIVER_ASSERT`, `QUIVER_ASSUME`, `QUIVER_RESTRICT`, `QUIVER_FORCE_INLINE`, inline-namespace macros, version macros (`QUIVER_VERSION_MAJOR/MINOR/PATCH`).
- **Non-responsibilities:** no behavior, no kernels, no detection, no dispatch, no allocation helpers.
- **Dependencies:** C++ standard library headers only. **Dependents:** every module.
- **Public interfaces:** `quiver/core.h` (stable), `quiver/detail/config.h` (public-but-unstable).
- **Internal structure:** two headers; no TUs.
- **Lifecycle / state model:** none; all-`constexpr`/`constinit`, stateless (shared defaults).
- **Invariants:** REQ-CORE-002 triviality; `sizeof(BatchView<T>) == 16` on LP64 (documented, tested); enum values are frozen numbers (dispatch tables index by `Isa`).
- **Failure modes:** none at runtime; misuse is compile-time (concept diagnostics).
- **Performance contract:** zero runtime cost; headers add < 5 ms to consumer compile (measured once, informational).
- **Threading contract / memory contract:** shared defaults; no state at all.
- **Test matrix:** `tests/unit/test_core.cpp` — static asserts (triviality, layout, concept accept/reject lists), assert-macro behavior test (death test when enabled, no-op when disabled) → REQ-CORE-001..003.
- **Benchmark matrix:** none (no behavior).
- **Documentation:** `docs/architecture/core.md`; types reference in `docs/api/core.md`.
- **Acceptance:** tests pass; headers self-contained (`-Wpedantic` include-what-you-use clean); REQ-CORE-001..004 verified.

## 4. MOD-CPU — CPU feature detection

- **Purpose:** answer "what can this CPU+OS execute?" once, correctly, per platform (Charter §6.3; Survey §2.3 runtime-dispatch pattern).
- **Responsibilities:** x86-64: CPUID leaf walk + `XGETBV` OS-state validation (AVX2 requires OSXSAVE+YMM state; AVX-512 requires opmask/ZMM/Hi16 state and the F+BW+DQ+VL feature set; VBMI2 and VPOPCNTDQ detected as optional sub-features); ARM64 Linux: `getauxval(AT_HWCAP)`; ARM64 macOS: `sysctlbyname`. Produce a `CpuFeatures` value: `{ bool avx2, avx512, avx512vbmi2, neon; char brand[64]; }`.
- **Non-responsibilities:** no caching, no policy (which ISA to *use* is MOD-DISPATCH), no environment-variable reading, no microarchitecture naming (the ledger's machine registry owns µarch identity).
- **Dependencies:** OS/intrinsic headers (`<cpuid.h>`/`__cpuidex`, `<sys/auxv.h>`, `<sys/sysctl.h>`). **Dependents:** MOD-DISPATCH only.
- **Public interfaces:** internal `src/cpu/cpu_features.h`: `detect_cpu_features()` (REQ-INT-001). Not exported.
- **Internal structure:** one header + one TU with three platform sections selected by predefined macros.
- **Lifecycle / state model:** stateless pure function (shared defaults).
- **Invariants:** never reports a tier the OS cannot save state for; on unknown/exotic platforms returns all-false (scalar-only) rather than guessing.
- **Failure modes:** none recoverable/unrecoverable at runtime — detection is total; a compile on an unsupported platform selects the all-false fallback path with a `#warning`.
- **Performance contract:** cold path only; ≤ a few microseconds; called once per process by dispatch (twice only under override changes).
- **Threading contract:** safe concurrent calls (pure). **Memory contract:** shared defaults.
- **Test matrix:** `test_dispatch.cpp` sections: detection sanity vs `/proc/cpuinfo`-independent invariants (e.g., avx512 ⇒ avx2 monotonicity — REQ-DISP-004), all-false path compile test via macro-forced fallback → REQ-INT-001.
- **Benchmark matrix:** none (cold path).
- **Documentation:** `docs/internals/cpu-detection.md` (leaf/bit tables with Intel SDM / ARM ARM references).
- **Acceptance:** correct tier reporting on all CI runner classes incl. under SDE (which exposes AVX-512 CPUID bits); monotonicity invariant test passes.

## 5. MOD-DISPATCH — dispatch tables, override, version

Specified in full in [07-runtime-dispatch.md](07-runtime-dispatch.md); module-template summary here for the inventory’s completeness.

- **Purpose:** route each public kernel call to the best available backend with negligible overhead (Charter §6.3).
- **Responsibilities:** per-entry atomic function-pointer tables; policy computation (features ∩ override ∩ env); policy epoch; `warmup()`; version introspection.
- **Non-responsibilities:** no kernel logic; no feature detection (delegates to MOD-CPU); no per-call heuristics (data-dependent backend choice is prohibited in v1 — evidence-gated *static* choice only, REQ-KERNEL-007).
- **Dependencies:** MOD-CORE, MOD-CPU. **Dependents:** all kernel modules (registration), public Surface C.
- **Public interfaces:** `quiver/dispatch.h` (API-DISP-001..005); internal `dispatch_internal.h` (entry/table types, registration macros).
- **Internal structure / lifecycle / state model / invariants / failure modes:** see [07 §4–§8](07-runtime-dispatch.md).
- **Performance contract:** steady-state overhead ≤ 3 atomic loads (one acquire) + 1 indirect call per public call (REQ-DISP-003); measured by `bench_dispatch.cpp`.
- **Threading contract:** all Surface C functions thread-safe; resolution races benign-by-design ([07 §6](07-runtime-dispatch.md)).
- **Memory contract:** static `constinit` tables only; no allocation ever (REQ-CORE-004).
- **Test matrix:** `test_dispatch.cpp` + TSan concurrent-first-call test + env-var matrix test → REQ-DISP-001..012.
- **Benchmark matrix:** `bench_dispatch.cpp`: dispatched vs direct-symbol call; epoch-check cost; warmup cost.
- **Documentation:** `docs/architecture/dispatch.md` + `docs/internals/dispatch-state-machine.md`.
- **Acceptance:** [07 §10](07-runtime-dispatch.md).

## 6. MOD-KCOMMON — shared kernel utilities

- **Purpose:** the small set of helpers every family needs, so families stay mutually independent (REQ-REPO-009).
- **Responsibilities:** target-region macros (`target_regions.h`, ADR-003); bitmap word helpers (load/store partial byte, tail-zero mask, popcount over words); tail-loop helpers; compaction LUTs (`luts.h/.cpp`: AVX2 32-bit 256×8 `uint32_t` permutation table; nibble tables for 8/16-bit and NEON `TBL` — layouts specified in [09 §6](09-simd-architecture.md)); seed constants shared with nothing (hash constants live in K7).
- **Non-responsibilities:** no public API; no family semantics; no dispatch.
- **Dependencies:** MOD-CORE. **Dependents:** MOD-K1…K10.
- **Public interfaces:** none (internal headers only).
- **Internal structure:** 3 headers + 1 TU (`luts.cpp` holds `constinit` tables generated by `consteval` functions — no code-generation build step, ADR-003 consequence).
- **Lifecycle / state model:** stateless; tables are immutable `constinit` data.
- **Invariants:** LUT contents provably correct by `consteval` construction + a unit test that re-derives them at runtime and compares; tables are `alignas(64)`.
- **Failure modes:** none at runtime.
- **Performance contract:** LUTs total ≤ 16 KiB (icache/dcache footprint documented; [20](20-risk-register.md) R-11); helpers must inline (force-inline macros; verified by inspecting no-call codegen in one smoke check, informational).
- **Threading / memory contract:** immutable shared data; safe concurrent reads; no allocation.
- **Test matrix:** `tests/unit/test_core.cpp` LUT re-derivation section; tail-helper unit tests over all residues 0..63 → REQ-KERNEL-002 support.
- **Benchmark matrix:** none directly (measured through family benchmarks).
- **Documentation:** `docs/internals/kernel-common.md`.
- **Acceptance:** LUT re-derivation test passes; no family includes another family's headers (include lint, REQ-REPO-009).

## 7. MOD-TESTKIT — test infrastructure

- **Purpose:** deterministic input generation and second-opinion oracles so every kernel test is reproducible and self-diagnosing (Survey §7.4/§7.5 discipline as code, Charter T2).
- **Responsibilities:** seeded generators (`generators.h/.cpp`): value distributions (sequential, uniform, Zipf θ=1.0 over 1,000 distinct values, boundary-heavy sets: `{min, min+1, -1, 0, 1, max-1, max}` and float specials `{±0.0, ±inf, NaN quiet/signaling payloads, denormals}`), bitmap patterns (uniform p ∈ {0,1,10,50,90,99,100}%, clustered runs (geometric run lengths, mean 64), alternating), selection vectors (derived from bitmaps), alignment-offset buffer factory (allocates padded arenas and returns pointers at requested element offsets — the *tests* may allocate; the library may not); naive reference implementations (`reference.h`) — a second, independently written oracle beside the scalar `_impl.h` reference (differential defense against a wrong specification implementation); diagnostic assertions (`assertions.h`): first-divergence reporting with index, byte context, seed, and REQ ID.
- **Non-responsibilities:** no production code paths; no benchmark data generation ownership (MOD-BENCH mirrors distributions via shared spec, not shared code — see delta note in [10 §5](10-benchmark-architecture.md)).
- **Dependencies:** library under test, GoogleTest (pinned, REQ-BUILD-007). **Dependents:** all test targets, fuzz harnesses.
- **Public interfaces (dev):** the three headers above; stable within the repo, no external stability.
- **Internal structure:** header-mostly + one TU for table-driven distributions.
- **Lifecycle:** constructed per test; no global state beyond GoogleTest's.
- **State model:** generator objects hold `{seed, stream position}`; copyable; value semantics.
- **Invariants:** same seed ⇒ same bytes on every platform (REQ-INT-002; integer-only generation, float values built from integer bit patterns — no libm, no platform FP divergence).
- **Failure modes:** generator misuse (e.g., requesting selectivity > 100%) asserts immediately.
- **Performance contract:** generation ≤ O(n); no requirement beyond "tests complete in CI budget" ([13 §6](13-ci-architecture.md)).
- **Threading contract:** each generator instance thread-confined; distinct instances concurrently safe.
- **Memory contract:** owns its arenas (RAII); tests never leak (LeakSanitizer nightly).
- **Test matrix:** self-tests: determinism across two processes (golden byte hashes committed), distribution sanity (empirical selectivity within ±0.5% at n=1e6) → REQ-INT-002.
- **Benchmark matrix:** none.
- **Documentation:** `docs/testing/testkit.md`.
- **Acceptance:** self-tests pass on all tier-1 platforms with identical golden hashes.

## 8. MOD-BENCH — benchmark harness

Specified in full in [10-benchmark-architecture.md](10-benchmark-architecture.md); summary against the template:

- **Purpose:** answer engineering questions about kernel performance reproducibly (master prompt Part 8; Charter T2/T7).
- **Responsibilities:** Google Benchmark integration and naming convention; input distributions mirroring the ledger axes; pre-timing output validation (REQ-INT-003) incl. the bench-local ADR-013 float-policy oracle (REQ-BENCH-004); PMU wrapper (`pmu.h/.cpp`, `perf_event_open`, ADR-022); metadata emission (git SHA, compiler, flags) into GB JSON `context`; forced-variant execution via API-DISP-003; equal-ISA auto-vectorized baselines (`bench/baselines/`, ADR-011).
- **Non-responsibilities:** statistics beyond GB's per-run numbers (aggregation/CI/CV is MOD-LEDGER's); environment control (documented procedure, runner-enforced); publishing.
- **Dependencies:** library, Google Benchmark (pinned), Linux perf ABI (optional at runtime). **Dependents:** MOD-LEDGER (subprocess).
- **Public interfaces (dev):** `bench/harness/*` headers; CLI contract of bench binaries (GB flags + `--quiver_variant=<isa|autovec-*|scalar>` custom flag, [10 §6](10-benchmark-architecture.md)).
- **Internal structure / lifecycle / state / invariants / failure modes / contracts:** [10 §4–§8](10-benchmark-architecture.md). Key invariant: a benchmark that fails validation must not emit timing output (REQ-INT-003).
- **Test matrix:** harness self-test target (validation-abort path, PMU-absent fallback path) → REQ-BENCH-004/005.
- **Benchmark matrix:** n/a (it *is* the benchmark layer).
- **Documentation:** `docs/benchmarks/methodology.md`, `docs/benchmarks/running.md`.
- **Acceptance:** [10 §10](10-benchmark-architecture.md).

## 9. MOD-LEDGER — ledger runner and schemas

Specified in full in [11-performance-ledger.md](11-performance-ledger.md); summary:

- **Purpose:** the second product: reproducible, statistically defensible, machine-readable performance records (Charter §1, §6.4).
- **Responsibilities:** orchestrate ≥10 process-level repetitions with seeded random interleaving; capture environment manifests; compute median/min/bootstrap-CI/CV; apply noise flags; validate against Surface D schemas; write `ledger/results/**`; verify pre-run environment checklist; provide the single-command reproduction entry point (`quiver_ledger.py run --machine <id> --filter <pattern>`).
- **Non-responsibilities:** no timing itself (delegates to bench binaries); no chart generation (docs embed numbers by reference, [14 §6](14-documentation.md)); no CI execution (ledger runs on registered machines only, [11 §8](11-performance-ledger.md)).
- **Dependencies:** Python ≥3.11 stdlib (REQ-INT-004); bench binaries via subprocess.
- **Public interfaces:** CLI (`run`, `aggregate`, `validate`, `manifest` subcommands); Surface D JSON schemas (`ledger/schema/*.json`, independently versioned).
- **Internal structure / lifecycle / state / invariants / failure modes:** [11 §5–§7](11-performance-ledger.md). Key invariants: no entry without a manifest; no published entry with CV > 5%; raw per-repetition data retained beside aggregates.
- **Performance contract:** runner overhead irrelevant (between repetitions, never inside timed regions).
- **Threading contract:** single-process orchestrator; repetitions strictly sequential (no parallel benchmarking on the machine under test — documented methodology rule QLM-1, [11 §6](11-performance-ledger.md)).
- **Memory contract:** n/a (Python tool).
- **Test matrix:** pytest-style stdlib `unittest` suite: statistics golden tests (bootstrap with fixed seed vs committed values), schema validation accept/reject fixtures, manifest capture smoke on Linux → REQ-LEDGER-*.
- **Benchmark matrix:** n/a.
- **Documentation:** `docs/benchmarks/ledger.md`, dispute guide `docs/guides/disputes.md`.
- **Acceptance:** [11 §10](11-performance-ledger.md).

## 10. MOD-AMALG — amalgamation generator

- **Purpose:** produce the two-file vendoring artifact (Charter T4; ADR-002/018).
- **Responsibilities:** deterministic text-level amalgamation per ADR-018 rules; MSVC guard injection; version stamping; self-verification hooks for `quiver_amalgamate_verify` (REQ-BUILD-013).
- **Non-responsibilities:** no semantic C++ parsing (rule-based text transformation only — the coding standards keep sources transformable, REQ-STD-006); no release uploading (release workflow's job).
- **Dependencies:** Python ≥3.11 stdlib; reads `include/` + `src/` as text. **Dependents:** release workflow, vendoring guide.
- **Public interfaces (dev):** `amalgamate.py --out-dir <dir>`; exits non-zero on any rule violation (unknown include form, guard mismatch) with file/line diagnostics.
- **Internal structure:** single script + `test_amalgamate.py` (stdlib unittest).
- **Lifecycle:** batch tool; no state between runs.
- **State model:** none. **Invariants:** REQ-INT-005 byte-determinism; output compiles warning-clean at the same warning level as the normal build.
- **Failure modes:** malformed source (violating REQ-STD-006 conventions) → hard error, never silent best-effort output.
- **Performance contract:** completes in seconds; irrelevant otherwise.
- **Threading / memory contract:** n/a.
- **Test matrix:** unit tests: include-resolution fixtures, guard stripping, determinism (two runs byte-equal), MSVC-guard presence → REQ-INT-005; integration: `quiver_amalgamate_verify` (build + full unit suite + byte-identical kernel outputs vs normal build).
- **Benchmark matrix:** n/a.
- **Documentation:** `docs/guides/vendoring.md`.
- **Acceptance:** verify target green in CI from M8; a fresh consumer project builds `quiver.cpp` + example 01 with a plain compiler invocation documented in the guide.

## 11. MOD-EXAMPLES — examples / demo layer

- **Purpose:** compilable usage documentation and the pipeline demo layer (Charter §6.6) that feeds end-to-end benchmarks and doc snippets.
- **Responsibilities:** the four example programs of [02 §3](02-repository-architecture.md), each ≤ 150 lines, exit code 0 on success, self-checking (verify results, print nothing on success); serve as the only source of doc snippets (REQ-DOC-006).
- **Non-responsibilities:** the Engine Test boundary (Charter §6.6): no schemas, no operator interfaces, no scheduling, no I/O formats. If an example needs any of those, it is cut, not grown.
- **Dependencies:** the library alone (REQ-INT-006). **Dependents:** docs (snippet extraction), CI examples job.
- **Public interfaces:** none (programs).
- **Internal structure:** four standalone `.cpp` files.
- **Lifecycle / state / invariants:** each runs to completion deterministically; asserts its own outputs against precomputed expectations.
- **Failure modes:** non-zero exit on any mismatch (CI-visible).
- **Performance contract:** none (illustrative); pipeline *benchmarking* lives in `bench/pipeline/`, not here.
- **Threading / memory contract:** single-threaded; examples may allocate freely (they are consumers).
- **Test matrix:** CI job builds and runs all four on all tier-1 platforms → REQ-INT-006.
- **Benchmark matrix:** none.
- **Documentation:** referenced by `docs/guides/getting-started.md`; snippet anchors per [14 §5](14-documentation.md).
- **Acceptance:** all examples build warning-clean and exit 0 on all tier-1 CI platforms.

## 12. Traceability

Charter §6.5/§6.6/§7.1, T2/T3/T4/T7 → REQ-CORE-001..004, REQ-INT-001..006 → modules above → tests/benches as tabulated → milestones M1 (CORE/CPU/DISPATCH), M2 (TESTKIT/BENCH), M5 (LEDGER), M8 (AMALG/EXAMPLES). Survey authority: §2.3 (dispatch pattern), §7.4/§7.5 (testkit/bench discipline), §4.2/§3.9 (performance-contract framing).
