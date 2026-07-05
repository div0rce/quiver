# Quiver examples (MOD-EXAMPLES) — dev-only, never installed (REQ-REPO-007). Built as ORDINARY
# consumers: exceptions + RTTI enabled (unlike the library's -fno-exceptions -fno-rtti,
# REQ-BUILD-004) and warning-clean at -Wall -Wextra -Werror — proving consumability under
# normal consumer settings (REQ-INT-006). The dev-preset CI matrix builds and runs all four
# (as example_* ctest cases) on every platform.
foreach(ex 01_minimal_filter 02_filter_take_reduce 03_isa_override 04_nullable_pipeline)
  add_executable(quiver_example_${ex} ${ex}.cpp)
  target_link_libraries(quiver_example_${ex} PRIVATE quiver::quiver)
  if(MSVC)
    target_compile_options(quiver_example_${ex} PRIVATE /W4 /WX)
  else()
    target_compile_options(quiver_example_${ex} PRIVATE -Wall -Wextra -Werror)
  endif()
  # A consumer linking a sanitizer-instrumented libquiver.a must sanitize its own link too,
  # else the sanitizer runtime is never pulled in (undefined __tsan_*/__asan_* at link). No-op
  # when QUIVER_SANITIZE is empty — the dev/release matrix still builds these as plain consumers.
  quiver_apply_sanitizers(quiver_example_${ex})
  set_target_properties(quiver_example_${ex} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
  if(QUIVER_ENABLE_TESTS)  # enable_testing() is active only when tests are built
    add_test(NAME example_${ex} COMMAND quiver_example_${ex})
  endif()
endforeach()
