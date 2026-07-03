// MOD-BENCH common helpers: the REQ-BENCH-002 naming convention, pre-timing validation with
// hard abort (REQ-BENCH-004 / REQ-INT-003 — a benchmark that fails validation must not emit
// timing output), and forced-variant execution (REQ-BENCH-006).
// Module: MOD-BENCH | REQs: REQ-BENCH-002/-004/-006 | ADR-008, ADR-011
#pragma once

#include <cstdio>
#include <cstdlib>
#include <string>

#include "quiver/dispatch.h"

namespace quiver::bench {

// Benchmark names are the ledger's join key: BM_<family>/<api>/<variant>/<type>/<k>=<v>/...
// (REQ-BENCH-002; renames are a ledger-schema event).
inline std::string bench_name(const char* family, const char* api, const char* variant,
                              const char* type, const std::string& axes) {
  std::string s = "BM_";
  s += family;
  s += '/';
  s += api;
  s += '/';
  s += variant;
  s += '/';
  s += type;
  if (!axes.empty()) {
    s += '/';
    s += axes;
  }
  return s;
}

// Variant name for the ACTIVE tier (REQ-BENCH-002/-010): the vocabulary is platform-
// dependent — on ARM the portable scalar build IS the NEON-baseline autovec variant and is
// reported as `autovec` (no separate `scalar` variant exists there: it would be the same
// binary code); on x86 the baseline build is reported once, as `scalar`. The ledger runner
// selects tiers per repetition process via the QUIVER_ISA env cap, so this single mapping
// covers every variant a bench binary can register.
inline const char* variant_name(quiver::Isa isa) {
#if defined(__aarch64__) || defined(_M_ARM64)
  constexpr const char* kBaseline = "autovec";
#else
  constexpr const char* kBaseline = "scalar";
#endif
  switch (isa) {
  case quiver::Isa::kScalar:
    return kBaseline;
  case quiver::Isa::kNeon:
    return "neon";
  case quiver::Isa::kAvx2:
    return "avx2";
  case quiver::Isa::kAvx512:
    return "avx512";
  }
  return kBaseline;
}

// Validation failure is fatal by contract: diagnostic to stderr, then abort, so no timing
// output can be emitted for incorrect code (REQ-BENCH-004; format per REQ-ERR-008).
inline void validate_or_abort(const char* bench, bool ok, const char* detail) {
  if (!ok) {
    std::fprintf(stderr,
                 "%s: pre-timing validation FAILED: %s [REQ-BENCH-004] — aborting; no timing "
                 "output is valid for incorrect code\n",
                 bench, detail);
    std::abort();
  }
}

// Force a specific ISA variant for the process (REQ-BENCH-006): override + warmup + verify.
// Returns false when the host cannot execute the tier — the caller skips with a reason.
inline bool force_variant(quiver::Isa isa) {
  if (!quiver::set_isa_override(isa)) {
    return false;
  }
  quiver::warmup();
  return quiver::active_isa() == isa;
}

}  // namespace quiver::bench
