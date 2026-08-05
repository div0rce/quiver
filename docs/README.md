# Quiver

Dependency-free C++23 analytical kernels with runtime SIMD dispatch. Compare, filter, gather,
reduce, hash, and unpack columnar data through one portable API; Quiver selects scalar, AVX2,
AVX-512, or NEON at run time, and every performance claim traces to a committed, reproducible
benchmark entry.

**Five-minute path:**

1. [Install](guides/vendoring.md) — two-file drop-in, `FetchContent`, or the CMake package.
2. [Quickstart](guides/getting-started.md) — first compile-and-run.
3. [Recipes](guides/recipes.md) — filter, pipeline, ISA control, nulls; complete tested programs.
4. [API reference](api/core.md) — the contracts, one page per operation family.
5. [Performance](benchmarks/README.md) — what is measured, on what, and
   [what it costs](benchmarks/investigations/apple-m2-full-performance-sweep.md); two machines so
   far, stated plainly ([compatibility](compatibility.md) keeps compiles / tested / measured
   distinct).

Contributions do not require reading any specification for docs, examples, packaging, or hardware
benchmark submissions — see [contributing](contributing.md), Lane A.

---

## How these docs are organized

Everything below this line is the engineering archive: binding specifications, decision records,
per-module architecture, and the full project history. Users rarely need it; contributors to
kernel internals do. New writing follows the
[documentation style guide](style/documentation-style-guide.md), and documentation is a
co-deliverable: every directory has a stated purpose and an owning module, and documentation
changes ship in the same change as the behavior they describe.

## Directory map (REQ-DOC-001, REQ-REPO-012)

| Directory | Purpose | Owner |
|---|---|---|
| [research/](research/README.md) | Pipeline stage 1–2 inputs: literature review, opportunity analysis | maintainer (immutable inputs) |
| [design/](design/README.md) | Pipeline stage 3: the Design Charter (binding product definition) | maintainer (amendment-controlled) |
| [prompts/](prompts/README.md) | Pipeline process contracts (PRD generation master prompt) | maintainer (immutable inputs) |
| [prd/](prd/README.md) | Pipeline stage 4: the Engineering PRD (binding architecture) | maintainer (amendment-controlled) |
| [adr/](adr/README.md) | Architecture Decision Records, the canonical living home for the settled design decisions (REQ-DOC-004) | per owning module |
| [architecture/](architecture/README.md) | Per-module architecture pages (REQ-DOC-003) | per owning module |
| [api/](api/README.md) | Per-family API reference pages (REQ-DOC-002, template PRD 14 §5) | MOD-K1…K10, MOD-CORE, MOD-DISPATCH |
| [benchmarks/](benchmarks/README.md) | Benchmark methodology, running guides, ledger docs, investigations | MOD-BENCH, MOD-LEDGER |
| [guides/](guides/README.md) | Task-oriented guides (building, vendoring, disputes, getting started) | maintainer |
| [internals/](internals/README.md) | Implementation notes for reviewers (SIMD, dispatch, CPU detection, UB catalog) | per owning module |
| [testing/](testing/README.md) | Test taxonomy and how-to (REQ-TEST docs) | MOD-TESTKIT |
| [releases/](releases/README.md) | Release notes and milestone gate records (REQ-DOC-010, REQ-MS-002) | maintainer |

The documentation site is built with MkDocs (`mkdocs.yml` in this directory; strict mode, so broken links fail CI, REQ-DOC-005). Site navigation grows per milestone as pages come into existence; the target navigation is PRD [14 §3](prd/14-documentation.md).

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
