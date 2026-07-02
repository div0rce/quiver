# Quiver documentation

Documentation is a co-deliverable of the product (Charter §6.7; PRD [14-documentation.md](prd/14-documentation.md)). Every directory below has a stated purpose and an owning module; documentation changes ship in the same PR as the behavior they describe (REQ-DOC-012).

## Directory map (REQ-DOC-001, REQ-REPO-012)

| Directory | Purpose | Owner |
|---|---|---|
| [research/](research/README.md) | Pipeline stage 1–2 inputs: literature review, opportunity analysis | maintainer (immutable inputs) |
| [design/](design/README.md) | Pipeline stage 3: the Design Charter (binding product definition) | maintainer (amendment-controlled) |
| [prompts/](prompts/README.md) | Pipeline process contracts (PRD generation master prompt) | maintainer (immutable inputs) |
| [prd/](prd/README.md) | Pipeline stage 4: the Engineering PRD (binding architecture) | maintainer (amendment-controlled) |
| [adr/](adr/README.md) | Architecture Decision Records — the canonical, living ADR home from M0 (REQ-DOC-004) | per owning module |
| [architecture/](architecture/README.md) | Per-module architecture pages (REQ-DOC-003) | per owning module |
| [api/](api/README.md) | Per-family API reference pages (REQ-DOC-002, template PRD 14 §5) | MOD-K1…K10, MOD-CORE, MOD-DISPATCH |
| [benchmarks/](benchmarks/README.md) | Benchmark methodology, running guides, ledger docs, investigations | MOD-BENCH, MOD-LEDGER |
| [guides/](guides/README.md) | Task-oriented guides (building, vendoring, disputes, getting started) | maintainer |
| [internals/](internals/README.md) | Implementation notes for reviewers (SIMD, dispatch, CPU detection, UB catalog) | per owning module |
| [testing/](testing/README.md) | Test taxonomy and how-to (REQ-TEST docs) | MOD-TESTKIT |
| [releases/](releases/README.md) | Release notes and milestone gate records (REQ-DOC-010, REQ-MS-002) | maintainer |

The documentation site is built with MkDocs (`mkdocs.yml` in this directory; strict mode — broken links fail CI, REQ-DOC-005). Site navigation grows per milestone as pages come into existence; the target navigation is PRD [14 §3](prd/14-documentation.md).

## Terminology

The single glossary lives in [prd/README.md](prd/README.md); all documentation uses those definitions identically (REQ-DOC-011).

### Lexicon deny-list (REQ-DOC-011)

The docs lexicon lint rejects these ambiguous usages:

| Denied term | Required replacement |
|---|---|
| bare "mask" | "validity bitmap", "selection bitmap", or "lane mask" |
| bare "tier" | "kernel Tier A/B", "toolchain tier-1/2", or "ISA tier" |
| "vector" for a selection | "selection vector (selvec)" |
| "safe" without qualifier in contract text | name the guarantee (memory-safe in contract, thread-compatible, …) |
