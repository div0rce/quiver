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
# AVX-512 backends (x86-64 only; empty TUs elsewhere). Incremental rollout at M7 — the list
# grows family by family behind the dispatch slot-[3] markers (src/dispatch/dispatch_tables.cpp).
set(QUIVER_AVX512_SOURCES
  src/kernels/mask/mask_avx512.cpp
  src/kernels/compare/compare_avx512.cpp
  src/kernels/hash/hash_avx512.cpp
  src/kernels/unpack/unpack_avx512.cpp
  src/kernels/arith/arith_avx512.cpp
  src/kernels/filter/filter_avx512.cpp
  src/kernels/select/select_avx512.cpp)

# --- Compile-time ISA pinning (REQ-DISP-013): a pinned build excludes backend TUs above the
# pinned tier so every dispatch entry statically resolves to the pin (scalar always compiles,
# REQ-DISP-001; lower tiers stay as the pin's fallbacks). QUIVER_PIN_ISA_TIER (the Isa enum int:
# scalar 0, neon 1, avx2 2, avx512 3) reaches dispatch_tables.cpp, which nulls the excluded row
# slots to match. Left empty = ordinary runtime dispatch.
set(_quiver_pin_tier "")
if(QUIVER_PIN_ISA STREQUAL "scalar")
  set(_quiver_pin_tier 0)
elseif(QUIVER_PIN_ISA STREQUAL "neon")
  set(_quiver_pin_tier 1)
elseif(QUIVER_PIN_ISA STREQUAL "avx2")
  set(_quiver_pin_tier 2)
elseif(QUIVER_PIN_ISA STREQUAL "avx512")
  set(_quiver_pin_tier 3)
endif()
if(NOT _quiver_pin_tier STREQUAL "")
  if(NOT _quiver_pin_tier EQUAL 1)  # NEON only when pinned to neon (ARM-only, not an x86 fallback)
    set(QUIVER_NEON_SOURCES "")
  endif()
  if(_quiver_pin_tier LESS 2)  # AVX2 stays for avx2 and avx512 pins (AVX-512 fallback)
    set(QUIVER_AVX2_SOURCES "")
  endif()
  if(_quiver_pin_tier LESS 3)  # AVX-512 only when pinned to avx512
    set(QUIVER_AVX512_SOURCES "")
  endif()
endif()

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
  ${QUIVER_AVX512_SOURCES}
  ${QUIVER_NEON_SOURCES})
add_library(quiver::quiver ALIAS quiver)

# MSVC has no per-function target attribute; the AVX2 TUs get /arch:AVX2 per-source instead
# (ADR-003 — the per-TU equivalent of the GCC/Clang target regions; safe because these TUs
# only execute after CPUID gating).
if(MSVC AND CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64")
  set_source_files_properties(${QUIVER_AVX2_SOURCES} PROPERTIES COMPILE_OPTIONS "/arch:AVX2")
  set_source_files_properties(${QUIVER_AVX512_SOURCES} PROPERTIES COMPILE_OPTIONS "/arch:AVX512")
endif()

target_include_directories(quiver
  PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
          $<INSTALL_INTERFACE:include>
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(quiver PUBLIC cxx_std_23)
if(NOT _quiver_pin_tier STREQUAL "")
  # PUBLIC so the in-tree tests (which link quiver::quiver) can skip the runtime-dispatch-policy
  # cases that a pinned build fixes. Inert for consumers — no public header references it.
  target_compile_definitions(quiver PUBLIC QUIVER_PIN_ISA_TIER=${_quiver_pin_tier})
endif()

quiver_apply_warnings(quiver)
quiver_apply_library_constraints(quiver)  # -fno-exceptions -fno-rtti (REQ-BUILD-004)
quiver_apply_sanitizers(quiver)

if(QUIVER_ENABLE_ASSERTS)
  target_compile_definitions(quiver PRIVATE QUIVER_ENABLE_ASSERTS=1)
endif()

# Test-only detection seam (QUIVER_TEST_SEAMS): lets test builds force the detected ISA via
# QUIVER_TEST_FORCE_ISA so hosts whose CPUID lacks AVX-512 but whose emulator (QEMU) executes
# the instructions can still exercise the AVX-512 dispatch path locally. Compiled out of any
# build with tests disabled (release/install), so shipped detection stays pure CPUID (PRD 07).
if(QUIVER_ENABLE_TESTS)
  target_compile_definitions(quiver PRIVATE QUIVER_TEST_SEAMS=1)
endif()

# --- Amalgamation (MOD-AMALG; ADR-018, REQ-BUILD-013): generate the single-file drop-in pair
# quiver.h + quiver.cpp with the rule-based generator; the verify target (tests/) compiles the
# pair and runs the unit suite against it, requiring byte-identical kernel outputs. Top-level
# only, needs Python 3; the generated pair is a build artifact, never committed (REQ-REPO-011).
option(QUIVER_ENABLE_AMALGAMATION "Enable the amalgamation generate/verify targets"
       ${QUIVER_ENABLE_TESTS})
if(PROJECT_IS_TOP_LEVEL AND QUIVER_ENABLE_AMALGAMATION)
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_Interpreter_FOUND)
    set(QUIVER_AMALG_DIR "${CMAKE_BINARY_DIR}/amalgamation")
    set(QUIVER_AMALG_H "${QUIVER_AMALG_DIR}/quiver.h")
    set(QUIVER_AMALG_CPP "${QUIVER_AMALG_DIR}/quiver.cpp")
    file(GLOB_RECURSE _quiver_amalg_inputs CONFIGURE_DEPENDS
      "${CMAKE_CURRENT_SOURCE_DIR}/include/quiver/*.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
    add_custom_command(
      OUTPUT "${QUIVER_AMALG_H}" "${QUIVER_AMALG_CPP}"
      COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/amalgamate/amalgamate.py"
              --out-dir "${QUIVER_AMALG_DIR}"
      DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/amalgamate/amalgamate.py" ${_quiver_amalg_inputs}
      COMMENT "Generating single-file amalgamation (ADR-018)"
      VERBATIM)
    add_custom_target(quiver_amalgamate DEPENDS "${QUIVER_AMALG_H}" "${QUIVER_AMALG_CPP}")
  else()
    message(STATUS "Quiver: Python3 not found — amalgamation targets disabled")
  endif()
endif()

# --- Dev trees (never installed, never part of the export set: REQ-REPO-007) ------------------
if(QUIVER_ENABLE_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
  enable_testing()
  add_subdirectory(tests)
endif()
if(QUIVER_ENABLE_BENCH AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/bench/CMakeLists.txt")
  add_subdirectory(bench)
endif()
if(QUIVER_ENABLE_EXAMPLES AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/CMakeLists.txt")
  add_subdirectory(examples)
endif()

# --- Install / export (REQ-BUILD-009): exactly headers + the static library + the CMake
# package (find_package(Quiver CONFIG) → quiver::quiver). Nothing else (REQ-REPO-007).
# Top-level only: as a subproject (FetchContent/add_subdirectory) consumers use the target
# directly and must not trigger our install rules.
if(PROJECT_IS_TOP_LEVEL)
  include(GNUInstallDirs)
  include(CMakePackageConfigHelpers)
  set(_quiver_cmakedir ${CMAKE_INSTALL_LIBDIR}/cmake/Quiver)
  install(TARGETS quiver EXPORT QuiverTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
  install(DIRECTORY include/quiver DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
  install(EXPORT QuiverTargets FILE QuiverTargets.cmake NAMESPACE quiver::
          DESTINATION ${_quiver_cmakedir})
  configure_package_config_file(cmake/QuiverConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/QuiverConfig.cmake" INSTALL_DESTINATION ${_quiver_cmakedir})
  write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/QuiverConfigVersion.cmake"
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/QuiverConfig.cmake"
                "${CMAKE_CURRENT_BINARY_DIR}/QuiverConfigVersion.cmake"
          DESTINATION ${_quiver_cmakedir})
endif()

message(STATUS "Quiver ${PROJECT_VERSION} | tests=${QUIVER_ENABLE_TESTS} bench=${QUIVER_ENABLE_BENCH} "
               "examples=${QUIVER_ENABLE_EXAMPLES} asserts=${QUIVER_ENABLE_ASSERTS} "
               "werror=${QUIVER_ENABLE_WERROR} sanitize='${QUIVER_SANITIZE}' pin='${QUIVER_PIN_ISA}'")
