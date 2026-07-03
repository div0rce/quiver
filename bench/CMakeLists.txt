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
target_compile_definitions(quiver_bench_harness PRIVATE
  QUIVER_BENCH_GIT_SHA="${_quiver_git_sha}"
  QUIVER_BENCH_GIT_DIRTY="${_quiver_git_dirty}"
  QUIVER_BENCH_CXX_FLAGS="${CMAKE_CXX_FLAGS} [${CMAKE_BUILD_TYPE}]")

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

# Family microbenchmarks (bench/baselines/ arrives M4; bench/pipeline/ at M8).
foreach(fam compare filter select mask take reduce dispatch)
  add_executable(quiver_bench_${fam} micro/bench_${fam}.cpp)
  target_link_libraries(quiver_bench_${fam} PRIVATE quiver_bench_harness)
  set_target_properties(quiver_bench_${fam} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endforeach()
