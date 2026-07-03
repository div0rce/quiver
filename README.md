# Quiver

**A dependency-free C++23 library of vectorized analytical kernels, with a public cross-ISA performance ledger.**

Quiver implements the ~10 kernel families every analytical engine reimplements — predicate evaluation, filter/compaction, selection↔bitmap conversion, mask algebra, gather/dictionary decode, reductions/SMA, batch hashing, bit-unpacking, and overflow-guarded arithmetic — hand-implemented per ISA (scalar, AVX2, NEON, AVX-512) with runtime dispatch, and ships them together with a reproducible, PMU-instrumented performance ledger across microarchitectures.

Two co-equal products, one repository: **the library** and **the ledger**.

## Project status

**v0.1 — Tier A kernels live (scalar backends):** all six Tier A families (compare, filter, sel_convert, mask_algebra, take/dict_decode, reduce/SMA) with their readable scalar reference implementations, 71 tests across unit/property/differential/invariant/guard-page suites, and validated benchmarks. SIMD backends land next (AVX2 → NEON → AVX-512); the performance ledger arrives with v0.3 — no performance claims before it. See [docs/prd/18-milestones.md](docs/prd/18-milestones.md) for the roadmap (M0–M10 → v0.1–v1.0) and [docs/releases/gates/](docs/releases/gates/) for gate evidence.

## What Quiver is (and is not)

Quiver is infrastructure: a small, vendorable library you import — not an analytical database, not a SQL engine, not a storage engine, not a scheduler, not a general SIMD abstraction. The product boundaries are fixed by the [Design Charter](docs/design/DESIGN_CHARTER.md); the engineering architecture is fixed by the [Engineering PRD](docs/prd/README.md).

## Documentation

| Document | Purpose |
|---|---|
| [Design Charter](docs/design/DESIGN_CHARTER.md) | Product definition: mission, tenets, scope, contracts |
| [Engineering PRD](docs/prd/README.md) | Complete engineering specification (requirements, ADRs, milestones) |
| [Architecture Decision Records](docs/adr/README.md) | The 26 settled engineering decisions |
| [Building](docs/guides/building.md) | Configure/build instructions (configure-only at M0) |
| [Contributing](CONTRIBUTING.md) | Workflow, DCO, review standards |
| [Research inputs](docs/research/) | Literature review and opportunity analysis behind the project |

## License

[Apache-2.0](LICENSE).
