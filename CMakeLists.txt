# Quiver root build.
# Owning module: build system (docs/prd/03-build-system.md).
# REQ-BUILD-001..008, -012; library target per REQ-BUILD-002 (ADR-002).
cmake_minimum_required(VERSION 3.28)

# Version single-source-of-truth: parsed from the header so the M1 acceptance criterion
# "version introspection matches the CMake project version" holds structurally.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/include/quiver/detail/config.h" _quiver_ver_lines
     REGEX "#define QUIVER_VERSION_(MAJOR|MINOR|PATCH) ")
string(REGEX MATCH "MAJOR ([0-9]+)" _ "${_quiver_ver_lines}")
set(_quiver_ver_major "${CMAKE_MATCH_1}")
string(REGEX MATCH "MINOR ([0-9]+)" _ "${_quiver_ver_lines}")
set(_quiver_ver_minor "${CMAKE_MATCH_1}")
string(REGEX MATCH "PATCH ([0-9]+)" _ "${_quiver_ver_lines}")
set(_quiver_ver_patch "${CMAKE_MATCH_1}")

project(Quiver
        VERSION ${_quiver_ver_major}.${_quiver_ver_minor}.${_quiver_ver_patch}
        DESCRIPTION "Dependency-free C++23 vectorized analytical kernels + cross-ISA performance ledger"
        HOMEPAGE_URL "https://github.com/div0rce/quiver"
        LANGUAGES CXX)

# --- Language floor (REQ-BUILD-001, Charter §7.6) --------------------------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# No C++20 modules in v1: CMake's Ninja+clang module scanning would require clang-scan-deps,
# which minimal build environments lack — disable the scan outright.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# --- Shared-library builds are out of scope for v1 (REQ-BUILD-002, F-10 in future work) ------
if(BUILD_SHARED_LIBS)
  message(WARNING
    "Quiver v1 builds as a static library only; BUILD_SHARED_LIBS is ignored "
    "(REQ-BUILD-002; see docs/prd/21-future-work.md F-10).")
endif()

# --- Option surface (REQ-BUILD-006 — exact list; new options require a PRD amendment) --------
if(PROJECT_IS_TOP_LEVEL)
  set(_quiver_dev_default ON)
else()
  set(_quiver_dev_default OFF)
endif()
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(_quiver_asserts_default ON)
else()
  set(_quiver_asserts_default OFF)
endif()

option(QUIVER_ENABLE_TESTS    "Build Quiver test suites"                 ${_quiver_dev_default})
option(QUIVER_ENABLE_BENCH    "Build Quiver benchmark suites"            OFF)
option(QUIVER_ENABLE_EXAMPLES "Build Quiver examples"                    ${_quiver_dev_default})
option(QUIVER_ENABLE_ASSERTS  "Enable QUIVER_ASSERT contract checks"     ${_quiver_asserts_default})
option(QUIVER_ENABLE_WERROR   "Treat warnings as errors (CI)"            OFF)
option(QUIVER_ENABLE_FUZZERS  "Build differential libFuzzer targets (Clang-only, REQ-TEST-007)" OFF)
option(QUIVER_DISABLE_AVX512  "Escape hatch: skip AVX-512 backend TUs (broken-toolchain only)" OFF)
set(QUIVER_SANITIZE "" CACHE STRING
    "Semicolon list of sanitizers: address;undefined;thread;memory (REQ-BUILD-012)")
set(QUIVER_PIN_ISA "" CACHE STRING
    "Compile-time ISA pinning: scalar|neon|avx2|avx512; empty = runtime dispatch (REQ-DISP-013)")

# --- Option validation (docs/prd/03-build-system.md §8) --------------------------------------
set(_quiver_pin_allowed "" "scalar" "neon" "avx2" "avx512")
if(NOT QUIVER_PIN_ISA IN_LIST _quiver_pin_allowed)
  message(FATAL_ERROR
    "QUIVER_PIN_ISA='${QUIVER_PIN_ISA}' is invalid; allowed: scalar, neon, avx2, avx512, "
    "or empty for runtime dispatch (REQ-DISP-013).")
endif()

if(QUIVER_DISABLE_AVX512)
  message(WARNING
    "QUIVER_DISABLE_AVX512 is a documented broken-toolchain escape hatch only; "
    "shipped builds include all backend TUs (REQ-KERNEL-006).")
endif()

# --- Build-system modules --------------------------------------------------------------------
include(cmake/flags.cmake)       # warning baseline, library constraints, toolchain minimums
include(cmake/sanitizers.cmake)  # QUIVER_SANITIZE validation + apply function (REQ-BUILD-012)
include(cmake/deps.cmake)        # pinned dev-dependency declarations (REQ-BUILD-007)

# Sanitized builds force asserts ON (REQ-BUILD-012).
if(QUIVER_SANITIZE AND NOT QUIVER_ENABLE_ASSERTS)
  message(STATUS "Quiver: QUIVER_SANITIZE is set; forcing QUIVER_ENABLE_ASSERTS=ON (REQ-BUILD-012).")
  set(QUIVER_ENABLE_ASSERTS ON)
endif()

# --- The library (REQ-BUILD-002: single static target; ADR-002) -------------------------------
# Per-ISA TUs are always listed: each guards itself to its architecture with the preprocessor
# and compiles empty elsewhere (ADR-003; no global ISA flags, REQ-BUILD-005).
set(QUIVER_AVX2_SOURCES
  src/kernels/hash/hash_avx2.cpp
  src/kernels/unpack/unpack_avx2.cpp
  src/kernels/arith/arith_avx2.cpp
  src/kernels/arith_guarded/arith_guarded_avx2.cpp
  src/kernels/compare/compare_avx2.cpp
  src/kernels/filter/filter_avx2.cpp
  src/kernels/select/select_avx2.cpp
  src/kernels/mask/mask_avx2.cpp
  src/kernels/take/take_avx2.cpp
  src/kernels/reduce/reduce_avx2.cpp)
set(QUIVER_NEON_SOURCES
  src/kernels/hash/hash_neon.cpp
  src/kernels/unpack/unpack_neon.cpp
  src/kernels/arith/arith_neon.cpp
  src/kernels/arith_guarded/arith_guarded_neon.cpp
  src/kernels/compare/compare_neon.cpp
  src/kernels/filter/filter_neon.cpp
  src/kernels/select/select_neon.cpp
  src/kernels/mask/mask_neon.cpp
  src/kernels/take/take_neon.cpp
  src/kernels/reduce/reduce_neon.cpp)
add_library(quiver STATIC
  src/cpu/cpu_features.cpp
  src/dispatch/dispatch_tables.cpp
  src/dispatch/version.cpp
  src/kernels/common/luts.cpp
  src/kernels/compare/compare_scalar.cpp
  src/kernels/filter/filter_scalar.cpp
  src/kernels/select/select_scalar.cpp
  src/kernels/mask/mask_scalar.cpp
  src/kernels/take/take_scalar.cpp
  src/kernels/reduce/reduce_scalar.cpp
  src/kernels/hash/hash_scalar.cpp
  src/kernels/unpack/unpack_scalar.cpp
  src/kernels/arith/arith_scalar.cpp
  src/kernels/arith_guarded/arith_guarded_scalar.cpp
  ${QUIVER_AVX2_SOURCES}
  ${QUIVER_NEON_SOURCES})
add_library(quiver::quiver ALIAS quiver)

# MSVC has no per-function target attribute; the AVX2 TUs get /arch:AVX2 per-source instead
# (ADR-003 — the per-TU equivalent of the GCC/Clang target regions; safe because these TUs
# only execute after CPUID gating).
if(MSVC AND CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64")
  set_source_files_properties(${QUIVER_AVX2_SOURCES} PROPERTIES COMPILE_OPTIONS "/arch:AVX2")
endif()

target_include_directories(quiver
  PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(quiver PUBLIC cxx_std_23)

quiver_apply_warnings(quiver)
quiver_apply_library_constraints(quiver)  # -fno-exceptions -fno-rtti (REQ-BUILD-004)
quiver_apply_sanitizers(quiver)

if(QUIVER_ENABLE_ASSERTS)
  target_compile_definitions(quiver PRIVATE QUIVER_ENABLE_ASSERTS=1)
endif()

# --- Dev trees (never installed, never part of the export set: REQ-REPO-007) ------------------
if(QUIVER_ENABLE_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
  enable_testing()
  add_subdirectory(tests)
endif()
if(QUIVER_ENABLE_BENCH AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/bench/CMakeLists.txt")
  add_subdirectory(bench)
endif()
# examples/ arrives at M8 (docs/prd/18-milestones.md).

message(STATUS "Quiver ${PROJECT_VERSION} | tests=${QUIVER_ENABLE_TESTS} bench=${QUIVER_ENABLE_BENCH} "
               "examples=${QUIVER_ENABLE_EXAMPLES} asserts=${QUIVER_ENABLE_ASSERTS} "
               "werror=${QUIVER_ENABLE_WERROR} sanitize='${QUIVER_SANITIZE}' pin='${QUIVER_PIN_ISA}'")
