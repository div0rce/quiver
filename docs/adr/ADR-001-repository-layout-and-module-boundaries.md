# ADR-001 — Repository layout and module boundaries

- **Identifier:** ADR-001
- **Title:** Repository layout and module boundaries
- **Status:** Accepted
- **Source:** extracted verbatim from the Engineering PRD, [docs/prd/02-repository-architecture.md](../prd/02-repository-architecture.md) (REQ-DOC-004). From M0 onward this file is the canonical, living ADR record; status changes happen here and in the [index](README.md), with the PRD text remaining the historical record.

---

- **Status:** Accepted.
- **Context:** Charter T4 (vendorable), T8 (boring on purpose); master prompt Part 5 requires a fully predetermined layout; the amalgamation (ADR-002/018) must be generatable by concatenation-with-rules, which constrains file structure.
- **Problem:** Choose a layout that (a) keeps the public surface minimal and obvious, (b) isolates per-ISA code into units that can carry target regions, (c) lets the scalar reference be recompiled by the bench baselines (ADR-011), (d) keeps dev trees strictly out of the shipped surface.
- **Constraints:** zero shipped dependencies; 10 closed families; per-family independent shippability (OA §10); solo maintainability.
- **Alternatives considered:**
  1. *Single `src/` pool with per-ISA suffixes* — rejected: weak family boundaries, painful file inventory for milestones.
  2. *Monolithic `quiver.h` header-only tree* — rejected with ADR-002 (per-ISA target regions in one header explode compile times; MSVC cannot compile AVX-512 regions without per-TU flags).
  3. *Per-family subdirectory with `_impl.h` reference + per-ISA TUs* (selected).
  4. *Bazel-style fine-grained packages* — rejected: CMake is the ecosystem default for vendorable C++ (T8).
- **Decision:** Layout of §3; five-file pattern per family; `_impl.h` scalar reference is the single source of truth included by both the scalar TU and bench baselines.
- **Consequences:** + deterministic milestones, per-family parallel implementation, trivially explainable to contributors; − 50 kernel files (mitigated: identical pattern), the `_impl.h` must remain target-region-clean (enforced by REQ-SIMD-006).
- **Reconsideration:** if a v2 compressed-kernel expansion (Charter §8.1) needs per-encoding sub-modules.
- **Related:** REQ-REPO-001..012, ADR-002, ADR-003, ADR-011.
