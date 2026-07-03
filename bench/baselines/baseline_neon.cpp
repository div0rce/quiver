// Equal-ISA auto-vectorized baseline for NEON (ADR-011, REQ-BENCH-010) — an IDENTITY, not a
// recompilation. NEON is the baseline ISA on AArch64: the compiler already auto-vectorizes
// the portable scalar build (`_scalar_impl.h` instantiated in the `*_scalar.cpp` TUs) with
// NEON, so the fair equal-ISA baseline IS the scalar backend, reported under the variant
// name `autovec` (REQ-BENCH-002 vocabulary: no separate `scalar` variant exists on ARM — it
// would measure the same binary code twice). The ledger runner produces the `autovec`
// variant by capping a repetition process with QUIVER_ISA=scalar; bench binaries then
// register their benchmarks under the `autovec` name via quiver::bench::variant_name().
//
// This TU therefore exports no symbols. It exists so the baseline set is explicit in the
// tree (one file per explicit-SIMD tier, per the PRD 18 M5 file list) and so this rationale
// lives next to baseline_avx2.cpp, where the x86 story (which DOES require recompilation
// under a target region) is implemented.
// Module: MOD-BENCH-BASELINES | REQs: REQ-BENCH-010, REQ-BENCH-002 | ADR-011
