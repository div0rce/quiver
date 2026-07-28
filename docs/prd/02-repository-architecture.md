# 02 — Repository Architecture

## 1. Purpose

Defines the complete repository organization: every directory, its ownership, its dependency rules, the module inventory, the dependency DAG, and the deterministic implementation order. The implementation agent shall never decide repository organization independently.

## 2. Scope

Everything in the repository except file *contents* (specified in later chapters). Build-target wiring is in [03-build-system.md](03-build-system.md).

## 3. Canonical repository layout

```text
quiver/
├── LICENSE                      # Apache-2.0 (Charter §12)
├── README.md                    # project front page
├── CONTRIBUTING.md              # workflow, DCO, review standards (ch. 14, 17)
├── CHANGELOG.md                 # Keep-a-Changelog format
├── CMakeLists.txt               # root build (ch. 03)
├── CMakePresets.json            # canonical build presets (ch. 03 §REQ-BUILD-011)
├── .clang-format  .clang-tidy  .gitignore  .gitattributes
├── cmake/                       # build-system modules (ch. 03)
│   ├── deps.cmake               # pinned dev-only dependencies
│   ├── flags.cmake              # warning/optimization/feature flags
│   ├── sanitizers.cmake         # ASan/UBSan/TSan/MSan configs
│   └── QuiverConfig.cmake.in    # install/package config template
├── include/quiver/              # THE public surface (Surface A–C, Charter §7.1)
│   ├── quiver.h                 # umbrella header
│   ├── core.h                   # vocabulary types, enums, concepts (Surface B)
│   ├── dispatch.h               # ISA query/override, version (Surface C)
│   ├── compare.h  filter.h  select.h  mask.h  take.h
│   ├── reduce.h   hash.h    unpack.h  arith.h          # (arith.h = K9 + K10)
│   └── detail/
│       ├── config.h             # QUIVER_* macros, assert, inline-namespace defs
│       └── extern_decls.h       # per-type concrete symbol declarations (generated-by-hand, stable)
├── src/                         # implementation (internal)
│   ├── cpu/
│   │   ├── cpu_features.h       # internal interface of MOD-CPU
│   │   └── cpu_features.cpp
│   ├── dispatch/
│   │   ├── dispatch_internal.h  # table types, resolver protocol (MOD-DISPATCH)
│   │   ├── dispatch_tables.cpp
│   │   └── version.cpp
│   └── kernels/
│       ├── common/
│       │   ├── kernel_common.h  # tail helpers, bitmap word ops, contracts helpers
│       │   ├── target_regions.h # QUIVER_TARGET_*_BEGIN/END macros (ch. 09)
│       │   └── luts.h/.cpp      # shared shuffle LUTs (AVX2/NEON compaction)
│       ├── compare/             # K1  — pattern identical for all families:
│       │   ├── compare_scalar_impl.h   # reference implementation (the specification, T3)
│       │   ├── compare_scalar.cpp      # instantiates _impl.h as the scalar backend
│       │   ├── compare_avx2.cpp
│       │   ├── compare_neon.cpp
│       │   └── compare_avx512.cpp
│       ├── filter/      ├── select/     ├── mask/       ├── take/
│       ├── reduce/      ├── hash/       ├── unpack/     ├── arith/
│       └── arith_guarded/               # same 5-file pattern each
├── tests/                       # dev-only (never installed/vendored)
│   ├── CMakeLists.txt
│   ├── testkit/                 # MOD-TESTKIT: generators, oracle plumbing
│   │   ├── generators.h/.cpp    # seeded distributions (§ch.12)
│   │   ├── reference.h          # naive re-implementations used as second oracle
│   │   └── assertions.h         # bitmap/selvec equality with diagnostics
│   ├── unit/                    # test_<family>.cpp ×10, test_core.cpp, test_dispatch.cpp
│   ├── property/                # prop_<family>.cpp ×10
│   ├── differential/            # diff_isa_<family>.cpp ×10 (cross-ISA equality)
│   ├── invariant/               # inv_bitmap_tail.cpp, inv_determinism.cpp, inv_selvec_sorted.cpp, inv_noalloc.cpp
│   ├── fuzz/                    # fuzz_<family>.cpp ×10 + corpus/<family>/
│   └── regression/              # one file per fixed defect + README.md
├── bench/                       # dev-only (never installed/vendored)
│   ├── CMakeLists.txt
│   ├── harness/                 # MOD-BENCH: distributions.h/.cpp, pmu.h/.cpp, meta.h/.cpp
│   ├── baselines/               # autovec baselines: baseline_avx2.cpp, baseline_avx512.cpp,
│   │                            #   baseline_neon.cpp (recompile *_scalar_impl.h per target, ADR-011)
│   ├── micro/                   # bench_<family>.cpp ×10, bench_dispatch.cpp
│   └── pipeline/                # bench_pipeline.cpp (filter→take→reduce; demo layer, Charter §6.6)
├── ledger/                      # the second product (Charter §1)
│   ├── schema/                  # ledger-entry.schema.json, manifest.schema.json (Surface D)
│   ├── runner/                  # quiver_ledger.py + package modules (MOD-LEDGER)
│   ├── machines/                # registered reference machines: <machine-id>.json
│   └── results/                 # committed results: <uarch>/<yyyymmdd>-<shortsha>/*.json
├── tools/
│   └── amalgamate/              # MOD-AMALG: amalgamate.py, test_amalgamate.py
├── examples/                    # MOD-EXAMPLES: compiled in CI, excluded from install
│   ├── 01_minimal_filter.cpp
│   ├── 02_filter_take_reduce.cpp
│   ├── 03_isa_override.cpp
│   └── 04_nullable_pipeline.cpp
├── docs/
│   ├── research/  design/  prompts/  prd/     # existing pipeline documents
│   ├── adr/                     # ADR-001..N materialized as standalone files (M0)
│   ├── architecture/            # per-module architecture pages
│   ├── api/                     # per-family API reference pages
│   ├── benchmarks/              # methodology + interpretation guides
│   ├── guides/                  # getting-started, vendoring, building
│   ├── internals/               # SIMD notes, dispatch internals
│   ├── testing/                 # test taxonomy and how-to
│   ├── releases/                # release notes per version
│   └── mkdocs.yml               # site config (ch. 14)
└── .github/
    ├── workflows/               # ci.yml, nightly.yml, release.yml (ch. 13)
    ├── ISSUE_TEMPLATE/          # bug.yml, benchmark-dispute.yml (Charter §12)
    └── PULL_REQUEST_TEMPLATE.md
```

## 4. Directory ownership table

| Directory | Purpose | Owner module(s) | May depend on | May be depended on by | Must never contain |
|---|---|---|---|---|---|
| `include/quiver/` | Public surface | MOD-CORE, MOD-DISPATCH, MOD-K1..K10 | C++ standard library only | everything | implementation logic beyond the template façade; allocation; I/O |
| `include/quiver/detail/` | Public-but-unstable support | MOD-CORE | std only | public headers | anything documented as stable |
| `src/cpu/` | CPU feature detection | MOD-CPU | std, OS APIs (cpuid/auxval/sysctl) | `src/dispatch/` | kernel logic |
| `src/dispatch/` | Dispatch tables, version | MOD-DISPATCH | `src/cpu/`, `include/` | kernel TUs (registration), public API | kernel logic |
| `src/kernels/common/` | Shared kernel utilities | MOD-KCOMMON | `include/`, std | all kernel modules | family-specific logic |
| `src/kernels/<family>/` | One kernel family | MOD-K1..K10 | `common/`, `include/`, `dispatch_internal.h` | dispatch tables | other families' internals |
| `tests/` | All validation | MOD-TESTKIT + per-family tests | library, GoogleTest, testkit | — | production code |
| `bench/` | All measurement | MOD-BENCH | library, Google Benchmark, harness | ledger runner (invokes binaries) | test code; production code |
| `ledger/` | Ledger product | MOD-LEDGER | bench binaries (subprocess), Python 3.11 stdlib | — | C++ code |
| `tools/amalgamate/` | Amalgamation generator | MOD-AMALG | Python 3.11 stdlib; reads `include/` + `src/` | release workflow | runtime logic |
| `examples/` | Compilable usage examples | MOD-EXAMPLES | library only | docs (inclusion by reference) | test/bench dependencies |
| `docs/` | All documentation | per owning module (ch. 14) | — | — | source code (snippets are extracted from `examples/`) |
| `.github/` | CI/CD | MOD-CI (process module) | — | — | — |

## 5. Module inventory

Layers: **L0** = no internal deps; **L1** = depends only on L0; etc. Priority = implementation order rank.

| Module ID | Layer | Responsibility (one line) | Public interfaces | Depends on | Milestone | Spec |
|---|---|---|---|---|---|---|
| MOD-CORE | L0 | Vocabulary types, enums, concepts, config macros, assert | `core.h`, `detail/config.h` | std | M1 | [05 §3](05-internal-architecture.md) |
| MOD-CPU | L0 | OS/ISA feature detection | internal `cpu_features.h` | std, OS | M1 | [05 §4](05-internal-architecture.md) |
| MOD-DISPATCH | L1 | Dispatch tables, override, version introspection | `dispatch.h` | MOD-CORE, MOD-CPU | M1 | [05 §5](05-internal-architecture.md), [07](07-runtime-dispatch.md) |
| MOD-KCOMMON | L1 | Shared kernel utilities, target-region macros, LUTs | internal `kernel_common.h` | MOD-CORE | M3 | [05 §6](05-internal-architecture.md) |
| MOD-K1-COMPARE … MOD-K6-REDUCE | L2 | Tier A kernel families | `compare.h`…`reduce.h` | MOD-CORE, MOD-KCOMMON, MOD-DISPATCH | M3–M5 | [08](08-kernel-design.md) |
| MOD-K7-HASH … MOD-K10-ARITH-GUARDED | L2 | Tier B kernel families | `hash.h`…`arith.h` | same | M6–M7 | [08](08-kernel-design.md) |
| MOD-TESTKIT | dev | Seeded generators, second-oracle references, diagnostics | `tests/testkit/*` | library, GoogleTest | M2 | [05 §7](05-internal-architecture.md) |
| MOD-BENCH | dev | Benchmark harness: distributions, PMU, metadata emission | `bench/harness/*` | library, Google Benchmark | M2 | [05 §8](05-internal-architecture.md), [10](10-benchmark-architecture.md) |
| MOD-LEDGER | dev | Ledger runner, schemas, statistics, manifests | `ledger/runner`, `ledger/schema` | bench binaries | M5 | [05 §9](05-internal-architecture.md), [11](11-performance-ledger.md) |
| MOD-AMALG | dev | Amalgamation generator + verification | `tools/amalgamate` | repo sources | M8 | [05 §10](05-internal-architecture.md) |
| MOD-EXAMPLES | dev | Compiled examples / demo layer | `examples/*` | library | M8 | [05 §11](05-internal-architecture.md) |
| MOD-CI | process | Workflows, gates | `.github/*` | all | M0+ | [13](13-ci-architecture.md) |

Seventeen modules total (counting K1–K10 individually). Every source file belongs to exactly one module (REQ-REPO-008).

## 6. Dependency DAG

```text
Title: Repository dependency graph (arrows point at dependencies)
Purpose: derive implementation order; enforce layering (REQ-REPO-005)

                 ┌──────────┐   ┌─────────┐
                 │ MOD-CORE │   │ MOD-CPU │            L0
                 └────▲─────┘   └────▲────┘
                      │              │
                ┌─────┴──────────────┴───┐
                │      MOD-DISPATCH      │             L1
                └─────▲──────────────────┘
                      │        ┌─────────────┐
                      │        │ MOD-KCOMMON │────► MOD-CORE
                      │        └──────▲──────┘
                ┌─────┴───────────────┴─────┐
                │   MOD-K1 … MOD-K10        │          L2
                └─────▲────────▲────────▲───┘
                      │        │        │
              ┌───────┴──┐ ┌───┴────┐ ┌─┴──────────┐
              │MOD-TESTKIT│ │MOD-BENCH│ │MOD-EXAMPLES│  dev
              └───────────┘ └───▲────┘ └────────────┘
                                │
                          ┌─────┴─────┐
                          │ MOD-LEDGER│ (subprocess boundary)
                          └───────────┘
   MOD-AMALG reads include/ + src/ as text; no link-time dependency.
```

Rules:
- The graph shall remain acyclic (REQ-REPO-005).
- Kernel modules shall not depend on each other. Cross-family composition exists only in `bench/pipeline/` and `examples/` (REQ-REPO-009). The single exception is documented in [08](08-kernel-design.md): K6 `reduce_count_valid` and K9's validity-combining overload delegate to K4's public API (a public-interface dependency, not an internals dependency).
- Dev modules shall never be dependencies of shipped code (REQ-REPO-007).

## 7. Visibility classification

| Category | Location | Stability |
|---|---|---|
| Public API (Surfaces A–C) | `include/quiver/*.h` | Frozen at v1.0 (Charter §7.5) |
| Public-but-unstable | `include/quiver/detail/` | No guarantees; documented as internal |
| Internal | `src/**` | None |
| Testing-only | `tests/**` | None |
| Benchmarking-only | `bench/**` | None |
| Ledger schema (Surface D) | `ledger/schema/*.json` | Independently versioned (Charter §7.5) |
| Documentation-only | `docs/**`, `examples/**` | Examples compile-checked in CI |

## 8. Production file inventory

The complete set of shipped-library files. The implementation agent shall create exactly these files (per milestone lists in [18](18-milestones.md)); adding a production file requires a PRD amendment.

- `include/quiver/`: 14 headers (§3 tree: umbrella, `core.h`, `dispatch.h`, 9 kernel headers, `detail/config.h`, `detail/extern_decls.h`).
- `src/cpu/`: 2 files. `src/dispatch/`: 3 files. `src/kernels/common/`: 4 files (`kernel_common.h`, `target_regions.h`, `luts.h`, `luts.cpp`).
- `src/kernels/<family>/` ×10 families × 5 files each (`<f>_scalar_impl.h`, `<f>_scalar.cpp`, `<f>_avx2.cpp`, `<f>_neon.cpp`, `<f>_avx512.cpp`) = 50 files.

Total shipped-library production files: **73**. Test/bench/tool file inventories are given in their chapters and in [18](18-milestones.md).

## 9. Implementation order and parallelization

| Phase | Modules | Prerequisites | Parallelizable within phase |
|---|---|---|---|
| 1 (M0) | repo skeleton, MOD-CI seed | — | n/a |
| 2 (M1) | MOD-CORE, MOD-CPU → MOD-DISPATCH | phase 1 | CORE ∥ CPU; DISPATCH after both |
| 3 (M2) | MOD-TESTKIT, MOD-BENCH | phase 2 | yes (mutually independent) |
| 4 (M3) | MOD-KCOMMON, then K1–K6 scalar | phase 3 | K1–K6 mutually parallel after KCOMMON |
| 5 (M4) | K1–K6 AVX2 | phase 4 | families parallel |
| 6 (M5) | K1–K6 NEON; MOD-LEDGER | phase 5 | NEON families parallel; LEDGER parallel with NEON |
| 7 (M6) | K7–K10 scalar+AVX2+NEON | phase 6 | families parallel |
| 8 (M7) | all families AVX-512 | phase 7 | families parallel |
| 9 (M8) | MOD-AMALG, MOD-EXAMPLES, packaging | phase 8 | yes |
| 10 (M9–M10) | study, freeze | phase 9 | n/a |

## 10. ADR-001 — Repository layout and module boundaries

- **Status:** Accepted.
- **Context:** Charter T4 (vendorable), T8 (boring on purpose); master prompt Part 5 requires a fully predetermined layout; the amalgamation (ADR-002/018) must be generatable by concatenation-with-rules, which constrains file structure.
- **Problem:** Choose a layout that (a) keeps the public surface minimal and obvious, (b) isolates per-ISA code into units that can carry target regions, (c) lets the scalar reference be recompiled by the bench baselines (ADR-011), (d) keeps dev trees strictly out of the shipped surface.
- **Constraints:** zero shipped dependencies; 10 closed families; per-family independent shippability (OA §10); solo maintainability.
- **Alternatives considered:**
  1. *Single `src/` pool with per-ISA suffixes* — rejected: weak family boundaries, painful file inventory for milestones.
  2. *Monolithic `quiver.h` header-only tree* — rejected with ADR-002 (per-ISA target regions in one header explode compile times; MSVC cannot compile AVX-512 regions without per-TU flags).
  3. *Per-family subdirectory with `_impl.h` reference + per-ISA TUs* (selected).
  4. *Bazel-style fine-grained packages* — rejected: CMake is the ecosystem default for vendorable C++ (T8).
- **Decision:** Layout of §3; five-file pattern per family; `_impl.h` scalar reference is the single source of truth included by both the scalar TU and bench baselines.
- **Consequences:** + deterministic milestones, per-family parallel implementation, trivially explainable to contributors; − 50 kernel files (mitigated: identical pattern), the `_impl.h` must remain target-region-clean (enforced by REQ-SIMD-006).
- **Reconsideration:** if a v2 compressed-kernel expansion (Charter §8.1) needs per-encoding sub-modules.
- **Related:** REQ-REPO-001..012, ADR-002, ADR-003, ADR-011.

## 11. Requirements

| ID | Requirement | Acceptance |
|---|---|---|
| REQ-REPO-001 | The repository shall match the §3 layout exactly; new top-level directories require a PRD amendment. Gitignored developer-local trees (build outputs, virtualenvs, IDE state) are not repository layout and are carried in the manifest allow-list as literal names or fnmatch globs; they may never hold committed content. | CI job compares tree against a committed manifest. |
| REQ-REPO-002 | All public declarations shall live under `include/quiver/`; no other header shall be installed. | Install-tree inspection test (M8). |
| REQ-REPO-003 | Each kernel family shall consist of exactly the five-file pattern of §3. | File-inventory check. |
| REQ-REPO-004 | Scalar reference logic shall live in `<family>_scalar_impl.h`, free of ISA intrinsics and target regions, includable by bench baselines. | Grep-based CI lint + baseline TUs compile. |
| REQ-REPO-005 | The module dependency graph shall remain acyclic and layer-respecting per §6. | include-graph lint in CI (clang-based or script). |
| REQ-REPO-006 | Shipped-library code shall include only `include/quiver/**`, `src/**`, and the C++ standard library. | Include lint. |
| REQ-REPO-007 | `tests/`, `bench/`, `ledger/`, `tools/`, `examples/` shall be excluded from install and amalgamation outputs. | Install/amalgamation content test. |
| REQ-REPO-008 | Every file shall belong to exactly one module per §5; the mapping shall be recorded in `docs/architecture/module-map.md`. | Doc review gate. |
| REQ-REPO-009 | No kernel module shall include another family's internal headers; cross-family use of *public* APIs is permitted only where documented in [08](08-kernel-design.md). | Include lint. |
| REQ-REPO-010 | Implementation shall follow the phase order of §9; a module shall not start before its prerequisites' acceptance criteria pass. | Milestone gates ([18](18-milestones.md)). |
| REQ-REPO-011 | Amalgamation outputs shall be published as release assets and shall not be committed to the repository. | Release workflow inspection. |
| REQ-REPO-012 | The `docs/` subdirectory hierarchy of §3 shall exist from M0 with a `README.md` stating each directory's purpose and owner. | M0 gate. |

## 12. Acceptance criteria (chapter)

All twelve REQ-REPO acceptance checks pass; every directory in §3 has an owner in §4; §5 inventory covers every file in §8; §6 DAG is acyclic; §9 order is derivable from §6 mechanically.

## 13. Traceability

Charter T1/T4/T8, §6.5, §7.1 → REQ-REPO-001..012 → ADR-001 → milestones M0–M8. Upstream method authority: OA §10 (per-family independent shippability), master prompt Part 5.
