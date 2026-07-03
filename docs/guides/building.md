# Building Quiver

> **M0 status:** the build is a **configure-only skeleton** — the option surface, presets, and toolchain checks are live, but no build targets exist yet. The library target (`quiver::quiver`) arrives at milestone M1; tests at M1+; benchmarks at M2 (PRD [18-milestones.md](../prd/18-milestones.md)).

## Prerequisites

- CMake ≥ 3.28 (REQ-BUILD-001)
- A tier-1 compiler: GCC ≥ 13, Clang ≥ 17, or AppleClang ≥ 16 (Xcode 16). MSVC ≥ 19.40 is tier-2 best-effort. Toolchain matrix: PRD [03 §7](../prd/03-build-system.md).
- For the documentation site: Python ≥ 3.11 and `pip install -r docs/requirements.txt`.

## Configure

Always use presets (REQ-BUILD-011 — CI invokes presets only, never ad-hoc flags):

```sh
cmake --preset dev        # Debug, asserts on, tests on
cmake --preset release    # Release, tests on
cmake --preset asan-ubsan # sanitized (forces asserts on, REQ-BUILD-012)
```

Preset inventory: `dev`, `release`, `bench`, `asan-ubsan`, `tsan`, `msan`, `ci-gcc`, `ci-clang`, `ci-msvc` (see `CMakePresets.json`). Build trees land in `build/<preset>/`.

## Options

The exact option surface (REQ-BUILD-006 — additions require a PRD amendment):

| Option | Default | Meaning |
|---|---|---|
| `QUIVER_ENABLE_TESTS` | ON top-level / OFF subproject | test suites |
| `QUIVER_ENABLE_BENCH` | OFF | benchmark suites |
| `QUIVER_ENABLE_EXAMPLES` | = tests default | examples |
| `QUIVER_ENABLE_ASSERTS` | ON for Debug | `QUIVER_ASSERT` contract checks |
| `QUIVER_ENABLE_WERROR` | OFF (ON in CI) | warnings as errors |
| `QUIVER_SANITIZE` | empty | `address;undefined;thread;memory` list |
| `QUIVER_DISABLE_AVX512` | OFF | broken-toolchain escape hatch only |
| `QUIVER_PIN_ISA` | empty | compile-time ISA pinning (REQ-DISP-013): `scalar`/`neon`/`avx2`/`avx512` |

`BUILD_SHARED_LIBS` is ignored with a warning — Quiver v1 is static-only (REQ-BUILD-002).

## Building the documentation site

```sh
python3 -m venv .venv && . .venv/bin/activate
pip install -r docs/requirements.txt
mkdocs build --strict -f docs/mkdocs.yml   # broken links fail the build (REQ-DOC-005)
mkdocs serve -f docs/mkdocs.yml            # local preview
```

## Consuming Quiver (from M8)

Three supported modes — installed package (`find_package(Quiver CONFIG)`), `FetchContent`/`add_subdirectory`, and the two-file amalgamation — all CI-verified from milestone M8 (REQ-BUILD-010; vendoring guide arrives then).

[deliberately broken link](does-not-exist.md)
