# Quiver benchmark tree — dev-only, never installed (REQ-REPO-007); never depends on test
# code or GoogleTest (REQ-BENCH-015 / REQ-TEST-018).
# Module: MOD-BENCH | REQs: REQ-BENCH-001..005 | ADR-008, ADR-022

quiver_fetch_dev_dependencies()  # provides benchmark::benchmark under QUIVER_ENABLE_BENCH

# Git state captured at configure time for run-context metadata (REQ-BENCH-013 groundwork).
find_package(Git QUIET)
set(_quiver_git_sha "unknown")
set(_quiver_git_dirty "unknown")
if(Git_FOUND)
  execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short=12 HEAD
                  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                  OUTPUT_VARIABLE _quiver_git_sha OUTPUT_STRIP_TRAILING_WHITESPACE
                  ERROR_QUIET)
  execute_process(COMMAND ${GIT_EXECUTABLE} status --porcelain
                  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                  OUTPUT_VARIABLE _quiver_git_status ERROR_QUIET)
  if(_quiver_git_status STREQUAL "")
    set(_quiver_git_dirty "clean")
  else()
    set(_quiver_git_dirty "dirty")
  endif()
endif()

add_library(quiver_bench_harness STATIC
  harness/distributions.cpp
  harness/pmu.cpp
  harness/meta.cpp)
target_link_libraries(quiver_bench_harness PUBLIC quiver::quiver benchmark::benchmark)
target_include_directories(quiver_bench_harness PUBLIC ${CMAKE_SOURCE_DIR})
# LTO state for the ledger manifest (REQ-BUILD-014, REQ-LEDGER-003): the standard
# CMAKE_INTERPROCEDURAL_OPTIMIZATION variable controls it and is OFF unless set.
if(CMAKE_INTERPROCEDURAL_OPTIMIZATION)
  set(_quiver_lto "on")
else()
  set(_quiver_lto "off")
endif()
target_compile_definitions(quiver_bench_harness PRIVATE
  QUIVER_BENCH_GIT_SHA="${_quiver_git_sha}"
  QUIVER_BENCH_GIT_DIRTY="${_quiver_git_dirty}"
  QUIVER_BENCH_CXX_FLAGS="${CMAKE_CXX_FLAGS} [${CMAKE_BUILD_TYPE}]"
  QUIVER_BENCH_LTO="${_quiver_lto}")

# Third-party headers exempt from the warning policy (REQ-BUILD-008).
get_target_property(_quiver_gb_incs benchmark::benchmark INTERFACE_INCLUDE_DIRECTORIES)
if(_quiver_gb_incs)
  target_include_directories(quiver_bench_harness SYSTEM PUBLIC ${_quiver_gb_incs})
endif()

if(MSVC)
  target_compile_options(quiver_bench_harness PRIVATE /W4)
else()
  target_compile_options(quiver_bench_harness PRIVATE -Wall -Wextra)
endif()

add_executable(quiver_bench_smoke harness/bench_smoke.cpp)
target_link_libraries(quiver_bench_smoke PRIVATE quiver_bench_harness)
set_target_properties(quiver_bench_smoke PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Equal-ISA autovec baselines (ADR-011, REQ-BENCH-010): the reference recompiled under the
# AVX2 target region (compiles empty off x86, ADR-003); the NEON baseline is an identity —
# baseline_neon.cpp documents why and exports nothing.
add_library(quiver_bench_baselines STATIC baselines/baseline_avx2.cpp baselines/baseline_neon.cpp)
target_link_libraries(quiver_bench_baselines PUBLIC quiver::quiver)
target_include_directories(quiver_bench_baselines PUBLIC ${CMAKE_SOURCE_DIR})
if(NOT MSVC)
  target_compile_options(quiver_bench_baselines PRIVATE -Wall -Wextra)
endif()
if(MSVC AND CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64")
  # MSVC has no target pragma; the baseline TU gets /arch:AVX2 per-source (ADR-003).
  set_source_files_properties(baselines/baseline_avx2.cpp PROPERTIES COMPILE_OPTIONS "/arch:AVX2")
endif()

# Family microbenchmarks (bench/pipeline/ arrives at M8).
foreach(fam compare filter select mask take reduce dispatch hash unpack arith arith_guarded)
  add_executable(quiver_bench_${fam} micro/bench_${fam}.cpp)
  target_link_libraries(quiver_bench_${fam} PRIVATE quiver_bench_harness quiver_bench_baselines)
  set_target_properties(quiver_bench_${fam} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endforeach()
