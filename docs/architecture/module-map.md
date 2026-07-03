# Module map

The file → module ownership record required by REQ-REPO-008: every file belongs to exactly one module. The authoritative module inventory, layering, and dependency rules live in PRD [02 §5–§6](../prd/02-repository-architecture.md); this page tracks the concrete mapping as the repository grows and is updated in every PR that adds files.

**State at M1:** MOD-CORE, MOD-CPU, and MOD-DISPATCH are live; kernels arrive at M3. Rows below marked *predetermined* describe ownership fixed by the PRD for files that do not exist yet; they become *live* at the listed milestone.

## Live

| Path | Module | Notes |
|---|---|---|
| `LICENSE`, `README.md`, `CONTRIBUTING.md`, `CHANGELOG.md`, `SECURITY.md` | process (maintainer) | governance |
| `.clang-format`, `.clang-tidy`, `.gitignore`, `.gitattributes` | process (maintainer) | REQ-STD-002/-007 |
| `CMakeLists.txt`, `CMakePresets.json`, `cmake/**` | build system | PRD [03](../prd/03-build-system.md) |
| `.github/**` | MOD-CI | workflows, templates, repo-lint assets |
| `docs/**` | per [docs/README.md](../README.md) directory map | REQ-DOC-001 |
| `include/quiver/{quiver,core}.h`, `include/quiver/detail/{config,extern_decls}.h` | MOD-CORE | live since M1 |
| `include/quiver/dispatch.h`, `src/dispatch/{dispatch_internal.h,dispatch_tables.cpp,version.cpp}` | MOD-DISPATCH | live since M1 |
| `src/cpu/{cpu_features.h,cpu_features.cpp}` | MOD-CPU | live since M1 |
| `tests/CMakeLists.txt`, `tests/unit/{test_main,test_core,test_dispatch}.cpp` | test suites (MOD-CORE/MOD-DISPATCH validation) | live since M1 |

## Predetermined (from PRD 02 §3/§8; 73 production files total)

| Path pattern | Module | Arrives |
|---|---|---|
| `src/kernels/common/**` | MOD-KCOMMON | M3 |
| `include/quiver/<family>.h`, `src/kernels/<family>/**` (5-file pattern) | MOD-K1…MOD-K10 | M3 (Tier A) / M6 (Tier B) |
| `tests/testkit/**` | MOD-TESTKIT | M2 |
| `tests/{unit,property,differential,invariant,fuzz,regression}/**` | owning kernel family / MOD-DISPATCH / MOD-CORE | M1–M7 |
| `bench/harness/**`, `bench/baselines/**`, `bench/micro/**`, `bench/pipeline/**` | MOD-BENCH | M2–M8 |
| `ledger/**` | MOD-LEDGER | M5 |
| `tools/amalgamate/**` | MOD-AMALG | M8 |
| `examples/**` | MOD-EXAMPLES | M8 |

Cross-module rules in force from M0: no kernel family includes another family's internals (REQ-REPO-009); dev trees never become dependencies of shipped code (REQ-REPO-007); the dependency graph stays acyclic (REQ-REPO-005).
