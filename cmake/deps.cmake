# Pinned development-only dependencies — REQ-BUILD-007 (docs/prd/03-build-system.md).
# The shipped library has ZERO dependencies (REQ-BUILD-003, Charter T4). Everything here is
# dev-only, fetched exclusively when the corresponding option is ON, never installed, and never
# part of the export set. Pins are exact release tags with URL SHA-256 hashes.
#
# Pin registry (update = deliberate PR touching this file + CHANGELOG; risk R-10):
#   GTEST_PIN  : googletest v1.15.2
#   GBENCH_PIN : benchmark  v1.9.1
# Hashes computed from the canonical GitHub archive tarballs (recorded in
# docs/releases/gates/M0.md).

include(FetchContent)

set(QUIVER_GTEST_PIN_TAG    "v1.15.2")
set(QUIVER_GTEST_PIN_URL    "https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz")
set(QUIVER_GTEST_PIN_SHA256 "7b42b4d6ed48810c5362c265a17faebe90dc2373c885e5216439d37927f02926")

set(QUIVER_GBENCH_PIN_TAG    "v1.9.1")
set(QUIVER_GBENCH_PIN_URL    "https://github.com/google/benchmark/archive/refs/tags/v1.9.1.tar.gz")
set(QUIVER_GBENCH_PIN_SHA256 "32131c08ee31eeff2c8968d7e874f3cb648034377dfc32a4c377fa8796d84981")

# Invoked from the tests/bench subdirectories starting at M1/M2 (docs/prd/18-milestones.md).
# At M0 this function exists but is never called: configure stays offline (REQ-BUILD-007).
function(quiver_fetch_dev_dependencies)
  if(QUIVER_ENABLE_TESTS)
    FetchContent_Declare(googletest
      URL      ${QUIVER_GTEST_PIN_URL}
      URL_HASH SHA256=${QUIVER_GTEST_PIN_SHA256}
      SYSTEM)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)  # never installed (REQ-BUILD-007)
    FetchContent_MakeAvailable(googletest)
  endif()

  if(QUIVER_ENABLE_BENCH)
    FetchContent_Declare(benchmark
      URL      ${QUIVER_GBENCH_PIN_URL}
      URL_HASH SHA256=${QUIVER_GBENCH_PIN_SHA256}
      SYSTEM)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(benchmark)
  endif()
endfunction()
