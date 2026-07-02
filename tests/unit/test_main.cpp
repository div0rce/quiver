// Custom test entry point: supports the --quiver_env_probe re-exec mode used by the
// QUIVER_ISA environment-matrix tests (REQ-DISP-005 validation needs one process per value).
// Module: test support | REQs: REQ-DISP-005 (env matrix), REQ-TEST-012
#include <cstdio>
#include <cstring>

#include <gtest/gtest.h>

#include "quiver/dispatch.h"

namespace quiver_test {
// Absolute-enough path of this binary (as invoked), captured for re-exec.
const char* g_self_path = nullptr;
}  // namespace quiver_test

int main(int argc, char** argv) {
  quiver_test::g_self_path = argv[0];
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--quiver_env_probe") == 0) {
      // Child mode: print the active ISA (after the once-per-process QUIVER_ISA read) and exit.
      std::printf("%d\n", static_cast<int>(quiver::active_isa()));
      return 0;
    }
  }
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
