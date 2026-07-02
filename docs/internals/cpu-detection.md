# CPU feature detection (MOD-CPU internals)

Implementation notes for `src/cpu/cpu_features.cpp` (REQ-INT-001, ADR-005). Detection is a pure function — no caching (that is MOD-DISPATCH's job) — and reports a tier only when both the CPU instructions **and** the OS-managed register state are present (REQ-DISP-004).

## x86-64 (Intel SDM references)

| Check | Source | Bits |
|---|---|---|
| OSXSAVE + AVX | CPUID leaf 1, ECX | bit 27 (OSXSAVE), bit 28 (AVX) — both required before any XGETBV use |
| YMM OS state | XGETBV(XCR0) | bits 1–2 (`0x6`): XMM + YMM saved |
| ZMM OS state | XGETBV(XCR0) | bits 1–2, 5–7 (`0xE6`): + opmask, ZMM_Hi256, Hi16_ZMM |
| AVX2 | CPUID leaf 7.0, EBX | bit 5 |
| AVX-512 required set | CPUID leaf 7.0, EBX | bit 16 (F), bit 17 (DQ), bit 30 (BW), bit 31 (VL) — all four required (REQ-DISP-004) |
| VBMI2 (optional sub-feature) | CPUID leaf 7.0, ECX | bit 6 |
| VPOPCNTDQ (optional sub-feature) | CPUID leaf 7.0, ECX | bit 14 |
| Brand string | CPUID leaves 0x80000002–4 | 48 bytes |

XGETBV is emitted as inline assembly on GCC/Clang so the TU needs no `-mxsave` flag (REQ-BUILD-005: no global ISA flags anywhere); MSVC uses `_xgetbv`. Monotonicity is enforced at the source: `avx512 &= avx2`.

## ARM64

Advanced SIMD (NEON) is architecturally mandatory in AArch64. Linux additionally confirms via `getauxval(AT_HWCAP) & HWCAP_ASIMD`; macOS reads the brand via `sysctlbyname("machdep.cpu.brand_string")`. `kAvx*` tiers are never reported (REQ-DISP-002/-012).

## Unknown platforms

All-false features (scalar-only) — the conservative baseline; never a guess, never a crash (PRD 07 §7).

## SVE2

Deliberately absent from v1 sources (REQ-SIMD-010; Charter §6.3 — exploratory branch only).
