// CPU+OS feature detection: CPUID/XGETBV (x86-64), getauxval (Linux/ARM64),
// sysctlbyname (macOS/ARM64). Leaf/bit references: docs/internals/cpu-detection.md.
// Module: MOD-CPU | REQs: REQ-INT-001, REQ-DISP-004 | ADRs: ADR-005
#include "src/cpu/cpu_features.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#if defined(__x86_64__) || defined(_M_X64)
#define QUIVER_CPU_X86_64 1
#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#else
#include <cpuid.h>
#endif
#elif defined(__aarch64__) || defined(_M_ARM64)
#define QUIVER_CPU_ARM64 1
#if defined(__linux__)
#include <sys/auxv.h>
#endif
#if defined(__APPLE__)
#include <sys/sysctl.h>
#endif
#endif

QUIVER_BEGIN_NAMESPACE
namespace detail {

namespace {

#if defined(QUIVER_CPU_X86_64)

struct CpuidRegs {
  unsigned eax, ebx, ecx, edx;
};

CpuidRegs cpuid_count(unsigned leaf, unsigned subleaf) noexcept {
  CpuidRegs r{0, 0, 0, 0};
#if defined(_MSC_VER) && !defined(__clang__)
  int out[4];
  __cpuidex(out, static_cast<int>(leaf), static_cast<int>(subleaf));
  r.eax = static_cast<unsigned>(out[0]);
  r.ebx = static_cast<unsigned>(out[1]);
  r.ecx = static_cast<unsigned>(out[2]);
  r.edx = static_cast<unsigned>(out[3]);
#else
  __cpuid_count(leaf, subleaf, r.eax, r.ebx, r.ecx, r.edx);
#endif
  return r;
}

std::uint64_t xgetbv0() noexcept {
#if defined(_MSC_VER) && !defined(__clang__)
  return _xgetbv(0);
#else
  unsigned lo = 0;
  unsigned hi = 0;
  // XGETBV with ECX=0 reads XCR0. Inline asm avoids requiring -mxsave on the whole TU
  // (REQ-BUILD-005: no global ISA flags). Guarded by the OSXSAVE CPUID bit before use.
  __asm__ volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0));
  return (static_cast<std::uint64_t>(hi) << 32) | lo;
#endif
}

void read_brand_x86(char (&brand)[64]) noexcept {
  const unsigned max_ext = cpuid_count(0x80000000u, 0).eax;
  if (max_ext < 0x80000004u) {
    std::memcpy(brand, "x86_64", 7);
    return;
  }
  char raw[48];
  for (std::size_t i = 0; i < 3; ++i) {
    const CpuidRegs r = cpuid_count(0x80000002u + static_cast<unsigned>(i), 0);
    std::memcpy(raw + 16 * i + 0, &r.eax, 4);
    std::memcpy(raw + 16 * i + 4, &r.ebx, 4);
    std::memcpy(raw + 16 * i + 8, &r.ecx, 4);
    std::memcpy(raw + 16 * i + 12, &r.edx, 4);
  }
  std::memcpy(brand, raw, 48);
  brand[48] = '\0';
  if (brand[0] == '\0') {  // some emulators (e.g. SDE -skx) expose the leaves but return no
    std::memcpy(brand, "x86_64", 7);  // string — keep the brand non-empty like the older path
  }
}

CpuFeatures detect_x86() noexcept {
  CpuFeatures f{};
  read_brand_x86(f.brand);

  const unsigned max_leaf = cpuid_count(0, 0).eax;
  if (max_leaf < 7) {
    return f;  // pre-AVX2 hardware: scalar baseline only
  }

  const CpuidRegs l1 = cpuid_count(1, 0);
  constexpr unsigned kOsxsaveBit = 1u << 27;
  constexpr unsigned kAvxBit = 1u << 28;
  if ((l1.ecx & kOsxsaveBit) == 0 || (l1.ecx & kAvxBit) == 0) {
    return f;  // no OS-managed extended state: no AVX tiers (REQ-DISP-004)
  }

  const std::uint64_t xcr0 = xgetbv0();
  const bool ymm_state = (xcr0 & 0x6u) == 0x6u;    // XMM+YMM
  const bool zmm_state = (xcr0 & 0xE6u) == 0xE6u;  // + opmask, ZMM_Hi256, Hi16_ZMM

  const CpuidRegs l7 = cpuid_count(7, 0);
  const bool has_avx2 = (l7.ebx & (1u << 5)) != 0;
  const bool has_bmi2 = (l7.ebx & (1u << 8)) != 0;
  const bool has_512f = (l7.ebx & (1u << 16)) != 0;
  const bool has_512dq = (l7.ebx & (1u << 17)) != 0;
  const bool has_512bw = (l7.ebx & (1u << 30)) != 0;
  const bool has_512vl = (l7.ebx & (1u << 31)) != 0;
  const bool has_vbmi2 = (l7.ecx & (1u << 6)) != 0;
  const bool has_vpopcntdq = (l7.ecx & (1u << 14)) != 0;

  // Quiver's avx2 tier compiles with target("avx2,bmi2") and emits PDEP/PEXT, so the tier
  // is reported only when BOTH are present (REQ-DISP-004; docs/internals/cpu-detection.md).
  f.avx2 = ymm_state && has_avx2 && has_bmi2;
  f.avx512 = zmm_state && has_512f && has_512bw && has_512dq && has_512vl;
  f.avx512 = f.avx512 && f.avx2;  // monotone by construction (REQ-DISP-004)
  f.avx512vbmi2 = f.avx512 && has_vbmi2;
  f.avx512vpopcntdq = f.avx512 && has_vpopcntdq;
  return f;
}

#elif defined(QUIVER_CPU_ARM64)

CpuFeatures detect_arm64() noexcept {
  CpuFeatures f{};
  // Advanced SIMD is architecturally mandatory in AArch64; getauxval confirms on Linux
  // (PRD 05 §4).
  f.neon = true;
#if defined(__linux__)
  f.neon = (getauxval(AT_HWCAP) & HWCAP_ASIMD) != 0;
#endif
#if defined(__APPLE__)
  std::size_t size = sizeof(f.brand);
  if (sysctlbyname("machdep.cpu.brand_string", f.brand, &size, nullptr, 0) != 0) {
    std::memcpy(f.brand, "arm64 (Apple)", 14);
  }
#else
  std::memcpy(f.brand, "aarch64", 8);
#endif
  return f;
}

#else

CpuFeatures detect_fallback() noexcept {
  // Unknown platform: scalar-only, never a crash (conservative baseline, PRD 07 §7).
  CpuFeatures f{};
  std::memcpy(f.brand, "unknown", 8);
  return f;
}

#endif

}  // namespace

CpuFeatures detect_cpu_features() noexcept {
#if defined(QUIVER_CPU_X86_64)
  CpuFeatures f = detect_x86();
#elif defined(QUIVER_CPU_ARM64)
  CpuFeatures f = detect_arm64();
#else
  CpuFeatures f = detect_fallback();
#endif
#if defined(QUIVER_TEST_SEAMS)
  // Test-only seam (compiled out of any tests-off/release/install build — shipped detection is
  // pure CPUID, PRD 07). QUIVER_TEST_FORCE_ISA lets a host whose emulator EXECUTES AVX-512 but
  // reports no AVX-512 in CPUID (QEMU) exercise the AVX-512 dispatch path; the three values
  // mirror the SDE -skx (no VBMI2) and -spr (VBMI2/VPOPCNTDQ) profiles. Monotone.
  if (const char* forced = std::getenv("QUIVER_TEST_FORCE_ISA")) {
    const bool vbmi2 = std::strcmp(forced, "avx512vbmi2") == 0;
    const bool vpopcntdq = std::strcmp(forced, "avx512vpopcntdq") == 0;
    if (std::strcmp(forced, "avx512") == 0 || vbmi2 || vpopcntdq) {
      f.avx2 = true;
      f.avx512 = true;
      f.avx512vbmi2 = vbmi2;
      f.avx512vpopcntdq = vpopcntdq;
    }
  }
#endif
  return f;
}

}  // namespace detail
QUIVER_END_NAMESPACE
