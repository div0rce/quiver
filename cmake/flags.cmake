# Warning baseline, library constraints, and toolchain minimums.
# Owning module: build system (docs/prd/03-build-system.md).
# REQ-BUILD-004 (exceptions/RTTI off for library TUs), REQ-BUILD-008 (warning baseline),
# toolchain minimums per docs/prd/03-build-system.md §7/§8.

# --- Toolchain minimum enforcement (hard errors with actionable messages, 03 §8) -------------
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 13)
    message(FATAL_ERROR
      "Quiver requires GCC >= 13 (found ${CMAKE_CXX_COMPILER_VERSION}). "
      "See docs/prd/03-build-system.md §7 for the toolchain support matrix.")
  endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 17)
    message(FATAL_ERROR
      "Quiver requires Clang >= 17 (found ${CMAKE_CXX_COMPILER_VERSION}). "
      "See docs/prd/03-build-system.md §7.")
  endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 16)
    message(FATAL_ERROR
      "Quiver requires AppleClang >= 16 / Xcode 16 (found ${CMAKE_CXX_COMPILER_VERSION}). "
      "See docs/prd/03-build-system.md §7.")
  endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  if(MSVC_VERSION LESS 1940)
    message(FATAL_ERROR
      "Quiver tier-2 support requires MSVC >= 19.40 / VS2022 (found ${MSVC_VERSION}). "
      "See docs/prd/03-build-system.md §7.")
  endif()
else()
  message(WARNING
    "Quiver: unrecognized compiler '${CMAKE_CXX_COMPILER_ID}'; only GCC/Clang/AppleClang "
    "(tier-1) and MSVC (tier-2) are supported (docs/prd/03-build-system.md §7).")
endif()

# --- Warning baseline (REQ-BUILD-008) ---------------------------------------------------------
# Applied per-target from M1 onward; fetched third-party code is excluded via SYSTEM includes.
function(quiver_apply_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4)
    if(QUIVER_ENABLE_WERROR)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE
      -Wall -Wextra -Wpedantic -Wshadow -Wvla -Wconversion -Wsign-conversion)
    if(QUIVER_ENABLE_WERROR)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()

# --- Library execution constraints (REQ-BUILD-004, Charter §7.3) ------------------------------
# Shipped-library TUs compile with exceptions and RTTI disabled to enforce the noexcept /
# -fno-exceptions usability contract structurally. Consumers keep their own settings.
function(quiver_apply_library_constraints target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /EHs-c- /GR-)
    target_compile_definitions(${target} PRIVATE _HAS_EXCEPTIONS=0)
  else()
    target_compile_options(${target} PRIVATE -fno-exceptions -fno-rtti)
  endif()
endfunction()
