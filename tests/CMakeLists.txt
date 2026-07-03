# Quiver test suites — dev-only, never installed (REQ-BUILD-007, REQ-REPO-007).
# Suites per PRD 12 REQ-TEST-001; M1 populates unit/ (test_core, test_dispatch).

quiver_fetch_dev_dependencies()

add_executable(quiver_tests_unit
  unit/test_main.cpp
  unit/test_core.cpp
  unit/test_dispatch.cpp)

target_link_libraries(quiver_tests_unit PRIVATE quiver::quiver GTest::gtest)
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

include(GoogleTest)
gtest_discover_tests(quiver_tests_unit DISCOVERY_TIMEOUT 60)
