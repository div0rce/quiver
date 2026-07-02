# 12 — Testing Architecture

## 1. Purpose

The complete validation strategy: taxonomy, the golden-oracle scheme, the differential matrix, fuzzing, sanitizers, static analysis, and failure diagnostics. Every requirement in this PRD maps to at least one validation artifact (master prompt Part 9); the map lives in [01-traceability.md](01-traceability.md). Upstream authority: Charter §9.2 (quality gates), T2/T3; Survey §7.4–§7.5.

## 2. Requirements

| ID | Requirement |
|---|---|
| REQ-TEST-001 | Test categories shall be exactly: unit, property, differential, invariant, fuzz, regression, plus module self-tests — one directory each ([02 §3](02-repository-architecture.md)); every kernel family populates all of the first five. |
| REQ-TEST-002 | **Dual oracle:** kernel correctness is judged against (a) the scalar reference (`_scalar_impl.h` — the specification, Charter T3) and (b) MOD-TESTKIT's independently written naive references (`reference.h`). Unit tests compare scalar-vs-naive (specification validation); differential tests compare every backend vs scalar (implementation validation). A scalar/naive disagreement is a specification bug and blocks everything downstream. |
| REQ-TEST-003 | **Differential matrix (the core artifact):** for every (family API × element type × backend present on the host), **defined output regions** (REQ-MEM-008) shall be compared byte-exactly over: lengths L = {0, 1, 2, 3, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64, 65, 127, 128, 129, 255, 256, 257, 1000, 4096, 65536}; alignment offsets {0, 1, 2, 3} elements; selectivity/null/pattern/value axes per [11 §4](11-performance-ledger.md); family-specific axes (K8 width 0..8·sizeof(Out) exhaustive; K10 boundary sets). PR CI runs full L × alignment with seeded sampling of the remaining axes (fixed seeds); nightly runs the full cross product. |
| REQ-TEST-004 | Float comparisons in differential tests are bit-exact against the policy oracle: testkit implements the ADR-013 accumulation policy parameterized by (ISA lane count), so float sums have an exact expected value per backend; min/max/compare are bit-exact after NaN canonicalization per [08 §3](08-kernel-design.md). |
| REQ-TEST-005 | Every documented invariant shall have a dedicated test: bitmap tail zeroing (ADR-016) — `inv_bitmap_tail.cpp`; produced-selvec sortedness (ADR-025) — `inv_selvec_sorted.cpp`; determinism (two runs, byte-equal outputs; REQ-API-006) — `inv_determinism.cpp`; no allocation (global new/delete counters around every API call; REQ-MEM-003) — `inv_noalloc.cpp`; aliasing allowlist (in-place == out-of-place; ADR-023) — rows inside family unit tests. |
| REQ-TEST-006 | **Guard-page suite:** every kernel × backend shall run with every input and output buffer placed flush against an inaccessible page (both ends tested) across all tail residues — the executable proof of REQ-MEM-001/REQ-SIMD-003. Output buffers are sized to the **capacity region** (REQ-MEM-008), with the guard page at the capacity boundary. Implemented in testkit (`mmap`/`mprotect`, `VirtualAlloc` on Windows tier-2). |
| REQ-TEST-007 | **Fuzzing:** one libFuzzer target per family (`fuzz_<family>.cpp`): fuzz input decoded (first-party bounded decoder) into contract-valid parameters + data; the harness runs **all host-available backends and asserts cross-backend equality** (differential fuzzing) under ASan+UBSan. K8 additionally fuzzes raw packed bytes as untrusted input (REQ-SEC-004). Corpora committed minimized under `tests/fuzz/corpus/<family>/`; PR smoke ≥ 5 min/family batched, nightly ≥ 4 h total. |
| REQ-TEST-008 | Property tests per family (seeded, ≥ 10⁴ cases nightly, 10³ on PR): the properties are enumerated per family in §5 and include the cross-family algebraic identities (K1↔K3↔K2 equivalences, K3 round-trip, K4 De Morgan, K6 composition, K9 composition, K10 count = popcount(bitmap)). |
| REQ-TEST-009 | Sanitizer matrix: ASan+UBSan on every PR (Clang, Debug-with-O2); TSan on dispatch tests every PR; MSan nightly (Clang, instrumented-libc++ build); LSan nightly on test binaries. All release gates require the full matrix green (Charter §9.2). |
| REQ-TEST-010 | Static analysis: clang-tidy with the pinned check set of [17 §6](17-coding-standards.md) on all PRs; warnings-as-errors per REQ-BUILD-008 in CI. |
| REQ-TEST-011 | Every fixed defect shall gain a permanent regression test under `tests/regression/` referencing the issue and violated REQ (master prompt Part 9). |
| REQ-TEST-012 | Determinism of the test suite itself: all randomness seeded; failure output shall include: seed, REQ/invariant ID, family, backend, axis values, first divergence index, and ± 4-element hex context (testkit `assertions.h` format) — sufficient to reproduce with a single re-run command. |
| REQ-TEST-013 | Coverage (llvm-cov) is collected nightly and published as an informational artifact; no numeric coverage gate exists (coverage is diagnostic, not the quality metric — master prompt Part 9). Requirement coverage (the [01](01-traceability.md) matrix) is the binding completeness metric. |
| REQ-TEST-014 | Precondition-violation behavior is tested via assert-death tests: each documented O(1)-checkable precondition ([16 §3](16-error-handling.md)) has a death-test row. Death tests self-skip at runtime (`GTEST_SKIP`) when the library was built with asserts disabled, keyed off a `quiver::detail` asserts-enabled introspection constant — so every suite runs under every preset without configuration-specific test lists. UB-class violations (out-of-range indices in release) are **not** exercised in release tests (they are UB); they are prevented by fuzz-harness construction (ADR-025). |
| REQ-TEST-015 | Cross-platform hash stability: `qhash64` golden vectors committed once (`tests/golden/qhash64_vectors.txt`) and verified on every CI platform (x86-64, ARM64 Linux, macOS, SDE) — REQ-K7-002's executable form. |
| REQ-TEST-016 | The avalanche/bias suite for K7 (first-party, thresholds per ADR-012) runs nightly (compute-heavy) and at every release gate. |
| REQ-TEST-017 | AVX-512 test execution on CI shall run under Intel SDE ([13 §4](13-ci-architecture.md)); SDE runs cover unit + differential + invariant suites (fuzz under SDE is nightly-only, duration-limited). |
| REQ-TEST-018 | Test binaries and GoogleTest shall never link into bench binaries and vice versa (REQ-BENCH-015). |

## 3. Validation hierarchy

```text
Requirements (this PRD)
  └─ invariant tests (REQ-TEST-005/006)          — contracts hold
  └─ unit tests (scalar vs naive; REQ-TEST-002)  — the spec itself is right
  └─ differential matrix + fuzz (003/007)        — every backend equals the spec
  └─ property tests (008)                        — algebra across APIs holds
  └─ sanitizers/static analysis (009/010)        — no UB, no rot
  └─ bench validation (REQ-BENCH-004)            — measured code is correct code
  └─ release gates ([19 §5](19-release-plan.md)) — all of the above, plus ledger duties
```

## 4. Test specification template

Every test file header shall state: purpose (engineering question), scope (module/APIs), inputs (generator + seed policy), expected outputs (oracle), REQs covered, invariants covered, failure definition, and repeatability (deterministic: yes — REQ-TEST-012). This header is the traceability hook the [01](01-traceability.md) matrix references.

## 5. Per-family property inventory (REQ-TEST-008)

| Family | Properties (beyond differential equality) |
|---|---|
| K1 | bitmap form ≡ selvec form (via K3); popcount return ≡ K4 popcount; `kNe` ≡ NOT(`kEq`) for integer types; between ≡ (ge lo) ∧ (le hi) |
| K2 | order preservation (output is a subsequence); count ≡ selection cardinality; filter(bitmap) ≡ filter(bitmap_to_selvec); in-place ≡ out-of-place |
| K3 | round-trip identity both directions; count ≡ popcount |
| K4 | De Morgan (¬(a∧b) ≡ ¬a∨¬b); idempotence (a∧a ≡ a); involution (¬¬a ≡ a); popcount(and) + popcount(andnot) ≡ popcount(a) |
| K5 | take(iota) ≡ copy; take(reverse) ≡ reverse; dict_decode ≡ take(dict, codes-as-indices); fused ≡ decode∘filter of codes |
| K6 | sum(selvec form) ≡ sum(filter → no-selection form); min ≤ every participating element; SMA ≡ (min, max, count composition); checked-sum flag ≡ big-int reference comparison; float sums ≡ policy oracle (REQ-TEST-004) |
| K7 | golden vectors; hash equality across backends and platforms; combine non-commutativity sanity (documented asymmetry); avalanche suite (REQ-TEST-016) |
| K8 | pack∘unpack identity (testkit implements the ADR-026 packer as the inverse oracle); width-0 ≡ constant; unpack_for ≡ unpack + K9 add |
| K9 | validity overload ≡ plain arith + K4 and-combine; wrapping ≡ big-int mod 2ⁿ reference |
| K10 | checked count ≡ popcount(overflow bitmap); wrapped values ≡ K9; zero-overflow inputs ⇒ count 0 and results ≡ K9; saturating ≡ clamp(big-int reference) |

## 6. Failure diagnostics

REQ-TEST-012 defines the format. Additionally: differential failures dump both buffers' divergent windows to a temp artifact path printed in the message; CI uploads these artifacts. GoogleTest `--gtest_filter` + printed seed + axis values constitute the complete reproduction recipe (documented in `docs/testing/`).

## 7. Acceptance criteria

Every REQ in this PRD appears in the [01](01-traceability.md) validation column with at least one artifact from this chapter (or an explicit “process-verified” marker for process REQs); the full matrix + guard-page + invariant suites are green on all tier-1 platforms and SDE; fuzz corpora exist and PR smoke passes; dual-oracle discipline is in place from the first kernel (M3 gate).

## 8. ADR-009 — Testing stack

- **Status:** Accepted.
- **Context:** dev-only dependencies are permitted but must stay pinned and minimal (Charter T4 scoping, REQ-BUILD-007); the oracle scheme (dual oracle + differential fuzzing) is first-party by necessity — no framework provides it.
- **Problem:** select the assertion framework, property-testing approach, and fuzzing engine.
- **Alternatives:** (1) Catch2/doctest — viable; rejected on T8 grounds: GoogleTest is the convention the target contributor pool knows, and death tests (REQ-TEST-014) are first-class there; (2) rapidcheck/fuzztest for properties — rejected: unpinned maturity risk and overlapping roles; seeded first-party generators (MOD-TESTKIT) already provide deterministic property enumeration with better failure diagnostics (REQ-TEST-012); (3) AFL++ — rejected: libFuzzer integrates with the sanitizer toolchain in-process and is OSS-Fuzz-shaped (F-14).
- **Decision:** GoogleTest (pinned) + first-party seeded generators/oracles + libFuzzer with structured differential harnesses.
- **Consequences:** property "shrinking" is manual (seed + axis printing substitutes); acceptable at this API scale.
- **Reconsideration:** if property-test volume outgrows the first-party runner (v2 kernels).
- **Related:** REQ-TEST-001/002/007/008/012, REQ-BUILD-007.

## 9. Traceability

Charter §9.2, T2/T3, §7.2 (sanitizer-clean pledge) → REQ-TEST-001..018 → MOD-TESTKIT ([05 §7](05-internal-architecture.md)) → CI jobs ([13](13-ci-architecture.md)) → milestones M2 (harness), M3 (first full application), all later gates. Survey authority: §7.4 (statistics discipline applies to test-time sampling), §7.5 (pitfall catalog → REQ-TEST-002/003/006), §3.4 (pattern axis rationale).
