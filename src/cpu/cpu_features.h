// Internal interface of MOD-CPU: pure, uncached CPU+OS feature detection (REQ-INT-001).
// Module: MOD-CPU | REQs: REQ-INT-001, REQ-DISP-004 | ADRs: ADR-005
#pragma once

#include "quiver/detail/config.h"

QUIVER_BEGIN_NAMESPACE
namespace detail {

// Field semantics (PRD 05 §4): a tier is reported only if the CPU has the instructions AND
// the OS saves the register state (XGETBV on x86). avx512 means the full required set
// F+BW+DQ+VL with opmask/ZMM/Hi16 OS state (REQ-DISP-004); vbmi2/vpopcntdq are optional
// sub-features used at resolution time from M7 (REQ-DISP-011). Monotone by construction:
// avx512 implies avx2.
struct CpuFeatures {
  bool neon;
  bool avx2;
  bool avx512;
  bool avx512vbmi2;
  bool avx512vpopcntdq;
  char brand[64];
};

// Callable concurrently; performs no caching (caching is MOD-DISPATCH's job, REQ-INT-001).
// Unknown platforms return all-false (scalar-only) rather than guessing (PRD 05 §4).
CpuFeatures detect_cpu_features() noexcept;

}  // namespace detail
QUIVER_END_NAMESPACE
