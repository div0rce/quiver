// MOD-BENCH first-party PMU wrapper (ADR-022): one non-multiplexed perf_event group of
// {cycles, instructions, branches, branch-misses} via perf_event_open. Fail-and-drop policy:
// if any event in the group cannot be opened, the whole group is dropped and benchmarks
// proceed without counters, marking their output (REQ-BENCH-005) — never a failure.
// Linux-only; other platforms compile the always-unavailable stub (Apple entries ship
// without PMU columns, Charter §6.4 / REQ-LEDGER-008).
// Module: MOD-BENCH | REQs: REQ-BENCH-005 | ADR-022
#pragma once

#include <cstdint>

namespace quiver::bench {

struct PmuCounters {
  std::uint64_t cycles = 0;
  std::uint64_t instructions = 0;
  std::uint64_t branches = 0;
  std::uint64_t branch_misses = 0;
  bool valid = false;
};

class PmuGroup {
public:
  PmuGroup() = default;
  ~PmuGroup();
  PmuGroup(const PmuGroup&) = delete;
  PmuGroup& operator=(const PmuGroup&) = delete;

  // Opens the counter group for the calling thread. Returns false (and stays unavailable)
  // on non-Linux platforms, permission failure (perf_event_paranoid), or virtualized PMUs.
  bool open();
  bool available() const { return leader_fd_ >= 0; }

  void start();                 // reset + enable (no-op when unavailable)
  PmuCounters stop_and_read();  // disable + read; .valid=false when unavailable

private:
  int leader_fd_ = -1;
  [[maybe_unused]] int fds_[3] = {-1, -1, -1};  // unused by the non-Linux stub
};

}  // namespace quiver::bench
