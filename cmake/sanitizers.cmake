# Sanitizer configuration — REQ-BUILD-012 (docs/prd/03-build-system.md).
# QUIVER_SANITIZE is a semicolon list drawn from: address, undefined, thread, memory.
# Flags apply uniformly to the library, tests, and fuzz targets via quiver_apply_sanitizers().

set(_quiver_san_allowed address undefined thread memory)

# --- Validation (configure-time hard errors, 03 §8) -------------------------------------------
foreach(_san IN LISTS QUIVER_SANITIZE)
  if(NOT _san IN_LIST _quiver_san_allowed)
    message(FATAL_ERROR
      "QUIVER_SANITIZE contains unknown sanitizer '${_san}'; allowed: "
      "address;undefined;thread;memory (REQ-BUILD-012).")
  endif()
endforeach()

if("thread" IN_LIST QUIVER_SANITIZE AND "address" IN_LIST QUIVER_SANITIZE)
  message(FATAL_ERROR
    "QUIVER_SANITIZE: 'thread' and 'address' are mutually exclusive (docs/prd/03-build-system.md §8).")
endif()
if("thread" IN_LIST QUIVER_SANITIZE AND "memory" IN_LIST QUIVER_SANITIZE)
  message(FATAL_ERROR "QUIVER_SANITIZE: 'thread' and 'memory' are mutually exclusive.")
endif()
if("address" IN_LIST QUIVER_SANITIZE AND "memory" IN_LIST QUIVER_SANITIZE)
  message(FATAL_ERROR "QUIVER_SANITIZE: 'address' and 'memory' are mutually exclusive.")
endif()

if(QUIVER_SANITIZE AND MSVC)
  message(FATAL_ERROR
    "QUIVER_SANITIZE is not supported with MSVC (tier-2); use a tier-1 toolchain "
    "(docs/prd/03-build-system.md §8).")
endif()

if("memory" IN_LIST QUIVER_SANITIZE AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  message(FATAL_ERROR
    "MemorySanitizer requires Clang (docs/prd/12-testing-architecture.md REQ-TEST-009).")
endif()

# MSan reports every read of memory written by UNINSTRUMENTED code as uninitialized, so the
# whole C++ standard library must be instrumented too. Against a stock libstdc++ the build
# links fine and then fails inside std::string/memchr during GoogleTest's test discovery —
# a false positive that looks exactly like a real defect and has cost real debugging time.
# CI's nightly MSan leg supplies an instrumented libc++ (see risk R-19). The heuristic below
# only detects whether libc++ was SELECTED; it cannot verify the runtime is instrumented, so
# it is a signpost, not a guarantee — the warning text says so.
if("memory" IN_LIST QUIVER_SANITIZE
   AND NOT CMAKE_CXX_FLAGS MATCHES "stdlib=libc\\+\\+"
   AND NOT "$ENV{CXXFLAGS}" MATCHES "stdlib=libc\\+\\+")
  message(WARNING
    "QUIVER_SANITIZE=memory without -stdlib=libc++: MemorySanitizer needs an MSan-INSTRUMENTED "
    "standard library. Against stock libstdc++ the suite fails with "
    "'use-of-uninitialized-value' inside memchr/std::string during test discovery — those are "
    "false positives, not Quiver defects. Build an instrumented libc++ and pass "
    "-stdlib=libc++ (plus its include/lib paths) via CXXFLAGS/LDFLAGS, as the nightly MSan job "
    "does (docs/prd/12-testing-architecture.md REQ-TEST-009). NOTE: this check only looks for "
    "-stdlib=libc++ — it cannot tell an instrumented libc++ from a stock one, so selecting "
    "libc++ silences the warning without guaranteeing the runtime is instrumented.")
endif()

# --- Application (used by all instrumented targets from M1 onward) -----------------------------
function(quiver_apply_sanitizers target)
  if(NOT QUIVER_SANITIZE)
    return()
  endif()
  set(_flags "")
  foreach(_san IN LISTS QUIVER_SANITIZE)
    list(APPEND _flags "-fsanitize=${_san}")
  endforeach()
  list(APPEND _flags -fno-omit-frame-pointer -fno-sanitize-recover=all)
  if("memory" IN_LIST QUIVER_SANITIZE)
    list(APPEND _flags -fsanitize-memory-track-origins)
  endif()
  target_compile_options(${target} PRIVATE ${_flags})
  target_link_options(${target} PRIVATE ${_flags})
endfunction()
