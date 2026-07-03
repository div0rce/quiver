# Quiver test suites — dev-only, never installed (REQ-BUILD-007, REQ-REPO-007).
# Suites per PRD 12 REQ-TEST-001; M1 populates unit/ (test_core, test_dispatch).

quiver_fetch_dev_dependencies()

# MOD-TESTKIT: seeded generators + oracles (PRD 05 §7). GoogleTest-free core so the drift
# check (REQ-BENCH-015) can link it without violating REQ-TEST-018.
add_library(quiver_testkit STATIC testkit/generators.cpp)
target_link_libraries(quiver_testkit PUBLIC quiver::quiver)
target_include_directories(quiver_testkit PUBLIC ${CMAKE_SOURCE_DIR})
if(NOT MSVC)
  target_compile_options(quiver_testkit PRIVATE -Wall -Wextra)
endif()

add_executable(quiver_tests_unit
  unit/test_main.cpp
  unit/test_core.cpp
  unit/test_dispatch.cpp
  unit/test_testkit.cpp
  unit/test_compare.cpp
  unit/test_filter.cpp
  unit/test_select.cpp
  unit/test_mask.cpp
  unit/test_take.cpp
  unit/test_reduce.cpp)

target_link_libraries(quiver_tests_unit PRIVATE quiver::quiver quiver_testkit GTest::gtest)
# Tests may include internal headers ("src/..." form, REQ-STD-006); consumers may not.
target_include_directories(quiver_tests_unit PRIVATE ${CMAKE_SOURCE_DIR})
# Third-party headers are exempt from the warning policy (REQ-BUILD-008): force SYSTEM
# treatment of GoogleTest includes so newer-compiler warnings in gtest never trip -Werror.
get_target_property(_quiver_gtest_incs GTest::gtest INTERFACE_INCLUDE_DIRECTORIES)
if(_quiver_gtest_incs)
  target_include_directories(quiver_tests_unit SYSTEM PRIVATE ${_quiver_gtest_incs})
endif()

if(QUIVER_ENABLE_ASSERTS)
  target_compile_definitions(quiver_tests_unit PRIVATE QUIVER_ENABLE_ASSERTS=1)
endif()

# Test code gets the core warning set; the full library baseline (REQ-BUILD-008) applies to
# shipped TUs, where -Wconversion strictness pays for itself.
if(MSVC)
  target_compile_options(quiver_tests_unit PRIVATE /W4)
else()
  target_compile_options(quiver_tests_unit PRIVATE -Wall -Wextra)
  if(QUIVER_ENABLE_WERROR)
    target_compile_options(quiver_tests_unit PRIVATE -Werror)
  endif()
endif()

quiver_apply_sanitizers(quiver_tests_unit)

# Suite executables per PRD 03 §4 (property/differential/invariant; regression activates
# with its first defect fix, REQ-TEST-011).
function(quiver_add_test_suite name)
  add_executable(${name} ${ARGN})
  target_link_libraries(${name} PRIVATE quiver::quiver quiver_testkit GTest::gtest GTest::gtest_main)
  target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR})
  if(_quiver_gtest_incs)
    target_include_directories(${name} SYSTEM PRIVATE ${_quiver_gtest_incs})
  endif()
  if(QUIVER_ENABLE_ASSERTS)
    target_compile_definitions(${name} PRIVATE QUIVER_ENABLE_ASSERTS=1)
  endif()
  if(NOT MSVC)
    target_compile_options(${name} PRIVATE -Wall -Wextra)
    if(QUIVER_ENABLE_WERROR)
      target_compile_options(${name} PRIVATE -Werror)
    endif()
  else()
    target_compile_options(${name} PRIVATE /W4)
  endif()
  quiver_apply_sanitizers(${name})
endfunction()

quiver_add_test_suite(quiver_tests_property
  property/prop_compare.cpp property/prop_filter.cpp property/prop_select.cpp
  property/prop_mask.cpp property/prop_take.cpp property/prop_reduce.cpp)
quiver_add_test_suite(quiver_tests_differential
  differential/diff_isa_compare.cpp differential/diff_isa_filter.cpp
  differential/diff_isa_select.cpp differential/diff_isa_mask.cpp
  differential/diff_isa_take.cpp differential/diff_isa_reduce.cpp)
quiver_add_test_suite(quiver_tests_invariant
  invariant/inv_bitmap_tail.cpp invariant/inv_determinism.cpp
  invariant/inv_selvec_sorted.cpp invariant/inv_noalloc.cpp invariant/inv_guard_pages.cpp)

include(GoogleTest)
gtest_discover_tests(quiver_tests_unit DISCOVERY_TIMEOUT 60)
gtest_discover_tests(quiver_tests_property DISCOVERY_TIMEOUT 60)
gtest_discover_tests(quiver_tests_differential DISCOVERY_TIMEOUT 60 PROPERTIES TIMEOUT 600)
gtest_discover_tests(quiver_tests_invariant DISCOVERY_TIMEOUT 60)

# Drift-alarm conformance runner (REQ-BENCH-015): compiles the bench-local distributions
# source directly — free of BOTH GoogleTest and Google Benchmark (REQ-TEST-018).
add_executable(quiver_drift_check
  testkit/drift_check.cpp
  ${CMAKE_SOURCE_DIR}/bench/harness/distributions.cpp)
target_link_libraries(quiver_drift_check PRIVATE quiver_testkit)
add_test(NAME Testkit.BenchDistributionDriftAlarm COMMAND quiver_drift_check)
