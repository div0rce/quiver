// Build configuration macros, version stamp, and assert machinery (public-but-unstable).
// Module: MOD-CORE | REQs: REQ-CORE-001/-003, REQ-ERR-002/-005 | ADRs: ADR-007, ADR-017
#pragma once

#include <cstdio>
#include <cstdlib>

// Version — single source of truth; CMake parses these macros (test: version introspection
// matches the CMake project version, PRD 18 M1 acceptance).
#define QUIVER_VERSION_MAJOR 0
#define QUIVER_VERSION_MINOR 1
#define QUIVER_VERSION_PATCH 0

// One namespace-opening style everywhere (REQ-STD-006; ABI epoch per ADR-007).
#define QUIVER_BEGIN_NAMESPACE                                                                     \
  namespace quiver {                                                                               \
  inline namespace v1 {
#define QUIVER_END_NAMESPACE                                                                       \
  }                                                                                                \
  }

#if defined(_MSC_VER) && !defined(__clang__)
#define QUIVER_FORCE_INLINE __forceinline
#define QUIVER_RESTRICT __restrict
#define QUIVER_ASSUME(x) __assume(x)
#else
#define QUIVER_FORCE_INLINE inline __attribute__((always_inline))
#define QUIVER_RESTRICT __restrict__
#define QUIVER_ASSUME(x)                                                                           \
  do {                                                                                             \
    if (!(x)) {                                                                                    \
      __builtin_unreachable();                                                                     \
    }                                                                                              \
  } while (0)
#endif

QUIVER_BEGIN_NAMESPACE
namespace detail {

// Fixed assert handler (REQ-ERR-005: single stderr emission + abort; no custom hook in v1).
// Message format is contract-tested: "<file>:<line>: assertion: <msg>" (REQ-ERR-002).
[[noreturn]] inline void assert_fail(const char* file, long line, const char* msg) noexcept {
  char buf[512];
  std::snprintf(buf, sizeof(buf), "%s:%ld: assertion: %s\n", file, line, msg);
  std::fputs(buf, stderr);
  std::abort();
}

// Reflects the *library build's* assert setting; used by test suites to self-skip death tests
// under assert-free presets (REQ-TEST-014). Defined in src/dispatch/version.cpp.
bool assertions_enabled() noexcept;

}  // namespace detail
QUIVER_END_NAMESPACE

// QUIVER_ASSERT compiles to nothing unless QUIVER_ENABLE_ASSERTS is defined (REQ-CORE-003).
// Call sites phrase msg as "<api>: <condition> [REQ-or-API-id]" (REQ-ERR-002).
#if defined(QUIVER_ENABLE_ASSERTS)
#define QUIVER_ASSERT(cond, msg)                                                                   \
  do {                                                                                             \
    if (!(cond)) {                                                                                 \
      ::quiver::detail::assert_fail(__FILE__, __LINE__, (msg));                                    \
    }                                                                                              \
  } while (0)
#else
#define QUIVER_ASSERT(cond, msg) ((void)0)
#endif
