# 17 — Coding Standards

## 1. Purpose

The implementation-level rules that keep the codebase reviewable, amalgamatable, UB-free, and consistent with the Charter's engineering identity (T8: boring on purpose everywhere except the kernels). The implementation agent's *local* freedom (naming inside functions, decomposition, comments) operates strictly inside these rules.

## 2. Requirements

| ID | Requirement |
|---|---|
| REQ-STD-001 | Language floor: C++23 (Charter §7.6). The **allowed-feature subset** for shipped code: concepts/`requires`, `constexpr`/`consteval`/`constinit`, `std::bit_cast`, `<bit>` (`popcount`, `countr_zero`, …), `std::to_underlying`, designated initializers, `if constexpr`, `[[likely]]/[[unlikely]]/[[assume]]` via `QUIVER_*` macros, `std::span`/`std::string_view` **internal only**. **Prohibited in shipped code:** exceptions, RTTI, iostreams, `<regex>`, `std::thread`/futures, coroutines, modules, ranges in kernel loops, virtual functions, dynamic initialization (REQ-CORE-004), STL containers in public signatures, allocation of any kind (REQ-MEM-003), `atomic` outside MOD-DISPATCH. Rationale: portability across tier-1 toolchains ([03 §7](03-build-system.md)) and the §7.3 charter contracts. |
| REQ-STD-002 | Formatting: `.clang-format` committed at M0 (LLVM base; IndentWidth 2; ColumnLimit 100; pointer-left `T* p`); the exact clang-format major version is pinned in CI (format drift across versions breaks the gate). No manual deviation; `// clang-format off` allowed only around LUT literals and target-region macro blocks. |
| REQ-STD-003 | Naming: files `snake_case`; types/concepts `PascalCase`; functions/variables `snake_case`; constants `kPascalCase`; macros `QUIVER_*`; internal namespaces `quiver::detail`, `quiver::detail::avx2/neon/avx512/scalar`. Test names: `TEST(<Family>, <Property>_<Condition>)`. Benchmark names per REQ-BENCH-002. |
| REQ-STD-004 | Comment policy (house style): comments state constraints the code cannot show — contract references (`// REQ-MEM-001: exact tail bytes only`), microarchitectural rationale with Survey citations, and algorithm provenance. Prohibited: narration of the next line, change-justification comments, TODO without an issue link. |
| REQ-STD-005 | Every production file shall begin with a header block: one-line purpose, owning module ID, related REQ/ADR IDs (the file-level traceability hook — master prompt Part 12). |
| REQ-STD-006 | **Amalgamation-compatibility rules** (ADR-018 dependency): include guards as `#pragma once` exactly; internal includes use quoted project-relative form `"quiver/…"` / `"src/…"`; system includes use angle brackets; one namespace-opening style; no `#include` inside namespaces or functions; no macros spanning file boundaries; per-ISA backend TUs that share an ISA namespace (`detail::<isa>`) must not define colliding symbols in their anonymous namespaces — those anonymous namespaces merge when the sources concatenate into the single amalgamation TU, so file-local helpers carry a family-unique name (e.g. `cmp_broadcast`, `ar_mul64_lo`). `amalgamate.py --check` lints the structural rules from M8 (and they apply from M0); the anonymous-namespace collision rule is enforced by the `quiver_amalgamate_verify` compile (REQ-BUILD-013), which fails to build on any collision. |
| REQ-STD-007 | clang-tidy configuration (pinned version + checks committed at M0): `bugprone-*`, `performance-*`, `portability-*`, `modernize-use-bit-cast`, `cppcoreguidelines-init-variables`, `misc-definitions-in-headers`, with documented per-check suppressions only via inline `NOLINT(check)` + reason. |
| REQ-STD-008 | Unsigned-internal idiom: all wrapping integer arithmetic implemented on unsigned types with `bit_cast` at the edges (REQ-SEC-002); overflow builtins (`__builtin_*_overflow` / MSVC equivalents) wrapped once in MOD-KCOMMON helpers. |
| REQ-STD-009 | Commit convention: Conventional-Commits-style prefixes (`feat:`, `fix:`, `bench:`, `docs:`, `test:`, `ci:`, `build:`); every commit body references the REQ/ADR/milestone it advances; DCO `Signed-off-by` required (Charter §12). |
| REQ-STD-010 | PR checklist (template, REQ-DOC-009): REQs/ADRs cited; tests + docs updated in-PR; no new public API without a [04](04-public-api.md) amendment; no new production file without an [02 §8](02-repository-architecture.md)/[18](18-milestones.md) mandate; gates green. |

## 3. Scope of implementation-agent freedom

Free (no approval needed): local variable naming, private helper decomposition within a module, loop structuring that preserves documented algorithms and contracts, comment wording within REQ-STD-004, test-case additions beyond the required matrix. **Not free** (PRD/charter amendment): anything listed in master prompt Part 12's prohibition list — module boundaries, public signatures, algorithms named in [08](08-kernel-design.md), benchmark methodology, invariants, file inventory.

## 4. Acceptance criteria / traceability

Format + tidy gates green from M0; file-header lint (REQ-STD-005) green from M1; amalgamation lint green from M8; a standards page `docs/internals/coding-standards.md` mirrors this chapter. **Traceability:** Charter §7.3/§7.6/T8, house comment style → REQ-STD-001..010 → CI gates ([13](13-ci-architecture.md)) → all milestones.
