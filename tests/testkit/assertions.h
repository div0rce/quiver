// MOD-TESTKIT failure diagnostics: first-divergence reporting with index, hex context (±4
// elements), the generator seed, and the violated requirement ID — the complete reproduction
// recipe (REQ-TEST-012; format also governs bench validation messages per REQ-ERR-008).
// Module: MOD-TESTKIT | REQs: REQ-TEST-012, REQ-ERR-008 | PRD 05 §7
#pragma once

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

namespace quiver_test {

// What a divergence report needs beyond the buffers: the seed that produced them and the
// contract they are being held to.
struct FailureContext {
  std::uint64_t seed;
  const char* req_id;
};

template <class T>
::testing::AssertionResult buffers_equal(const T* expected, const T* actual, std::int64_t n,
                                         FailureContext ctx) {
  for (std::int64_t i = 0; i < n; ++i) {
    if (std::memcmp(&expected[i], &actual[i], sizeof(T)) != 0) {
      std::ostringstream os;
      os << "first divergence at index " << i << " of " << n << " [" << ctx.req_id << "] seed=0x"
         << std::hex << ctx.seed << std::dec << "\n  idx: expected / actual\n";
      const std::int64_t lo = i >= 4 ? i - 4 : 0;
      const std::int64_t hi = i + 4 < n ? i + 4 : n - 1;
      for (std::int64_t j = lo; j <= hi; ++j) {
        std::uint64_t eb = 0;
        std::uint64_t ab = 0;
        std::memcpy(&eb, &expected[j], sizeof(T));
        std::memcpy(&ab, &actual[j], sizeof(T));
        os << (j == i ? "->" : "  ") << std::setw(6) << j << ": 0x" << std::hex << eb << " / 0x"
           << ab << std::dec << "\n";
      }
      return ::testing::AssertionFailure() << os.str();
    }
  }
  return ::testing::AssertionSuccess();
}

}  // namespace quiver_test
