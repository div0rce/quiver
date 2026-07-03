// MOD-DISPATCH + MOD-CPU validation: version introspection, feature monotonicity, override
// round-trip, QUIVER_ISA environment matrix (per-process re-exec), synthetic-entry resolution,
// null-row skipping, warmup idempotence, and the TSan concurrent-resolution protocol test.
// Covers: REQ-DISP-001..010/-012, REQ-INT-001, API-DISP-001..005 (PRD 07 §10/§11)
#include <atomic>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "quiver/detail/config.h"
#include "quiver/dispatch.h"
#include "src/cpu/cpu_features.h"
#include "src/dispatch/dispatch_internal.h"

namespace quiver_test {
extern const char* g_self_path;  // captured by test_main.cpp
}

namespace {

using quiver::Isa;
using quiver::detail::BackendRow;
using quiver::detail::DispatchEntry;
using quiver::detail::KernelFn;

// Marker backends: distinct addresses identify which tier resolution selected.
void backend_scalar() {}
void backend_neon() {}
void backend_avx2() {}
void backend_avx512() {}

BackendRow<KernelFn> full_row() {
  return BackendRow<KernelFn>{{&backend_scalar, &backend_neon, &backend_avx2, &backend_avx512}};
}

KernelFn expected_for(Isa cap, const BackendRow<KernelFn>& row) {
  for (int i = static_cast<int>(cap); i >= 0; --i) {
    if (row.backends[i] != nullptr) {
      return row.backends[i];
    }
  }
  return nullptr;
}

// --- Version introspection (API-DISP-005; M1 acceptance: matches the CMake project version,
// which is itself parsed from config.h) ------------------------------------------------------
TEST(Dispatch, VersionMatchesConfigHeader) {
  const quiver::Version v = quiver::version();
  EXPECT_EQ(v.major, QUIVER_VERSION_MAJOR);
  EXPECT_EQ(v.minor, QUIVER_VERSION_MINOR);
  EXPECT_EQ(v.patch, QUIVER_VERSION_PATCH);
  char expect[32];
  std::snprintf(expect, sizeof(expect), "%d.%d.%d", v.major, v.minor, v.patch);
  EXPECT_STREQ(quiver::version_string(), expect);
}

// --- Feature reporting (REQ-DISP-004, REQ-DISP-012, REQ-INT-001) -----------------------------
TEST(Dispatch, CpuSupportsIsMonotoneAndPlatformSane) {
  EXPECT_TRUE(quiver::cpu_supports(Isa::kScalar));  // always (REQ-DISP-002)
  if (quiver::cpu_supports(Isa::kAvx512)) {
    EXPECT_TRUE(quiver::cpu_supports(Isa::kAvx2));  // monotone (REQ-DISP-004)
  }
#if defined(__aarch64__) || defined(_M_ARM64)
  EXPECT_TRUE(quiver::cpu_supports(Isa::kNeon));  // baseline (REQ-DISP-012)
  EXPECT_FALSE(quiver::cpu_supports(Isa::kAvx2));
  EXPECT_FALSE(quiver::cpu_supports(Isa::kAvx512));
#elif defined(__x86_64__) || defined(_M_X64)
  EXPECT_FALSE(quiver::cpu_supports(Isa::kNeon));  // x86 never reports kNeon (REQ-DISP-002)
#endif
  // Out-of-range enum values are total: false, no UB (PRD 04 §4 invalid-input row).
  EXPECT_FALSE(quiver::cpu_supports(static_cast<Isa>(200)));
}

TEST(Dispatch, DetectionIsPureAndRepeatable) {
  const quiver::detail::CpuFeatures a = quiver::detail::detect_cpu_features();
  const quiver::detail::CpuFeatures b = quiver::detail::detect_cpu_features();
  EXPECT_EQ(a.neon, b.neon);
  EXPECT_EQ(a.avx2, b.avx2);
  EXPECT_EQ(a.avx512, b.avx512);
  EXPECT_FALSE(a.avx512 && !a.avx2);  // monotone at the source too (REQ-DISP-004)
  EXPECT_NE(a.brand[0], '\0');
}

// --- Override round-trip (API-DISP-003, REQ-DISP-006) ----------------------------------------
// --8<-- [start:override]
TEST(Dispatch, OverrideRoundTrip) {
  const Isa hw = quiver::active_isa();
  ASSERT_TRUE(quiver::set_isa_override(Isa::kScalar));  // kScalar always accepted
  EXPECT_EQ(quiver::active_isa(), Isa::kScalar);
  quiver::clear_isa_override();
  EXPECT_EQ(quiver::active_isa(), hw);  // hardware+env policy restored
}
// --8<-- [end:override]

TEST(Dispatch, OverrideRejectsUnsupportedTierWithoutStateChange) {
  const Isa hw = quiver::active_isa();
  // Find a tier this CPU cannot execute (every machine lacks at least one of neon/avx512).
  const Isa impossible = quiver::cpu_supports(Isa::kNeon) ? Isa::kAvx512 : Isa::kNeon;
  if (quiver::cpu_supports(impossible)) {
    GTEST_SKIP() << "no unsupported tier on this host";
  }
  EXPECT_FALSE(quiver::set_isa_override(impossible));
  EXPECT_EQ(quiver::active_isa(), hw);                            // no state change (REQ-DISP-006)
  EXPECT_FALSE(quiver::set_isa_override(static_cast<Isa>(200)));  // out-of-range: false
  EXPECT_EQ(quiver::active_isa(), hw);
}

// --- Synthetic-entry resolution (REQ-DISP-001/-002/-008; PRD 07 §11 framework tests) ---------
TEST(Dispatch, ResolutionSelectsHighestBackendUnderPolicy) {
  DispatchEntry entry;
  const BackendRow<KernelFn> row = full_row();
  const KernelFn fn = quiver::detail::dispatch_get(entry, row);
  EXPECT_EQ(fn, expected_for(quiver::active_isa(), row));
  // Second call takes the hot path and returns the identical pointer (Resolved state).
  EXPECT_EQ(quiver::detail::dispatch_get(entry, row), fn);
}

TEST(Dispatch, ResolutionSkipsNullRowsDownToScalar) {
  DispatchEntry entry;
  BackendRow<KernelFn> row{
      {&backend_scalar, nullptr, nullptr, nullptr}};                     // scalar-only build shape
  EXPECT_EQ(quiver::detail::dispatch_get(entry, row), &backend_scalar);  // REQ 07 §7 null-skip
}

TEST(Dispatch, EpochBumpRetractsResolvedEntries) {
  DispatchEntry entry;
  const BackendRow<KernelFn> row = full_row();
  const KernelFn before = quiver::detail::dispatch_get(entry, row);
  ASSERT_TRUE(quiver::set_isa_override(Isa::kScalar));
  EXPECT_EQ(quiver::detail::dispatch_get(entry, row), &backend_scalar);  // re-resolved
  quiver::clear_isa_override();
  EXPECT_EQ(quiver::detail::dispatch_get(entry, row), before);  // restored (REQ-DISP-006)
}

TEST(Dispatch, WarmupIsIdempotent) {
  quiver::warmup();
  quiver::warmup();  // REQ-DISP-010: idempotent; resolves the (M1: empty) kernel registry
  SUCCEED();
}

// --- Concurrent resolution under override churn (REQ-DISP-008/-009; TSan-validated) ----------
TEST(Dispatch, ConcurrentResolutionIsRaceBenign) {
  constexpr int kThreads = 8;
  constexpr int kIters = 2000;
  DispatchEntry entry;
  const BackendRow<KernelFn> row = full_row();
  std::atomic<bool> failed{false};

  std::vector<std::thread> workers;
  workers.reserve(kThreads);
  for (int t = 0; t < kThreads; ++t) {
    workers.emplace_back([&entry, &row, &failed, t] {
      for (int i = 0; i < kIters; ++i) {
        if (t == 0 && i % 64 == 0) {
          // Override churn from one thread: concurrent calls may run under either policy —
          // both are correct executions (REQ-DISP-009).
          if (i % 128 == 0) {
            (void)quiver::set_isa_override(Isa::kScalar);
          } else {
            quiver::clear_isa_override();
          }
        }
        const KernelFn fn = quiver::detail::dispatch_get(entry, row);
        // Never null after an epoch match; always one of the row's backends.
        if (fn != &backend_scalar && fn != &backend_neon && fn != &backend_avx2 &&
            fn != &backend_avx512) {
          failed.store(true, std::memory_order_relaxed);
        }
      }
    });
  }
  for (std::thread& w : workers) {
    w.join();
  }
  quiver::clear_isa_override();
  EXPECT_FALSE(failed.load());
  EXPECT_EQ(quiver::detail::dispatch_get(entry, row), expected_for(quiver::active_isa(), row));
}

// --- QUIVER_ISA environment matrix (REQ-DISP-005): one child process per case ----------------
#if !defined(_WIN32)
int probe_active_isa_with_env(const std::string& env_assignment) {
  const std::string cmd =
      env_assignment + " \"" + quiver_test::g_self_path + "\" --quiver_env_probe";
  FILE* pipe = ::popen(cmd.c_str(), "r");
  if (pipe == nullptr) {
    return -1;
  }
  int value = -1;
  if (std::fscanf(pipe, "%d", &value) != 1) {
    value = -1;
  }
  ::pclose(pipe);
  return value;
}

TEST(Dispatch, EnvVarMatrix) {
  ASSERT_NE(quiver_test::g_self_path, nullptr);
  const int hw = static_cast<int>(quiver::active_isa());

  // Unset: hardware policy.
  EXPECT_EQ(probe_active_isa_with_env("env -u QUIVER_ISA"), hw);
  // Valid + always-supported: caps to scalar.
  EXPECT_EQ(probe_active_isa_with_env("env QUIVER_ISA=scalar"), static_cast<int>(Isa::kScalar));
  // Unrecognized value (case-sensitive contract): ignored.
  EXPECT_EQ(probe_active_isa_with_env("env QUIVER_ISA=SCALAR"), hw);
  EXPECT_EQ(probe_active_isa_with_env("env QUIVER_ISA=bogus"), hw);
  // Valid-but-unsupported tier on this host: ignored.
  const char* unsupported_case =
      quiver::cpu_supports(Isa::kNeon) ? "env QUIVER_ISA=avx512" : "env QUIVER_ISA=neon";
  if (!quiver::cpu_supports(quiver::cpu_supports(Isa::kNeon) ? Isa::kAvx512 : Isa::kNeon)) {
    EXPECT_EQ(probe_active_isa_with_env(unsupported_case), hw);
  }
  // Valid + supported non-scalar tier caps to itself (identity when it equals hw max).
  if (quiver::cpu_supports(Isa::kNeon)) {
    EXPECT_EQ(probe_active_isa_with_env("env QUIVER_ISA=neon"), static_cast<int>(Isa::kNeon));
  } else if (quiver::cpu_supports(Isa::kAvx2)) {
    EXPECT_EQ(probe_active_isa_with_env("env QUIVER_ISA=avx2"), static_cast<int>(Isa::kAvx2));
  }
}
#endif  // !_WIN32

}  // namespace
