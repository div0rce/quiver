# ADR-005 — First-party CPU feature detection

- **Identifier:** ADR-005
- **Title:** First-party CPU feature detection
- **Status:** Accepted
- **Source:** extracted verbatim from the Engineering PRD, [docs/prd/07-runtime-dispatch.md](../prd/07-runtime-dispatch.md) (REQ-DOC-004). From M0 onward this file is the canonical, living ADR record; status changes happen here and in the [index](README.md), with the PRD text remaining the historical record.

---

**ADR-005 — First-party CPU feature detection.** *Status:* Accepted. *Context:* zero-dependency pledge (Charter T4); need OS-state-correct AVX detection. *Alternatives:* (1) libcpuid/other library — rejected: dependency; (2) compiler builtins `__builtin_cpu_supports` — rejected: no VBMI2/OS-state granularity guarantees across toolchains, MSVC absent; (3) **first-party CPUID/XGETBV + getauxval + sysctl** (selected; ~150 lines, SDM/ARM-ARM-cited). *Consequences:* platform table maintained in `docs/internals/cpu-detection.md`. *Reconsideration:* new OS/arch targets. *Related:* REQ-DISP-004, REQ-INT-001.
