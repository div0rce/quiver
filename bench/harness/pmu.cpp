// perf_event_open group implementation (Linux) + always-unavailable stub elsewhere.
// Module: MOD-BENCH | REQs: REQ-BENCH-005 | ADR-022
#include "bench/harness/pmu.h"

#if defined(__linux__)

#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <cstring>

namespace quiver::bench {

namespace {

int perf_open(std::uint32_t type, std::uint64_t config, int group_fd) {
  perf_event_attr attr;
  std::memset(&attr, 0, sizeof(attr));
  attr.type = type;
  attr.size = sizeof(attr);
  attr.config = config;
  attr.disabled = (group_fd == -1) ? 1 : 0;
  attr.exclude_kernel = 1;
  attr.exclude_hv = 1;
  attr.read_format = PERF_FORMAT_GROUP;
  return static_cast<int>(
      syscall(SYS_perf_event_open, &attr, 0 /*self*/, -1 /*any cpu*/, group_fd, 0));
}

}  // namespace

PmuGroup::~PmuGroup() {
  for (const int fd : fds_) {
    if (fd >= 0) {
      close(fd);
    }
  }
  if (leader_fd_ >= 0) {
    close(leader_fd_);
  }
}

bool PmuGroup::open() {
  leader_fd_ = perf_open(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES, -1);
  if (leader_fd_ < 0) {
    return false;
  }
  const std::uint64_t configs[3] = {PERF_COUNT_HW_INSTRUCTIONS, PERF_COUNT_HW_BRANCH_INSTRUCTIONS,
                                    PERF_COUNT_HW_BRANCH_MISSES};
  for (int i = 0; i < 3; ++i) {
    fds_[i] = perf_open(PERF_TYPE_HARDWARE, configs[i], leader_fd_);
    if (fds_[i] < 0) {
      // Fail-and-drop: no partial groups, no multiplexing (REQ-BENCH-005).
      for (int j = 0; j < i; ++j) {
        close(fds_[j]);
        fds_[j] = -1;
      }
      close(leader_fd_);
      leader_fd_ = -1;
      return false;
    }
  }
  return true;
}

void PmuGroup::start() {
  if (leader_fd_ < 0) {
    return;
  }
  ioctl(leader_fd_, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
  ioctl(leader_fd_, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
}

PmuCounters PmuGroup::stop_and_read() {
  PmuCounters out;
  if (leader_fd_ < 0) {
    return out;
  }
  ioctl(leader_fd_, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
  // PERF_FORMAT_GROUP layout: nr, then one value per event in group order.
  std::uint64_t buf[1 + 4] = {};
  const ssize_t got = read(leader_fd_, buf, sizeof(buf));
  if (got < static_cast<ssize_t>(sizeof(buf))) {
    return out;
  }
  out.cycles = buf[1];
  out.instructions = buf[2];
  out.branches = buf[3];
  out.branch_misses = buf[4];
  out.valid = true;
  return out;
}

}  // namespace quiver::bench

#else  // non-Linux stub: always unavailable (REQ-BENCH-005 degrade path)

namespace quiver::bench {

PmuGroup::~PmuGroup() = default;
bool PmuGroup::open() {
  return false;
}
void PmuGroup::start() {}
PmuCounters PmuGroup::stop_and_read() {
  return PmuCounters{};
}

}  // namespace quiver::bench

#endif
