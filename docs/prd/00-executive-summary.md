# 00 — Executive Summary

**Quiver Engineering PRD, revision 1.0 — July 2026**

Pipeline position: *Literature Review → Opportunity Analysis → Design Charter → **Engineering PRD** → Implementation.*

---

## 1. Purpose and authority

This PRD is the complete engineering specification for Quiver. It translates the Design Charter (`docs/design/DESIGN_CHARTER.md`, v1.0) into a concrete architecture that an autonomous implementation agent can execute for weeks without making an architecturally significant decision.

Document authority, highest first:

1. Design Charter — product definition. Binding. This PRD implements it exactly and never redefines it.
2. Opportunity Analysis (`docs/research/open-source-opportunity-analysis.md`) — product positioning. This PRD never contradicts it.
3. Literature Review (`docs/research/vectorized-execution-engine-survey.md`) — engineering knowledge. Cited as technical authority ("Survey §n").
4. This PRD — engineering architecture. Binding on implementation.

If implementation discovers a genuine conflict with an upstream document, work **shall stop** and a structured report shall be produced requesting a charter or PRD amendment (per master prompt Part 12). Silent divergence is prohibited.

## 2. Normative conventions

- **shall / shall not / must / must not / required / prohibited** — binding requirements.
- *should* — strong default; deviation requires a documented reason in the PR description.
- Requirement IDs: `REQ-<AREA>-NNN`. Globally unique, never reused, stable across revisions.
- Architectural decisions: `ADR-NNN`, embedded in the owning chapter and indexed in [01-traceability.md](01-traceability.md). ADRs are settled; implementation never reopens them.
- Modules: `MOD-<NAME>`. Public APIs: `API-<AREA>-NNN`. Milestones: `M0`–`M10`. Releases: `v0.1`–`v1.0`.
- All cross-document references use relative links within `docs/prd/`.

## 3. Product summary (from the charter; not restated as authority)

Quiver is a dependency-free C++23 library of ten vectorized analytical kernel families — predicate evaluation, filter/compaction, selection↔bitmap conversion, mask algebra, gather/dictionary decode, reductions/SMA, batch hashing, bit-unpacking, elementwise arithmetic, and overflow-guarded arithmetic — hand-implemented per ISA (scalar, AVX2, NEON, AVX-512) with runtime dispatch, shipped together with a public, PMU-instrumented, statistically rigorous cross-microarchitecture performance ledger. Two co-equal products, one repository: **the library** and **the ledger** (Charter §1).

## 4. Engineering architecture at a glance

| Concern | Decision | ADR | Chapter |
|---|---|---|---|
| Library form | Compiled static library + generated single-pair amalgamation (`quiver.h`/`quiver.cpp`); header-only rejected | ADR-002 | [03](03-build-system.md) |
| Public API style | Concrete per-type symbols behind a thin constrained-template façade; first-party view structs (`BatchView<T>`, `BitmapView`, `SelVec`, `Sma<T>`); no `std::span` in public signatures | ADR-006 | [04](04-public-api.md) |
| ABI | `namespace quiver { inline namespace v1 { … } }` | ADR-007 | [04](04-public-api.md) |
| Per-ISA code | One translation unit per (family × ISA); ISA code enabled by target-attribute macro regions, not global flags — identical mechanism in normal and amalgamated builds | ADR-003 | [09](09-simd-architecture.md) |
| Dispatch | Lazy per-entry resolution through `std::atomic` function pointers; benign-race idempotent init; `QUIVER_ISA` env override + programmatic override; optional eager `warmup()`; compile-time pinning via `QUIVER_PIN_ISA` (REQ-DISP-013) | ADR-004 | [07](07-runtime-dispatch.md) |
| CPU detection | First-party: CPUID+XGETBV (x86), `getauxval` (Linux/ARM), `sysctlbyname` (macOS/ARM) | ADR-005 | [07](07-runtime-dispatch.md) |
| Memory contract | No allocation in any kernel; no read/write outside documented input ranges (no over-read, sanitizer-clean); no `*_padded` variants shipped in v1 (mechanism reserved) | ADR-015, ADR-023 | [06](06-memory-model.md) |
| Error model | All kernels `noexcept`; documented preconditions; debug-mode assertions (`QUIVER_ASSERT`); no error codes on hot paths; K10 reports overflow via return count + optional position bitmap | ADR-017, ADR-014 | [16](16-error-handling.md) |
| Hash algorithm | `qhash64` v1: Murmur3-style 64-bit finalizer with seed pre-mix; cross-ISA/platform bit-identical; frozen test vectors | ADR-012 | [08](08-kernel-design.md) |
| Float reductions | Fixed blocked multi-accumulator reassociation per (version, ISA); NaN-propagating min/max; deterministic identity elements for empty reductions | ADR-013 | [08](08-kernel-design.md) |
| Benchmarks | Google Benchmark (dev-only, pinned) for microbenchmarks + first-party ledger runner (Python 3.11+, stdlib-only) for process-level repetitions, manifests, statistics | ADR-008 | [10](10-benchmark-architecture.md) |
| Honest baseline | Equal-ISA auto-vectorized baselines: scalar reference recompiled under each ISA target region so the compiler competes at the same ISA level | ADR-011 | [10](10-benchmark-architecture.md), [11](11-performance-ledger.md) |
| PMU | First-party `perf_event_open` wrapper (Linux); Apple entries wall-clock-only, labeled secondary | ADR-022 | [10](10-benchmark-architecture.md) |
| Testing | GoogleTest (dev-only, pinned) + first-party seeded generators + golden scalar oracle + cross-ISA differential + libFuzzer/ASan/UBSan | ADR-009 | [12](12-testing-architecture.md) |
| CI | GitHub Actions: x86-64, ARM64 (Graviton-class), macOS M-series runners; AVX-512 correctness via Intel SDE emulation; MSVC tier-2 non-blocking | ADR-010 | [13](13-ci-architecture.md) |
| Docs | MkDocs Material (dev-only, pinned); hand-written API reference; all examples compiled in CI; ADRs materialized as files under `docs/adr/` | ADR-019 | [14](14-documentation.md) |
| Statistics | ≥10 process-level repetitions; median + min + seeded percentile-bootstrap 95% CIs; CV thresholds 3% (warn) / 5% (exclude) | ADR-020 | [11](11-performance-ledger.md) |
| Releases | SemVer; trunk-based; v0.1→v1.0 progression mapped to milestones M0–M10; 9-month shrink point = end of M5 | ADR-024 | [19](19-release-plan.md) |

## 5. Kernel catalog (charter §6.1, closed)

| # | Family | Module | Public header | Tier | First release |
|---|---|---|---|---|---|
| K1 | compare | MOD-K1-COMPARE | `quiver/compare.h` | A | v0.1 (scalar) |
| K2 | filter | MOD-K2-FILTER | `quiver/filter.h` | A | v0.1 |
| K3 | sel_convert | MOD-K3-SELECT | `quiver/select.h` | A | v0.1 |
| K4 | mask_algebra | MOD-K4-MASK | `quiver/mask.h` | A | v0.1 |
| K5 | take / dict_decode | MOD-K5-TAKE | `quiver/take.h` | A | v0.1 |
| K6 | reduce / SMA | MOD-K6-REDUCE | `quiver/reduce.h` | A | v0.1 |
| K7 | hash | MOD-K7-HASH | `quiver/hash.h` | B | v0.4 |
| K8 | unpack | MOD-K8-UNPACK | `quiver/unpack.h` | B | v0.4 |
| K9 | arith | MOD-K9-ARITH | `quiver/arith.h` | B | v0.4 |
| K10 | arith_guarded | MOD-K10-ARITH-GUARDED | `quiver/arith.h` | B | v0.4 |

The catalog is closed for v1 (Charter §6.1). Adding a family requires a charter amendment, not a PRD change.

## 6. Version roadmap summary

| Release | Milestones | Capability |
|---|---|---|
| v0.1 | M0–M3 | Core types, dispatch skeleton, test/bench harnesses, Tier A scalar kernels |
| v0.2 | M4 | Tier A AVX2 + differential tests + fuzzing |
| v0.3 | M5 | Tier A NEON, ledger v1 on ≥3 microarchitectures — **public launch; charter 6-month gate; pre-authorized 9-month shrink point** |
| v0.4 | M6 | Tier B (K7–K10) scalar + AVX2 + NEON |
| v0.5 | M7 | AVX-512 across all families |
| v0.6 | M8 | Amalgamation, packaging (vcpkg/Conan groundwork), examples, dispatch hardening |
| — | M9 | Representation study, ledger ≥5 microarchitectures, workshop paper (no library release required) |
| v1.0 | M10 | API freeze of surfaces A–C; charter §9.1 18-month criteria |

Full definitions: [18-milestones.md](18-milestones.md), [19-release-plan.md](19-release-plan.md).

## 7. Document map

| File | Contents |
|---|---|
| [README.md](README.md) | PRD index, glossary, status |
| [00-executive-summary.md](00-executive-summary.md) | This file |
| [01-traceability.md](01-traceability.md) | Charter→REQ map, ADR index, master requirement traceability matrix |
| [02-repository-architecture.md](02-repository-architecture.md) | Layout, directory ownership, module inventory, dependency DAG, file inventory, implementation order |
| [03-build-system.md](03-build-system.md) | CMake architecture, targets, flags, amalgamation, packaging |
| [04-public-api.md](04-public-api.md) | Complete public surface: types, all kernel APIs, dispatch APIs, compatibility rules |
| [05-internal-architecture.md](05-internal-architecture.md) | Non-kernel module specifications (core, cpu, dispatch, testkit, bench harness, ledger runner, amalgamator, examples) |
| [06-memory-model.md](06-memory-model.md) | Ownership, bounds, alignment, aliasing, bitmap/selvec representations |
| [07-runtime-dispatch.md](07-runtime-dispatch.md) | Feature detection, dispatch tables, override, lifecycle, state machine |
| [08-kernel-design.md](08-kernel-design.md) | Common kernel contract + per-family specifications K1–K10, algorithms per ISA |
| [09-simd-architecture.md](09-simd-architecture.md) | ISA organization, target regions, tail policy, compaction algorithms, per-ISA notes |
| [10-benchmark-architecture.md](10-benchmark-architecture.md) | Harness, categories, generators, PMU, regression policy |
| [11-performance-ledger.md](11-performance-ledger.md) | Ledger data model, schema, statistics, machine registry, publication workflow |
| [12-testing-architecture.md](12-testing-architecture.md) | Test taxonomy, golden oracle, differential matrix, fuzzing, sanitizers, static analysis |
| [13-ci-architecture.md](13-ci-architecture.md) | Workflows, runner matrix, gates, SDE, nightly |
| [14-documentation.md](14-documentation.md) | Docs hierarchy, per-kernel template, ADR materialization, site tooling |
| [15-security-and-ub.md](15-security-and-ub.md) | UB catalog, bounds discipline, untrusted-input posture, supply chain |
| [16-error-handling.md](16-error-handling.md) | Precondition model, assertions, overflow reporting, diagnostics |
| [17-coding-standards.md](17-coding-standards.md) | Language subset, naming, formatting, prohibited constructs, review rules |
| [18-milestones.md](18-milestones.md) | M0–M10 full specifications with file lists and release gates |
| [19-release-plan.md](19-release-plan.md) | Versioning, branching, release process, gate checklists |
| [20-risk-register.md](20-risk-register.md) | Engineering risk register with mitigations and owners |
| [21-future-work.md](21-future-work.md) | Deferred items (charter §8.1) with revisit conditions; explicitly out of v1 |
| [22-final-review-checklist.md](22-final-review-checklist.md) | Master-prompt Part 13 checklist, evaluated, with certification |
| [REVIEW_REPORT.md](REVIEW_REPORT.md) | Consistency review against the master prompt |

## 8. Reading order for the implementation agent

1. This file, then [README.md](README.md) (glossary).
2. [02](02-repository-architecture.md) → [03](03-build-system.md) — where everything lives and how it builds.
3. [04](04-public-api.md) → [06](06-memory-model.md) → [16](16-error-handling.md) — the contracts.
4. [07](07-runtime-dispatch.md) → [08](08-kernel-design.md) → [09](09-simd-architecture.md) — what to build.
5. [12](12-testing-architecture.md) → [10](10-benchmark-architecture.md) → [11](11-performance-ledger.md) — how it is validated and measured.
6. [18](18-milestones.md) — the execution plan. Implement strictly in milestone order.

At any ambiguity: search this PRD → ADRs → requirements → module specs; if still ambiguous, stop and report (master prompt Part 12). Never invent.
