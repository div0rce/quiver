// Version introspection (API-DISP-005) and the library-build assert-state probe used by test
// suites to self-skip death tests (REQ-TEST-014).
// Module: MOD-DISPATCH | REQs: REQ-API-002, REQ-CORE-003 | ADRs: ADR-017
#include "quiver/detail/config.h"
#include "quiver/dispatch.h"

#define QUIVER_STRINGIZE_IMPL(x) #x
#define QUIVER_STRINGIZE(x) QUIVER_STRINGIZE_IMPL(x)

QUIVER_BEGIN_NAMESPACE

Version version() noexcept {
  return Version{QUIVER_VERSION_MAJOR, QUIVER_VERSION_MINOR, QUIVER_VERSION_PATCH};
}

const char* version_string() noexcept {
  // Static storage duration, never freed (API-DISP-005).
  return QUIVER_STRINGIZE(QUIVER_VERSION_MAJOR) "." QUIVER_STRINGIZE(
      QUIVER_VERSION_MINOR) "." QUIVER_STRINGIZE(QUIVER_VERSION_PATCH);
}

namespace detail {

bool assertions_enabled() noexcept {
#if defined(QUIVER_ENABLE_ASSERTS)
  return true;
#else
  return false;
#endif
}

}  // namespace detail
QUIVER_END_NAMESPACE
