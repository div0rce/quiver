// MOD-KCOMMON: target-region macros enabling per-ISA code generation without global flags
// (ADR-003; REQ-SIMD-002). x86 backends wrap their whole implementation namespace in
// QUIVER_TARGET_*_BEGIN/END; NEON needs no region on ARM64 (baseline); MSVC compiles ISA TUs
// with per-file /arch flags instead (docs/prd/03-build-system.md).
// First consumed by the AVX2 backends at M4; committed at M3 per the milestone file list.
// Module: MOD-KCOMMON | REQs: REQ-SIMD-002, REQ-BUILD-005 | ADR-003
#pragma once

#if defined(__x86_64__) || defined(_M_X64)

// AVX-512 has TWO regions: the BASE required set F+BW+DQ+VL (REQ-SIMD-004 — the dispatch
// precondition, safe on the SDE -skx profile) and a VBMI2 region that adds the optional
// sub-feature (used only by resolution-time VBMI2 variants, REQ-DISP-011). Base backends must
// compile under the base region so the compiler cannot emit VBMI2/VPOPCNTDQ into the -skx path.
#if defined(__clang__)
// Build the pragma from TOKENS so clang-format cannot split the long target string into two
// adjacent literals (which _Pragma rejects — it takes a single string literal).
#define QUIVER_DO_PRAGMA(x) _Pragma(#x)
#define QUIVER_TARGET_AVX2_BEGIN                                                                   \
  _Pragma("clang attribute push(__attribute__((target(\"avx2,bmi2\"))), apply_to = function)")
#define QUIVER_TARGET_AVX2_END _Pragma("clang attribute pop")
#define QUIVER_TARGET_AVX512_BEGIN                                                                 \
  QUIVER_DO_PRAGMA(clang attribute push(                                                           \
      __attribute__((target("avx512f,avx512bw,avx512dq,avx512vl"))), apply_to = function))
#define QUIVER_TARGET_AVX512_END _Pragma("clang attribute pop")
#define QUIVER_TARGET_AVX512_VBMI2_BEGIN                                                           \
  QUIVER_DO_PRAGMA(clang attribute push(                                                           \
      __attribute__((target("avx512f,avx512bw,avx512dq,avx512vl,avx512vbmi2"))),                   \
      apply_to = function))
#define QUIVER_TARGET_AVX512_VBMI2_END _Pragma("clang attribute pop")
#elif defined(__GNUC__)
#define QUIVER_TARGET_AVX2_BEGIN _Pragma("GCC push_options") _Pragma("GCC target(\"avx2,bmi2\")")
#define QUIVER_TARGET_AVX2_END _Pragma("GCC pop_options")
#define QUIVER_TARGET_AVX512_BEGIN                                                                 \
  _Pragma("GCC push_options") _Pragma("GCC target(\"avx512f,avx512bw,avx512dq,avx512vl\")")
#define QUIVER_TARGET_AVX512_END _Pragma("GCC pop_options")
#define QUIVER_TARGET_AVX512_VBMI2_BEGIN                                                           \
  _Pragma("GCC push_options")                                                                      \
      _Pragma("GCC target(\"avx512f,avx512bw,avx512dq,avx512vl,avx512vbmi2\")")
#define QUIVER_TARGET_AVX512_VBMI2_END _Pragma("GCC pop_options")
#else  // MSVC: per-TU /arch flags (REQ-SIMD-002 exception; docs/prd/03 §3)
#define QUIVER_TARGET_AVX2_BEGIN
#define QUIVER_TARGET_AVX2_END
#define QUIVER_TARGET_AVX512_BEGIN
#define QUIVER_TARGET_AVX512_END
#define QUIVER_TARGET_AVX512_VBMI2_BEGIN
#define QUIVER_TARGET_AVX512_VBMI2_END
#endif

#else  // non-x86: regions are no-ops (NEON is the ARM64 baseline)

#define QUIVER_TARGET_AVX2_BEGIN
#define QUIVER_TARGET_AVX2_END
#define QUIVER_TARGET_AVX512_BEGIN
#define QUIVER_TARGET_AVX512_END
#define QUIVER_TARGET_AVX512_VBMI2_BEGIN
#define QUIVER_TARGET_AVX512_VBMI2_END

#endif
