// Internal dispatch machinery: entry/row types and the resolution protocol of PRD 07 §4–§6.
// Module: MOD-DISPATCH | REQs: REQ-DISP-001/-002/-003/-007/-008/-009 | ADRs: ADR-004
#pragma once

#include <atomic>
#include <cstdint>

#include "quiver/core.h"
#include "quiver/detail/config.h"

QUIVER_BEGIN_NAMESPACE
namespace detail {

// Kernel function pointers are stored type-erased; each call site casts back to the concrete
// signature it declared in extern_decls.h (well-defined round-trip for function pointers).
using KernelFn = void (*)();

// One entry per concrete kernel symbol (REQ-DISP-007). States: Unresolved (fn_epoch == 0,
// policy epochs start at 1) -> Resolved(epoch E) -> Stale on epoch bump -> re-Resolved.
struct DispatchEntry {
  std::atomic<KernelFn> fn{nullptr};
  std::atomic<std::uint32_t> fn_epoch{0};
};

// Backend table row, indexed by Isa; typed by the entry's own function-pointer type so rows
// are constinit-initializable with no casts (REQ-CORE-004: reinterpret_cast is not constexpr;
// erasure happens at runtime inside resolve/dispatch_get, where the round-trip is well
// defined). backends[Isa::kScalar] shall never be null (REQ-DISP-001); null rows are skipped
// at resolution — the single mechanism for partial builds (PRD 07 §7).
template <class Fn>
struct BackendRow {
  Fn backends[4];
};

// Current policy epoch (relaxed read; monotonically increasing, starts at 1).
std::uint32_t current_policy_epoch() noexcept;

// Effective policy cap: highest hardware-supported tier, capped by the once-read QUIVER_ISA
// environment value and any programmatic override (REQ-DISP-002/-005/-006).
Isa current_policy_cap() noexcept;

// Slow path: select the highest non-null backend <= policy cap, publish {fn (relaxed store),
// then fn_epoch (release store)} and return it. Idempotent and race-benign: concurrent
// resolvers may interleave, but any epoch-matched read observes a non-null backend valid
// under the current or a newer policy (REQ-DISP-008/-009; memory-order argument in
// docs/internals/dispatch-state-machine.md).
template <class Fn>
Fn resolve(DispatchEntry& entry, const BackendRow<Fn>& row) noexcept {
  const std::uint32_t now = current_policy_epoch();
  const Isa cap = current_policy_cap();
  Fn fn = nullptr;
  for (int i = static_cast<int>(cap); i >= 0; --i) {
    if (row.backends[i] != nullptr) {
      fn = row.backends[i];
      break;
    }
  }
  QUIVER_ASSERT(fn != nullptr, "dispatch: scalar backend row must be non-null [REQ-DISP-001]");
  // Publication order is the protocol (REQ-DISP-008): fn first (relaxed), epoch second
  // (release). dispatch_get()'s acquire on fn_epoch pairs with this release.
  entry.fn.store(reinterpret_cast<KernelFn>(fn), std::memory_order_relaxed);
  entry.fn_epoch.store(now, std::memory_order_release);
  return fn;
}

// Hot path (REQ-DISP-003): <= 3 atomic loads (one acquire, two relaxed) + 1 predictable
// branch + 1 indirect call at the caller.
template <class Fn>
QUIVER_FORCE_INLINE Fn dispatch_get(DispatchEntry& entry, const BackendRow<Fn>& row) noexcept {
  const std::uint32_t now = current_policy_epoch();  // relaxed
  if (entry.fn_epoch.load(std::memory_order_acquire) == now) [[likely]] {
    // The acquire above pairs with resolve()'s release store of fn_epoch, which was preceded
    // by the fn store: a matched epoch guarantees a visible, non-null fn — on ARM's weak
    // memory model as well as x86-TSO. The typed round-trip through KernelFn is well defined.
    return reinterpret_cast<Fn>(entry.fn.load(std::memory_order_relaxed));
  }
  return resolve(entry, row);
}

// Warmup registry row: type-erased resolver thunk per concrete symbol (REQ-DISP-010).
struct WarmupEntry {
  DispatchEntry* entry;
  KernelFn (*resolve_thunk)(DispatchEntry&) noexcept;
};

}  // namespace detail
QUIVER_END_NAMESPACE
