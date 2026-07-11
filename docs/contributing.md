# Contributing

The canonical rules live in the repository's
[CONTRIBUTING.md](https://github.com/div0rce/quiver/blob/main/CONTRIBUTING.md); this
page is the site-side summary. The core idea: **not every contribution needs the same ceremony** —
pick your lane.

## Lane A — easy contributions (no process)

Documentation fixes, new examples, compiler/platform compatibility reports, packaging fixes,
reproduction reports, and **hardware benchmark submissions**. No specification reading, no
requirement citations. Sign your commits (`git commit -s`, DCO) and let CI run — that is all.

The single most valuable contribution is running the benchmark suite on hardware the ledger has
not measured (any x86 machine qualifies today — see [compatibility](compatibility.md)):

```sh
cmake --preset bench && cmake --build --preset bench -j
python3 ledger/runner/quiver_ledger.py community-run --machine <your-machine-id>
```

Then open a PR containing only the generated `submission/` directory. Details:
[running benchmarks](benchmarks/running.md); machine manifest format in the
[ledger overview](https://github.com/div0rce/quiver/blob/main/ledger/README.md). Noisy entries are
excluded by the CV policy automatically — that is the methodology working, not a failure.

## Lane B — normal implementation work

Bug fixes, performance work inside existing kernels, tests and benchmarks. Requires: an issue
reference, tests + docs in the same PR, a pre-registered benchmark hypothesis where performance is
claimed (in-source, before measurement), and green CI. Dispatch-choice changes need ledger
evidence (REQ-KERNEL-007) — losses are publishable, invented numbers are not.

## Lane C — public API or architectural changes

Quiver is specification-driven: the [Design Charter](design/DESIGN_CHARTER.md) fixes the product,
the [Engineering PRD](prd/README.md) fixes the architecture, and changes to either go through the
amendment process plus an ADR and the full review gate. Open an issue first; the Engine Test
(Charter T1) is applied — Quiver does not grow toward being an engine.

## Disputes

Every published number is reproducible by contract. To dispute one, use the **benchmark dispute**
issue template with your machine manifest and reproduction output —
[the disputes guide](guides/disputes.md) walks through it. Disputes are resolved in public.
