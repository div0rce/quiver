# 14 — Documentation Architecture

## 1. Purpose

Documentation is a co-deliverable: per-kernel pages are the marketing (OA §10), the ledger's publication surface, and the P4 persona's product (Charter §3, §6.7). This chapter fixes the hierarchy, templates, tooling, ADR materialization, and synchronization rules.

## 2. Requirements

| ID | Requirement |
|---|---|
| REQ-DOC-001 | The docs hierarchy shall be the [02 §3](02-repository-architecture.md) `docs/` tree; each directory carries a `README.md` naming purpose and owning module (REQ-REPO-012). Ownership mirrors implementation ownership (master prompt Part 10). |
| REQ-DOC-002 | Every kernel family shall have `docs/api/<family>.md` following the template of §5 — created **in the same milestone** as the family's first backend and updated in every milestone that touches the family. Documentation debt blocks milestone gates. |
| REQ-DOC-003 | Every module shall have `docs/architecture/<module>.md` covering: purpose, responsibilities, non-responsibilities, interfaces, dependencies, lifecycle, memory/threading model, performance notes, failure modes, related REQs and ADRs (the [05](05-internal-architecture.md)/[08](08-kernel-design.md) specs restated for readers, kept synchronized). |
| REQ-DOC-004 | ADRs shall be materialized as standalone files `docs/adr/ADR-NNN-<slug>.md` at M0, extracted verbatim from this PRD, with an index `docs/adr/README.md`. From M0 onward `docs/adr/` is the canonical, living ADR home (status changes happen there); the PRD text remains the historical record. Every ADR status change requires a PR touching the ADR file and the index. |
| REQ-DOC-005 | The documentation site shall build with MkDocs + Material theme (pinned in `docs/requirements.txt`), `mkdocs build --strict` (broken links fail CI, REQ-CI-002 docs job). |
| REQ-DOC-006 | Every C++ snippet in docs shall be extracted (MkDocs snippet include) from `examples/` or `tests/` — no free-floating code blocks that can rot (master prompt Part 10 "examples shall compile"). |
| REQ-DOC-007 | Ledger numbers in docs shall be referenced by `entry_id` with the docs-build existence check (REQ-LEDGER-015); every Tier A family page carries the explicit-vs-autovec verdict block from v0.3 (REQ-LEDGER-011). |
| REQ-DOC-008 | Guides shall exist per milestone plan: `getting-started.md` (M3), `building.md` (M0), `benchmarking/running.md` + `methodology.md` (M2/M5), `vendoring.md` (M8), `disputes.md` (M5). |
| REQ-DOC-009 | `CONTRIBUTING.md` (M0) shall cover: repo structure pointer, coding standards pointer, DCO sign-off, PR checklist (REQs/ADRs referenced, tests+docs updated, gates green), review standards, benchmark-dispute process pointer (Charter §12). |
| REQ-DOC-010 | Release notes per release under `docs/releases/vX.Y.Z.md` using the template: summary, completed milestones, completed REQs (by ID), API changes, ledger changes, ADR changes, known limitations, migration notes (master prompt Part 10). |
| REQ-DOC-011 | Terminology shall follow the PRD glossary ([README.md](README.md)); docs CI includes a lexicon lint (deny-list of ambiguous terms, e.g., bare "mask" without bitmap/lane qualifier — list maintained in `docs/README.md`). |
| REQ-DOC-012 | Documentation changes ship in the same PR as the behavior they describe (documentation-debt prohibition, master prompt Part 10); review checklist enforces it. |

## 3. Site navigation (mkdocs.yml, normative)

Home → Getting started → API reference (core, dispatch, 10 family pages) → Architecture (modules) → Benchmarks & Ledger (methodology, running, per-µarch results, investigations) → Internals (SIMD notes, dispatch state machine, CPU detection) → ADRs → Contributing → Releases.

## 4. ADR-019 — Documentation toolchain

- **Status:** Accepted.
- **Context:** small stable API surface (≈30 entry points); docs are a primary adoption artifact; solo maintenance (T8: boring).
- **Alternatives:** (1) Doxygen(+Breathe) — rejected: generated reference adds toolchain weight and reads worse than curated pages for a surface this small; header comments would duplicate the [04](04-public-api.md) contracts; (2) plain README tree without a site — rejected: navigation and strict link checking matter for the ledger's credibility; (3) **MkDocs Material with hand-written reference pages + compiled snippet extraction** (selected).
- **Consequences:** API pages are hand-maintained — REQ-DOC-002/012 synchronization discipline is the price; the M10 freeze review includes a signature-by-signature docs-vs-headers audit.
- **Reconsideration:** if the API surface triples (v2), revisit generated reference.
- **Related:** REQ-DOC-002/005/006.

## 5. Kernel family page template (normative)

1. **One-paragraph purpose** + roofline class (REQ-KERNEL-008).
2. **Contract:** signatures (mirroring [04](04-public-api.md)), preconditions, postconditions, capacity table row, aliasing row, numeric semantics.
3. **Scalar reference:** included verbatim from `_scalar_impl.h` (snippet extraction) — the T3 teaching artifact.
4. **Per-ISA notes:** technique + microarchitectural rationale with Survey citations (REQ-KERNEL-008); evidence-gated decisions recorded here (REQ-KERNEL-007).
5. **Ledger:** entry-referenced results table; **verdict block** (explicit vs autovec, wins and losses — REQ-LEDGER-011); reproduction command.
6. **Usage:** minimal, typical, and edge-case examples (master prompt Part 7 triple), extracted from `examples/` or the family's unit tests (the two REQ-DOC-006 snippet sources); composition idioms (e.g., K7 null handling, K10 position extraction).
7. **Traceability footer:** REQs, ADRs, tests, benchmarks.

## 6. Failure modes / acceptance / traceability

Strict docs build red = PR blocked; entry-id check failures = PR blocked; missing family page at a family's milestone = gate failure. **Acceptance:** site builds strict; templates §5 satisfied for all shipped families; ADR files + index exist and match the PRD inventory ([01 §3](01-traceability.md)). **Traceability:** Charter §6.7, §12, T2/T3/T7/T8; OA §10 (docs-as-marketing) → REQ-DOC-001..012 → ADR-019 → milestones M0+ (per [18](18-milestones.md)).
