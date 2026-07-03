// MOD-BENCH metadata emission: git SHA, compiler, and flags recorded into every benchmark
// run's Google Benchmark context so raw JSON is self-describing (REQ-BENCH-013 groundwork;
// full environment manifests are the ledger runner's job from M5, REQ-LEDGER-003).
// Module: MOD-BENCH | REQs: REQ-BENCH-013 | ADR-008
#pragma once

namespace quiver::bench {

// Registers key/value context entries (benchmark::AddCustomContext): quiver_version,
// git_sha, git_dirty, compiler, optimization flags, pmu availability.
void add_run_context(bool pmu_available);

}  // namespace quiver::bench
