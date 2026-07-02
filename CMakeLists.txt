# Quiver root build — M0 configure-only skeleton.
# Owning module: build system (docs/prd/03-build-system.md).
# REQ-BUILD-001..006, -008 (declared), -012; targets arrive at M1 (docs/prd/18-milestones.md).
cmake_minimum_required(VERSION 3.28)

project(Quiver
        VERSION 0.1.0
        DESCRIPTION "Dependency-free C++23 vectorized analytical kernels + cross-ISA performance ledger"
        HOMEPAGE_URL "https://github.com/div0rce/quiver"
        LANGUAGES CXX)

# --- Language floor (REQ-BUILD-001, Charter §7.6) --------------------------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

# --- M0 state: configure-only skeleton -------------------------------------------------------
# The library target (`quiver::quiver`, REQ-BUILD-002), tests, benchmarks, and examples are
# introduced by milestones M1+ per docs/prd/18-milestones.md. This skeleton exists so that the
# option surface, presets (REQ-BUILD-011), and toolchain checks are live and CI-verified from M0.
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src")
  message(STATUS
    "Quiver: M0 configure-only skeleton — no build targets yet "
    "(first targets arrive at milestone M1; see docs/prd/18-milestones.md).")
endif()

message(STATUS "Quiver ${PROJECT_VERSION} | tests=${QUIVER_ENABLE_TESTS} bench=${QUIVER_ENABLE_BENCH} "
               "examples=${QUIVER_ENABLE_EXAMPLES} asserts=${QUIVER_ENABLE_ASSERTS} "
               "werror=${QUIVER_ENABLE_WERROR} sanitize='${QUIVER_SANITIZE}' pin='${QUIVER_PIN_ISA}'")
