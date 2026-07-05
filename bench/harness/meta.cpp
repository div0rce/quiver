// Metadata emission implementation. Git state is captured at configure time by
// bench/CMakeLists.txt into QUIVER_BENCH_GIT_SHA / QUIVER_BENCH_GIT_DIRTY definitions.
// Module: MOD-BENCH | REQs: REQ-BENCH-013, REQ-BUILD-014 (LTO state) | ADR-008
#include "bench/harness/meta.h"

#include <benchmark/benchmark.h>

#include "quiver/dispatch.h"

#ifndef QUIVER_BENCH_GIT_SHA
#define QUIVER_BENCH_GIT_SHA "unknown"
#endif
#ifndef QUIVER_BENCH_GIT_DIRTY
#define QUIVER_BENCH_GIT_DIRTY "unknown"
#endif
#ifndef QUIVER_BENCH_CXX_FLAGS
#define QUIVER_BENCH_CXX_FLAGS "unknown"
#endif
#ifndef QUIVER_BENCH_LTO
#define QUIVER_BENCH_LTO "unknown"
#endif

namespace quiver::bench {

void add_run_context(bool pmu_available) {
  benchmark::AddCustomContext("quiver_version", quiver::version_string());
  benchmark::AddCustomContext("git_sha", QUIVER_BENCH_GIT_SHA);
  benchmark::AddCustomContext("git_dirty", QUIVER_BENCH_GIT_DIRTY);
#if defined(__VERSION__)
  benchmark::AddCustomContext("compiler", __VERSION__);
#else
  benchmark::AddCustomContext("compiler", "unknown");
#endif
  benchmark::AddCustomContext("cxx_flags", QUIVER_BENCH_CXX_FLAGS);
  benchmark::AddCustomContext("lto", QUIVER_BENCH_LTO);
  benchmark::AddCustomContext("pmu", pmu_available ? "available" : "unavailable");
}

}  // namespace quiver::bench
