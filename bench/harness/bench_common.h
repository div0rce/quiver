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
