# Architecture Decision Records

The 26 settled engineering decisions of the Quiver Engineering PRD, materialized as standalone files at milestone M0 (REQ-DOC-004). Implementation follows accepted ADRs; reopening one requires a PRD amendment, never implementation-time discretion (master prompt Part 12; PRD [00 §2](../prd/00-executive-summary.md)).

**Conventions.** ADR bodies are preserved verbatim from the PRD (relative links rewritten for this location). Long-form ADRs carry explicitly labeled fields; compressed-prose ADRs use inline labels (*Context:*, *Problem:*, *Constraints:*, *Alternatives:*, *Consequences:*, *Reconsideration:*, *Related:*), and in them **the decision is the alternative marked "(selected)"**. Constraints are stated where applicable per the field-normalization review ([REVIEW_REPORT F-12](../prd/REVIEW_REPORT.md)).

**Lifecycle.** From M0 this directory is the canonical ADR home: a status change (Superseded, Deprecated) is a PR touching the ADR file **and** this index. The PRD chapters remain the historical record.

**Coverage waiver (PRD [01 §5](../prd/01-traceability.md)).** Three master-prompt-mandated ADR areas carry no dedicated ADR by documented waiver: *memory ownership* (fixed by Charter T6/§7.2 → REQ-API-003/REQ-MEM-009), *allocator strategy* (a Charter §8.2 permanent non-goal → REQ-MEM-003), and *threading model* (fixed by Charter T1/§7.3 — no threads, pure functions; the only synchronization design is dispatch, ADR-004). The waiver itself is the recorded decision.

## Index

| ADR | Title | PRD source | Status |
|---|---|---|---|
| [ADR-001](ADR-001-repository-layout-and-module-boundaries.md) | Repository layout and module boundaries | [02-repository-architecture.md](../prd/02-repository-architecture.md) | Accepted |
| [ADR-002](ADR-002-library-form-compiled-static-library-generated-amalgamation.md) | Library form: compiled static library + generated amalgamation | [03-build-system.md](../prd/03-build-system.md) | Accepted |
| [ADR-003](ADR-003-per-isa-translation-units-with-target-region-macros.md) | Per-ISA translation units with target-region macros | [09-simd-architecture.md](../prd/09-simd-architecture.md) | Accepted |
| [ADR-004](ADR-004-lazy-atomic-per-entry-dispatch-with-policy-epoch.md) | Lazy atomic per-entry dispatch with policy epoch | [07-runtime-dispatch.md](../prd/07-runtime-dispatch.md) | Accepted |
| [ADR-005](ADR-005-first-party-cpu-feature-detection.md) | First-party CPU feature detection | [07-runtime-dispatch.md](../prd/07-runtime-dispatch.md) | Accepted |
| [ADR-006](ADR-006-public-api-style.md) | Public API style | [04-public-api.md](../prd/04-public-api.md) | Accepted |
| [ADR-007](ADR-007-inline-namespace-abi-versioning.md) | Inline-namespace ABI versioning | [04-public-api.md](../prd/04-public-api.md) | Accepted |
| [ADR-008](ADR-008-google-benchmark-first-party-ledger-runner.md) | Google Benchmark + first-party ledger runner | [10-benchmark-architecture.md](../prd/10-benchmark-architecture.md) | Accepted |
| [ADR-009](ADR-009-testing-stack.md) | Testing stack | [12-testing-architecture.md](../prd/12-testing-architecture.md) | Accepted |
| [ADR-010](ADR-010-ci-topology-and-sde-based-avx-512-coverage.md) | CI topology and SDE-based AVX-512 coverage | [13-ci-architecture.md](../prd/13-ci-architecture.md) | Accepted |
| [ADR-011](ADR-011-equal-isa-auto-vectorized-baselines.md) | Equal-ISA auto-vectorized baselines | [10-benchmark-architecture.md](../prd/10-benchmark-architecture.md) | Accepted |
| [ADR-012](ADR-012-qhash64-v1-algorithm.md) | qhash64 v1 algorithm | [08-kernel-design.md](../prd/08-kernel-design.md) | Accepted |
| [ADR-013](ADR-013-float-reduction-reassociation-policy.md) | Float reduction reassociation policy | [08-kernel-design.md](../prd/08-kernel-design.md) | Accepted |
| [ADR-014](ADR-014-overflow-reporting-design.md) | Overflow reporting design | [08-kernel-design.md](../prd/08-kernel-design.md) | Accepted |
| [ADR-015](ADR-015-tail-handling-policy.md) | Tail handling policy | [09-simd-architecture.md](../prd/09-simd-architecture.md) | Accepted |
| [ADR-016](ADR-016-bitmap-tail-zeroing-and-output-determinism.md) | Bitmap tail zeroing and output determinism | [06-memory-model.md](../prd/06-memory-model.md) | Accepted |
| [ADR-017](ADR-017-contract-based-error-model.md) | Contract-based error model | [16-error-handling.md](../prd/16-error-handling.md) | Accepted |
| [ADR-018](ADR-018-amalgamation-generation-strategy.md) | Amalgamation generation strategy | [03-build-system.md](../prd/03-build-system.md) | Accepted |
| [ADR-019](ADR-019-documentation-toolchain.md) | Documentation toolchain | [14-documentation.md](../prd/14-documentation.md) | Accepted |
| [ADR-020](ADR-020-statistics-implementation.md) | Statistics implementation | [11-performance-ledger.md](../prd/11-performance-ledger.md) | Accepted |
| [ADR-021](ADR-021-ledger-data-model-and-storage.md) | Ledger data model and storage | [11-performance-ledger.md](../prd/11-performance-ledger.md) | Accepted |
| [ADR-022](ADR-022-first-party-pmu-wrapper.md) | First-party PMU wrapper | [10-benchmark-architecture.md](../prd/10-benchmark-architecture.md) | Accepted |
| [ADR-023](ADR-023-aliasing-contract.md) | Aliasing contract | [06-memory-model.md](../prd/06-memory-model.md) | Accepted |
| [ADR-024](ADR-024-release-and-branching-strategy.md) | Release and branching strategy | [19-release-plan.md](../prd/19-release-plan.md) | Accepted |
| [ADR-025](ADR-025-selection-vector-semantics.md) | Selection-vector semantics | [08-kernel-design.md](../prd/08-kernel-design.md) | Accepted |
| [ADR-026](ADR-026-bit-packing-layout.md) | Bit-packing layout | [08-kernel-design.md](../prd/08-kernel-design.md) | Accepted |
