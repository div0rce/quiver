// Dispatch policy state and resolution (PRD 07): feature-mask caching, once-read QUIVER_ISA
// environment cap, programmatic override with policy epoch, warmup, Surface C queries.
// Module: MOD-DISPATCH | REQs: REQ-DISP-001..010, -012, REQ-CORE-004 | ADRs: ADR-004, ADR-005
#include <atomic>
#include <cstdlib>
#include <cstring>

#include "quiver/detail/extern_decls.h"
#include "quiver/dispatch.h"
#include "src/cpu/cpu_features.h"
#include "src/dispatch/dispatch_internal.h"

QUIVER_BEGIN_NAMESPACE
namespace detail {

namespace {

// Global dispatch state (REQ-DISP-007): all constinit, no dynamic initialization
// (REQ-CORE-004), no locks, no allocation.
//
// g_feature_mask bit layout: bit7 = detection-complete flag (an all-false-features CPU is
// representable distinctly from "not yet detected"); bit0 neon, bit1 avx2, bit2 avx512,
// bit3 avx512vbmi2, bit4 avx512vpopcntdq.
constexpr std::uint8_t kDetectedBit = 0x80u;
constexpr std::uint8_t kNeonBit = 0x01u;
constexpr std::uint8_t kAvx2Bit = 0x02u;
constexpr std::uint8_t kAvx512Bit = 0x04u;

constinit std::atomic<std::uint32_t> g_policy_epoch{1};  // epochs start at 1; 0 = never valid
constinit std::atomic<std::int8_t> g_override{-1};       // -1 = none, else int(Isa)
constinit std::atomic<std::uint8_t> g_feature_mask{0};
// QUIVER_ISA cache: -2 = unread, -1 = unset/ignored, else int(Isa). A fifth state slot beyond
// the REQ-DISP-007 inventory — recorded as a deviation with a proposed PRD amendment in
// docs/releases/gates/M1.md (REQ-DISP-005 mandates read-exactly-once, which requires storage).
constinit std::atomic<std::int8_t> g_env_cap{-2};

std::int8_t parse_env_isa(const char* value) noexcept {
  if (value == nullptr) {
    return -1;
  }
  // Case-sensitive by contract; unrecognized values are ignored (REQ-DISP-005).
  if (std::strcmp(value, "scalar") == 0) {
    return static_cast<std::int8_t>(Isa::kScalar);
  }
  if (std::strcmp(value, "neon") == 0) {
    return static_cast<std::int8_t>(Isa::kNeon);
  }
  if (std::strcmp(value, "avx2") == 0) {
    return static_cast<std::int8_t>(Isa::kAvx2);
  }
  if (std::strcmp(value, "avx512") == 0) {
    return static_cast<std::int8_t>(Isa::kAvx512);
  }
  return -1;
}

// First policy computation detects CPU features and reads QUIVER_ISA exactly once, together
// (REQ-DISP-005). Duplicate concurrent detection is benign: detect_cpu_features() is pure and
// every publisher writes identical values (REQ-INT-001; PRD 07 §6).
std::uint8_t feature_mask() noexcept {
  std::uint8_t mask = g_feature_mask.load(std::memory_order_acquire);
  if ((mask & kDetectedBit) != 0) {
    return mask;
  }
  const CpuFeatures f = detect_cpu_features();
  std::uint8_t fresh = kDetectedBit;
  fresh |= f.neon ? kNeonBit : 0u;
  fresh |= f.avx2 ? kAvx2Bit : 0u;
  fresh |= f.avx512 ? kAvx512Bit : 0u;
  fresh |= f.avx512vbmi2 ? std::uint8_t{0x08} : 0u;
  fresh |= f.avx512vpopcntdq ? std::uint8_t{0x10} : 0u;
  // Environment cap is published before the detected flag so any reader observing the flag
  // (acquire) also observes the cached env value.
  g_env_cap.store(parse_env_isa(std::getenv("QUIVER_ISA")), std::memory_order_relaxed);
  g_feature_mask.store(fresh, std::memory_order_release);
  return fresh;
}

bool mask_supports(std::uint8_t mask, Isa isa) noexcept {
  switch (isa) {
  case Isa::kScalar:
    return true;
  case Isa::kNeon:
    return (mask & kNeonBit) != 0;
  case Isa::kAvx2:
    return (mask & kAvx2Bit) != 0;
  case Isa::kAvx512:
    return (mask & kAvx512Bit) != 0;
  }
  return false;  // out-of-range enum value (API-DISP invalid input: false, PRD 04 §4)
}

Isa hw_max(std::uint8_t mask) noexcept {
  if ((mask & kAvx512Bit) != 0) {
    return Isa::kAvx512;
  }
  if ((mask & kAvx2Bit) != 0) {
    return Isa::kAvx2;
  }
  if ((mask & kNeonBit) != 0) {
    return Isa::kNeon;
  }
  return Isa::kScalar;
}

}  // namespace

// --- Scalar backend declarations (defined in the family *_scalar.cpp TUs) -------------------
namespace scalar {
#define QUIVER_DECLARE_BACKEND(uid, ret, name, params, args) ret name params noexcept;
QUIVER_KERNEL_ENTRY_LIST(QUIVER_DECLARE_BACKEND)
#undef QUIVER_DECLARE_BACKEND
}  // namespace scalar

// --- AVX2 backend declarations (defined in the family *_avx2.cpp TUs, x86-64 builds only;
// --- the Tier A inventory has full AVX2 coverage, so the whole list declares) ---------------
#if defined(__x86_64__) || defined(_M_X64)
namespace avx2 {
#define QUIVER_DECLARE_BACKEND(uid, ret, name, params, args) ret name params noexcept;
QUIVER_KERNEL_ENTRY_LIST(QUIVER_DECLARE_BACKEND)
#undef QUIVER_DECLARE_BACKEND
}  // namespace avx2
#define QUIVER_AVX2_BACKEND(name) &avx2::name
#else
#define QUIVER_AVX2_BACKEND(name) nullptr
#endif

// --- NEON backend declarations (defined in the family *_neon.cpp TUs, ARM64 builds only;
// --- full Tier A coverage, so the whole list declares) --------------------------------------
#if defined(__aarch64__) || defined(_M_ARM64)
namespace neon {
#define QUIVER_DECLARE_BACKEND(uid, ret, name, params, args) ret name params noexcept;
QUIVER_KERNEL_ENTRY_LIST(QUIVER_DECLARE_BACKEND)
#undef QUIVER_DECLARE_BACKEND
}  // namespace neon
#define QUIVER_NEON_BACKEND(name) &neon::name
#else
#define QUIVER_NEON_BACKEND(name) nullptr
#endif

// --- Entries + typed rows, one per concrete symbol (REQ-DISP-007; constinit, REQ-CORE-004).
// Rows hold typed pointers so no (non-constexpr) function-pointer cast is needed at init;
// AVX2 populates on x86-64 builds, NEON on ARM64 builds; AVX-512 lands at M7.
namespace {
// NOLINTBEGIN(bugprone-macro-parentheses): ret/params/args are type, signature, and call
// syntax — parenthesizing them is not valid C++.
#define QUIVER_DEFINE_ENTRY(uid, ret, name, params, args)                                          \
  constinit DispatchEntry g_entry_##uid;                                                           \
  constinit BackendRow<ret(*) params noexcept> g_row_##uid{                                        \
      {&scalar::name, QUIVER_NEON_BACKEND(name), QUIVER_AVX2_BACKEND(name), nullptr}};             \
  KernelFn warm_##uid(DispatchEntry& e) noexcept {                                                 \
    return reinterpret_cast<KernelFn>(resolve(e, g_row_##uid));                                    \
  }
QUIVER_KERNEL_ENTRY_LIST(QUIVER_DEFINE_ENTRY)
#undef QUIVER_DEFINE_ENTRY

constinit const WarmupEntry g_registry[] = {
#define QUIVER_REGISTRY_ROW(uid, ret, name, params, args) {&g_entry_##uid, &warm_##uid},
    QUIVER_KERNEL_ENTRY_LIST(QUIVER_REGISTRY_ROW)
#undef QUIVER_REGISTRY_ROW
};
static_assert(sizeof(g_registry) / sizeof(g_registry[0]) == kKernelEntryCount);
}  // namespace

// --- Dispatched wrappers: the concrete symbols the public facades call (ADR-006) ------------
#define QUIVER_DEFINE_WRAPPER(uid, ret, name, params, args)                                        \
  ret name params noexcept {                                                                       \
    return dispatch_get(g_entry_##uid, g_row_##uid) args;                                          \
  }
QUIVER_KERNEL_ENTRY_LIST(QUIVER_DEFINE_WRAPPER)
#undef QUIVER_DEFINE_WRAPPER
// NOLINTEND(bugprone-macro-parentheses)

std::uint32_t current_policy_epoch() noexcept {
  return g_policy_epoch.load(std::memory_order_relaxed);
}

Isa current_policy_cap() noexcept {
  const std::uint8_t mask = feature_mask();
  Isa cap = hw_max(mask);
  // Environment cap applies only when the named tier is supported on this CPU; unsupported
  // values are ignored (REQ-DISP-005).
  const std::int8_t env = g_env_cap.load(std::memory_order_relaxed);
  if (env >= 0 && mask_supports(mask, static_cast<Isa>(env)) && static_cast<Isa>(env) < cap) {
    cap = static_cast<Isa>(env);
  }
  // Programmatic override was validated at set time (REQ-DISP-006).
  const std::int8_t ovr = g_override.load(std::memory_order_relaxed);
  if (ovr >= 0 && static_cast<Isa>(ovr) < cap) {
    cap = static_cast<Isa>(ovr);
  }
  return cap;
}

}  // namespace detail

// --- Surface C ---------------------------------------------------------------------------

Isa active_isa() noexcept {
  return detail::current_policy_cap();
}

bool cpu_supports(Isa isa) noexcept {
  return detail::mask_supports(detail::feature_mask(), isa);
}

bool set_isa_override(Isa isa) noexcept {
  // kScalar is always accepted; other tiers require CPU support; out-of-range enum values
  // return false with no state change (API-DISP-003).
  if (isa != Isa::kScalar && !cpu_supports(isa)) {
    return false;
  }
  detail::g_override.store(static_cast<std::int8_t>(isa), std::memory_order_release);
  detail::g_policy_epoch.fetch_add(1, std::memory_order_release);
  return true;
}

void clear_isa_override() noexcept {
  detail::g_override.store(-1, std::memory_order_release);
  detail::g_policy_epoch.fetch_add(1, std::memory_order_release);
}

void warmup() noexcept {
  // Forces detection + env read, then resolves every registry entry under the current policy
  // (REQ-DISP-010). Idempotent.
  (void)detail::current_policy_cap();
  for (const detail::WarmupEntry& w : detail::g_registry) {
    (void)w.resolve_thunk(*w.entry);
  }
}

QUIVER_END_NAMESPACE
