# Contributing to Quiver

Thank you for considering a contribution. Quiver is specification-driven: the [Design Charter](docs/design/DESIGN_CHARTER.md) fixes the product and the [Engineering PRD](docs/prd/README.md) fixes the architecture. Contributions implement those documents; they do not renegotiate them. Proposals that change public contracts, kernel families, or permanent non-goals require a charter/PRD amendment first (see [docs/prd/README.md](docs/prd/README.md), "Amendment process").

## Repository structure

The canonical layout, directory ownership, and module map are specified in [docs/prd/02-repository-architecture.md](docs/prd/02-repository-architecture.md) and mirrored in [docs/architecture/module-map.md](docs/architecture/module-map.md). Every file belongs to exactly one module; do not add top-level directories.

## Coding standards

Normative rules live in [docs/prd/17-coding-standards.md](docs/prd/17-coding-standards.md): C++23 feature subset, naming, formatting (`.clang-format`, enforced in CI), clang-tidy configuration, comment policy, and the amalgamation-compatibility source conventions (REQ-STD-006).

## Developer Certificate of Origin (DCO)

Every commit must be signed off (`git commit -s`), certifying the [Developer Certificate of Origin 1.1](https://developercertificate.org/). There is no CLA. Commits without `Signed-off-by` are rejected by CI.

## Commit convention

Conventional-Commits-style prefixes (`feat:`, `fix:`, `bench:`, `docs:`, `test:`, `ci:`, `build:`); the commit body references the requirement (`REQ-…`), ADR, or milestone it advances (REQ-STD-009).

## Pull request checklist

Per REQ-STD-010, every PR must:

- [ ] cite the REQ/ADR identifiers it implements or affects;
- [ ] update tests **and** documentation in the same PR (documentation debt is prohibited, REQ-DOC-012);
- [ ] introduce no new public API without a PRD §04 amendment;
- [ ] introduce no new production file without a mandate in [docs/prd/02-repository-architecture.md](docs/prd/02-repository-architecture.md) §8 / [docs/prd/18-milestones.md](docs/prd/18-milestones.md);
- [ ] pass all blocking CI gates (format, lint, build/test matrix, sanitizers, docs build, repo lint).

## Testing and benchmark expectations

Testing architecture: [docs/prd/12-testing-architecture.md](docs/prd/12-testing-architecture.md). Benchmarks answer stated engineering questions and never gate on CI timing: [docs/prd/10-benchmark-architecture.md](docs/prd/10-benchmark-architecture.md).

## Benchmark disputes

Performance claims are reproducible by contract. To dispute a published ledger number, open an issue with the **benchmark dispute** template: it requires your machine manifest and the reproduction command output. All disputes are resolved in public with reproducible runs.

## Review standards

Reviews check contract conformance (REQs/ADRs cited), test/doc co-delivery, standards compliance, and scope discipline (the Engine Test — Charter T1). Milestones execute strictly in order (REQ-MS-001); work outside the current milestone's scope will be asked to wait.
