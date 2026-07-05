# Vendoring Quiver

Quiver is a dependency-free static library with zero runtime dependencies (Charter T4). It can
be consumed in three ways, each verified in CI from M8 onward (REQ-BUILD-010):

| Mode | Use when | What you get |
|------|----------|--------------|
| **(a) Installed package** | You control the build environment | `find_package(Quiver CONFIG)` → `quiver::quiver` |
| **(b) Source subproject** | You build Quiver from source alongside your project | `FetchContent` / `add_subdirectory` → `quiver::quiver` |
| **(c) Amalgamation drop-in** | You want no build-system entanglement at all | Two files: `quiver.h` + `quiver.cpp` |

## (a) Installed package — `find_package`

```sh
cmake -S quiver -B build -DCMAKE_BUILD_TYPE=Release -DQUIVER_ENABLE_TESTS=OFF
cmake --build build
cmake --install build --prefix /your/prefix
```

```cmake
find_package(Quiver CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE quiver::quiver)
```

The install tree contains exactly the public headers, the static archive, and the CMake package
files — nothing else (REQ-BUILD-009).

## (b) Source subproject — `FetchContent`

```cmake
include(FetchContent)
FetchContent_Declare(Quiver GIT_REPOSITORY https://github.com/div0rce/quiver GIT_TAG vX.Y.Z)
FetchContent_MakeAvailable(Quiver)
target_link_libraries(your_target PRIVATE quiver::quiver)
```

As a subproject Quiver does not build its tests, benchmarks, or examples and does not install
anything; you consume the `quiver::quiver` target directly. `cxx_std_23` is a public usage
requirement, so your target inherits the C++23 standard automatically.

## (c) Amalgamation drop-in

The amalgamation is the single-file pair `quiver.h` + `quiver.cpp` (the proven simdjson/FSST
vendoring pattern). It is **published as a release asset and never committed** to the repository
(REQ-REPO-011); generate it from a source checkout when you need it:

```sh
python3 tools/amalgamate/amalgamate.py --out-dir amalg   # writes amalg/quiver.{h,cpp}
```

Drop the two files into your tree and build with a **single compiler command** — no CMake, no
flags beyond the language standard (the per-ISA backends select their target features internally
via compiler pragmas, ADR-003):

```sh
c++ -std=c++23 -Iamalg amalg/quiver.cpp your_app.cpp -o your_app
```

`#include "quiver.h"` gives the complete public surface. Runtime dispatch still selects the best
backend the CPU supports at run time — the amalgamation compiles every backend into the one
translation unit exactly as the normal multi-file build does.

### How the amalgamation is generated and verified

`amalgamate.py` is a rule-based text transformer (no C++ parsing): it concatenates the public
headers in dependency order into `quiver.h` (include guards stripped, system includes hoisted and
sorted) and the internal headers plus every source file into `quiver.cpp`, in a fixed
deterministic order (ADR-018). Two runs on the same tree produce byte-identical output
(REQ-INT-005). The generator stays trivial because the sources obey the REQ-STD-006
amalgamation-compatibility rules, which `amalgamate.py --check` lints in CI.

The `quiver_amalgamate_verify` build target compiles the generated pair and runs the full unit
suite against it, requiring byte-identical kernel outputs versus the normal multi-file build
(REQ-BUILD-013) — so the drop-in is never a second-class citizen.

### MSVC

On GCC and Clang the single translation unit compiles every backend with no global ISA flags
(per-function target attributes confine each backend's instructions to itself). MSVC has no such
attribute, but it still works — **build the amalgamation with the default `/arch`** (the SSE2
baseline). MSVC self-contains AVX2 and AVX-512 intrinsics at their call sites, so the SIMD
backends compile without `/arch:AVX2`/`/arch:AVX512` while the scalar and dispatch code stay
baseline; runtime dispatch then selects the best backend the CPU supports, exactly as on the
tier-1 platforms. This is verified by the `msvc-amalgamation` CI leg.

Do **not** build the amalgamation with `/arch:AVX2` or higher: `/arch` raises the baseline for the
*whole* translation unit, which can emit AVX2/AVX-512 into the scalar and dispatch code and produce
a binary that crashes on older CPUs. The default `/arch` is the supported configuration; a
narrowing scheme for `/arch`-setting consumers is not implemented (tier-2 deferral, see
[ADR-018](../adr/ADR-018-amalgamation-generation-strategy.md)).
