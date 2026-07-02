# Coding standards (mirror of PRD 17)

The normative source is PRD [17-coding-standards.md](../prd/17-coding-standards.md); this page is the contributor-facing summary. CI enforces formatting (clang-format major 18), clang-tidy (pinned check set), warnings-as-errors on `ci-*` presets, DCO sign-off, and the repo/include lints.

- **Language:** C++23 floor; conservative subset (REQ-STD-001). Shipped code prohibits: exceptions, RTTI, iostreams, `<regex>`, `std::thread`, coroutines, modules, virtual functions, dynamic initialization (`constinit` enforced), STL containers in public signatures, any allocation, `atomic` outside MOD-DISPATCH.
- **Naming (REQ-STD-003):** files `snake_case`; types/concepts `PascalCase`; functions/variables `snake_case`; constants `kPascalCase`; macros `QUIVER_*`; internal namespaces `quiver::detail[::isa]`.
- **Formatting (REQ-STD-002):** `.clang-format` (LLVM base, 2-space indent, 100 columns, pointer-left); no manual deviation; `// clang-format off` only around LUT literals and target-region blocks.
- **Comments (REQ-STD-004):** state constraints the code cannot show — contract references (`// REQ-…`), microarchitectural rationale with Survey citations, algorithm provenance. No next-line narration, no change-justification comments, no TODO without an issue link.
- **File headers (REQ-STD-005):** every production file starts with one-line purpose, owning module ID, related REQ/ADR IDs.
- **Amalgamation compatibility (REQ-STD-006):** `#pragma once`; quoted project-relative includes (`"quiver/…"`, `"src/…"`); angle brackets for system headers; one namespace-opening style (`QUIVER_BEGIN_NAMESPACE`); no includes inside namespaces/functions.
- **clang-tidy (REQ-STD-007):** pinned checks in `.clang-tidy`; suppressions only via inline `NOLINT(check)` + reason.
- **Wrapping arithmetic (REQ-STD-008):** unsigned internals with `std::bit_cast` at the edges; overflow builtins wrapped once in MOD-KCOMMON helpers (from M3).
- **Commits (REQ-STD-009):** Conventional-Commits prefixes; bodies reference REQ/ADR/milestone; DCO `Signed-off-by` mandatory.
- **PRs (REQ-STD-010):** the checklist in `.github/PULL_REQUEST_TEMPLATE.md`.

**Implementation-agent latitude (PRD 17 §3):** local naming, helper decomposition, loop structuring that preserves documented algorithms — free. Module boundaries, public signatures, named algorithms, benchmark methodology, invariants, file inventory — fixed (amendment required).
