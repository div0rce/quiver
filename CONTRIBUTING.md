# Contributing to Quiver

Thank you for considering a contribution. Not every contribution needs the same ceremony: pick
your lane below. The two highest-value contributions right now need **no C++ at all**: running the
benchmark suite on hardware we have not measured, and testing the install paths on a fresh
machine.

## Lane A — easy contributions (no process, just open a PR or issue)

No requirement citations, no specification amendments, no prior mandate. Covered:

- documentation corrections and clarity fixes, typos;
- new runnable examples under `examples/`;
- compiler or platform compatibility reports ("it builds/fails on X with Y");
- **hardware benchmark submissions** (see below — the single most valuable contribution);
- packaging fixes (vcpkg/Conan/CMake consumption);
- reproduction reports for any published ledger number.

The only universal rules that still apply: sign your commits (`git commit -s`, DCO below) and let
CI run. That is it.

### Submitting a hardware benchmark

Quiver's performance record has two registered machines — Apple M2 (NEON) and an Intel i9-9900K,
Coffee Lake (x86-64 AVX2) — stated plainly everywhere numbers appear. An independent submission
(AVX-512 x86, additional Arm, or any third machine) is worth more than any code change. The flow:

```sh
cmake --preset bench && cmake --build --preset bench -j
python3 ledger/runner/quiver_ledger.py community-run --machine <your-machine-id>
```

Open a PR containing only the generated `submission/` directory (manifest, entries, rejected-noisy
record, raw measurements) plus a machine registration file — the format is documented in
[`ledger/README.md`](ledger/README.md) and [running benchmarks](docs/benchmarks/running.md).
Noisy entries are excluded by the CV policy automatically; that is normal and stays in the record.
Open a **hardware benchmark** issue first if you want help with the machine manifest.

## Lane B — normal implementation work

Bug fixes, performance work inside existing kernels, test or benchmark additions. Requirements:

- an issue reference (open one first if none exists);
- tests and documentation updated in the same PR (documentation debt is prohibited, REQ-DOC-012);
- for performance changes: a benchmark hypothesis stated *before* measurement (in-source, the
  existing benches show the pattern) and ledger evidence for any dispatch-choice change
  (REQ-KERNEL-007) — losses are publishable, invented numbers are not;
- all blocking CI gates green (format, lint, build/test matrix, sanitizers, docs build, repo lint).

## Lane C — public API or architectural changes

Quiver is specification-driven: the [Design Charter](docs/design/DESIGN_CHARTER.md) fixes the
product and the [Engineering PRD](docs/prd/README.md) fixes the architecture. Changes to public
contracts, kernel families, permanent non-goals, or the repository layout require the amendment
process first (see [docs/prd/README.md](docs/prd/README.md), "Amendment process"), an ADR where a
design decision is being made or reversed, and the full review gate. Open an issue to discuss
before writing code; the Engine Test (Charter T1) will be applied — Quiver does not grow toward
being an engine.

## Universal rules (all lanes)

- **DCO**: every commit signed off (`git commit -s`), certifying the
  [Developer Certificate of Origin 1.1](https://developercertificate.org/). No CLA.
- **Commit style**: Conventional-Commits prefixes (`feat:`, `fix:`, `bench:`, `docs:`, `test:`,
  `ci:`, `build:`). Lane B/C commit bodies reference the requirement or ADR they advance.
- **Formatting**: `.clang-format` (pinned major in CI) and the coding standards in
  [docs/prd/17-coding-standards.md](docs/prd/17-coding-standards.md) for C++ changes.

## Benchmark disputes

Performance claims are reproducible by contract. To dispute a published ledger number, open an
issue with the **benchmark dispute** template: it asks for your machine manifest and the
reproduction command output. Disputes are resolved in public with reproducible runs — this is a
feature, not a threat.

## Review standards

Reviews check correctness and scope discipline. Lane A changes get a fast, light review. Lane B
adds contract conformance (cited REQs/ADRs) and test/doc co-delivery. Lane C adds the amendment
gate. The repository structure is specified in
[docs/prd/02-repository-architecture.md](docs/prd/02-repository-architecture.md); new files
update the tree manifest (`repo-lint` will tell you).
