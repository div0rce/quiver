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
  unit/test_reduce.cpp
  unit/test_luts.cpp
  unit/test_hash.cpp
  unit/test_unpack.cpp
  unit/test_arith.cpp)

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
# Committed golden vectors resolve from the source tree (REQ-TEST-015).
target_compile_definitions(quiver_tests_unit PRIVATE
  QUIVER_GOLDEN_DIR="${CMAKE_SOURCE_DIR}/tests/golden")

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
  property/prop_mask.cpp property/prop_take.cpp property/prop_reduce.cpp
  property/prop_hash.cpp property/prop_unpack.cpp property/prop_arith.cpp)
quiver_add_test_suite(quiver_tests_differential
  differential/diff_isa_compare.cpp differential/diff_isa_filter.cpp
  differential/diff_isa_select.cpp differential/diff_isa_mask.cpp
  differential/diff_isa_take.cpp differential/diff_isa_reduce.cpp
  differential/diff_isa_hash.cpp differential/diff_isa_unpack.cpp
  differential/diff_isa_arith.cpp)
quiver_add_test_suite(quiver_tests_invariant
  invariant/inv_bitmap_tail.cpp invariant/inv_determinism.cpp
  invariant/inv_selvec_sorted.cpp invariant/inv_noalloc.cpp invariant/inv_guard_pages.cpp)
# Regression suite — activated by its first defect fix per REQ-TEST-011 (empty-SelVec
# façade defect found by differential fuzzing at M4).
quiver_add_test_suite(quiver_tests_regression regression/reg_empty_selvec.cpp)

include(GoogleTest)
gtest_discover_tests(quiver_tests_unit DISCOVERY_TIMEOUT 60)
gtest_discover_tests(quiver_tests_property DISCOVERY_TIMEOUT 60)
gtest_discover_tests(quiver_tests_differential DISCOVERY_TIMEOUT 60 PROPERTIES TIMEOUT 600)
gtest_discover_tests(quiver_tests_invariant DISCOVERY_TIMEOUT 60)
gtest_discover_tests(quiver_tests_regression DISCOVERY_TIMEOUT 60)

# Differential libFuzzer targets (REQ-TEST-007; ADR-009): one per family, Clang-only
# (libFuzzer), always built with fuzzer+ASan+UBSan baked in — independent of QUIVER_SANITIZE,
# which must stay empty for these targets. Run by the CI fuzz-smoke and nightly fuzz jobs
# (REQ-CI-005); corpora live in tests/fuzz/corpus/<family>/.
if(QUIVER_ENABLE_FUZZERS)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "QUIVER_ENABLE_FUZZERS requires Clang (libFuzzer, ADR-009).")
  endif()
  if(NOT QUIVER_SANITIZE MATCHES "address")
    # The LIBRARY must be instrumented, not just the harness TU (REQ-TEST-007/REQ-BUILD-012):
    # sanitizer flags come from QUIVER_SANITIZE so they apply to libquiver uniformly.
    message(FATAL_ERROR "QUIVER_ENABLE_FUZZERS requires QUIVER_SANITIZE=address,undefined "
                        "(fuzzing runs under ASan+UBSan, REQ-TEST-007).")
  endif()
  foreach(fam compare filter select mask take reduce)
    add_executable(quiver_fuzz_${fam} fuzz/fuzz_${fam}.cpp)
    target_link_libraries(quiver_fuzz_${fam} PRIVATE quiver::quiver)
    target_include_directories(quiver_fuzz_${fam} PRIVATE ${CMAKE_SOURCE_DIR})
    target_compile_options(quiver_fuzz_${fam} PRIVATE -fsanitize=fuzzer -Wall -Wextra)
    target_link_options(quiver_fuzz_${fam} PRIVATE -fsanitize=fuzzer)
    if(QUIVER_ENABLE_WERROR)
      target_compile_options(quiver_fuzz_${fam} PRIVATE -Werror)
    endif()
    quiver_apply_sanitizers(quiver_fuzz_${fam})
  endforeach()
endif()

# Drift-alarm conformance runner (REQ-BENCH-015): compiles the bench-local distributions
# source directly — free of BOTH GoogleTest and Google Benchmark (REQ-TEST-018).
add_executable(quiver_drift_check
  testkit/drift_check.cpp
  ${CMAKE_SOURCE_DIR}/bench/harness/distributions.cpp)
target_link_libraries(quiver_drift_check PRIVATE quiver_testkit)
add_test(NAME Testkit.BenchDistributionDriftAlarm COMMAND quiver_drift_check)

# MOD-LEDGER runner unit tests (REQ-INT-004: Python >= 3.11 stdlib only): ADR-020 golden
# statistics + QLS-1 schema accept/reject fixtures (PRD 11 §10).
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_FOUND AND NOT Python3_VERSION VERSION_LESS 3.11)
  add_test(NAME Ledger.StatsGolden
           COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/ledger/runner/tests/test_stats.py)
  add_test(NAME Ledger.SchemaFixtures
           COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/ledger/runner/tests/test_schema.py)
  add_test(NAME Ledger.ResultsValidate
           COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/ledger/runner/quiver_ledger.py
                   validate)
endif()
