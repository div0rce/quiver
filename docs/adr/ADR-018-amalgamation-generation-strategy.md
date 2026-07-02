# ADR-018 — Amalgamation generation strategy

- **Identifier:** ADR-018
- **Title:** Amalgamation generation strategy
- **Status:** Accepted
- **Source:** extracted verbatim from the Engineering PRD, [docs/prd/03-build-system.md](../prd/03-build-system.md) (REQ-DOC-004). From M0 onward this file is the canonical, living ADR record; status changes happen here and in the [index](README.md), with the PRD text remaining the historical record.

---

- **Status:** Accepted.
- **Context:** the pair `quiver.h`/`quiver.cpp` must compile as one TU on GCC/Clang with full ISA coverage, and degrade gracefully on MSVC.
- **Problem:** a single TU cannot receive per-file ISA flags, and naive concatenation breaks include guards and ordering.
- **Alternatives:** (a) ship per-ISA amalgamated TUs (4 files) — rejected: violates the two-file promise; (b) preprocessor-driven single pair with target regions (selected); (c) unity-build CMake trick — rejected: not a drop-in artifact.
- **Decision:** `amalgamate.py` shall emit:
  - `quiver.h` = license banner + version stamp (git describe) + public headers concatenated in dependency order (`detail/config.h`, `core.h`, `detail/extern_decls.h`, `dispatch.h`, then the nine kernel headers alphabetically), include guards stripped, `#include <std…>` hoisted, deduplicated, and sorted first.
  - `quiver.cpp` = `#include "quiver.h"` + internal headers + all `src/**.cpp` contents in deterministic order (cpu, dispatch, kernels/common, then families alphabetically, scalar→avx2→neon→avx512 within a family). Because ISA code is enabled by target-region macros (ADR-003), the single TU compiles with no special flags on GCC/Clang. On MSVC, each ISA section is additionally guarded so that only baseline-compatible backends compile unless the consumer sets `/arch` (`QUIVER_AMALG_HAS_AVX2` derived from `__AVX2__` etc.); dispatch then selects among compiled-in backends only. This MSVC narrowing shall be documented in the vendoring guide.
  - Determinism: two runs on the same tree shall produce byte-identical output (REQ: sorted, no timestamps beyond the version stamp).
- **Consequences:** source files must keep guard style and internal-include forms uniform (enforced by [17-coding-standards.md](../prd/17-coding-standards.md) REQ-STD-006) so the generator stays trivial.
- **Reconsideration:** if the file count or macro complexity makes text transformation error-prone, switch to a libclang-based generator (PRD amendment).
- **Related:** REQ-BUILD-013, REQ-REPO-011, ADR-002/003.
