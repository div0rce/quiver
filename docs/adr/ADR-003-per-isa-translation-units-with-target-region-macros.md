# ADR-003 — Per-ISA translation units with target-region macros

- **Identifier:** ADR-003
- **Title:** Per-ISA translation units with target-region macros
- **Status:** Accepted
- **Source:** extracted verbatim from the Engineering PRD, [docs/prd/09-simd-architecture.md](../prd/09-simd-architecture.md) (REQ-DOC-004). From M0 onward this file is the canonical, living ADR record; status changes happen here and in the [index](README.md), with the PRD text remaining the historical record.

---

- **Status:** Accepted.
- **Context:** shipped binaries must run on baseline CPUs while containing AVX2/AVX-512 code (Charter §6.3 conservative-baseline dispatch); the amalgamation must compile as one TU (ADR-018); MSVC lacks function-level target attributes (tier-2).
- **Problem:** how ISA code gets compiled without global flags and without UB (calling AVX2 code on a non-AVX2 CPU is prevented by dispatch; *compiling* it must not leak into baseline paths).
- **Alternatives:**
  1. *Per-TU `-mavx2` flags only* — works for the normal build, **fails amalgamation** (single TU) and risks the compiler auto-vectorizing shared inline helpers with AVX2 into code reachable from baseline paths (a classic latent-crash bug class).
  2. *GCC/Clang function multiversioning (`target_clones`)* — rejected: uneven MSVC/AppleClang support, ifunc coupling (see ADR-004 alt 1), less explicit dispatch.
  3. *Highway/xsimd abstraction* — charter-prohibited (Charter §4 decision).
  4. **Target-region macros around whole implementation namespaces in dedicated TUs** (selected): the TU boundary provides organizational isolation; the pragma region provides flag-independent compilation; identical source works in normal and amalgamated builds.
- **Decision:** Alternative 4; regions defined once in `target_regions.h`; every symbol inside a region lives in `quiver::detail::<isa>` namespaces so cross-region leakage is nameable and lintable.
- **Consequences:** + one mechanism everywhere; − pragma dialect differences GCC vs Clang (encapsulated in the two macros), MSVC needs the per-TU-flag fallback and amalgamation narrowing (ADR-018).
- **Reconsideration:** if a tier-1 compiler regresses target-pragma codegen quality (tracked by ledger deltas per compiler).
- **Related:** REQ-SIMD-001/002/006, REQ-BUILD-005, ADR-018.
