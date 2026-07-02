# Quiver Engineering PRD

**Revision 1.0 — July 2026. Status: reviewed and certified implementation-ready ([22](22-final-review-checklist.md), [REVIEW_REPORT.md](REVIEW_REPORT.md)).**

This directory is the complete engineering specification for Quiver, produced as stage four of the pipeline *Literature Review → Opportunity Analysis → Design Charter → Engineering PRD → Implementation*, under the process contract of `docs/prompts/prd-generation-master-prompt.md`.

Binding upstream inputs (higher authority than this PRD):

- `docs/design/DESIGN_CHARTER.md` (v1.0) — product definition; this PRD implements it exactly.
- `docs/research/open-source-opportunity-analysis.md` — positioning; never contradicted.
- `docs/research/vectorized-execution-engine-survey.md` — technical authority, cited as "Survey §n".

## Reading order

Start with [00-executive-summary.md](00-executive-summary.md) (§7 document map, §8 reading order). The traceability backbone is [01-traceability.md](01-traceability.md). Implementation executes [18-milestones.md](18-milestones.md) strictly in order.

## Chapter index

| # | File | # | File |
|---|---|---|---|
| 00 | [Executive summary](00-executive-summary.md) | 12 | [Testing architecture](12-testing-architecture.md) |
| 01 | [Traceability](01-traceability.md) | 13 | [CI architecture](13-ci-architecture.md) |
| 02 | [Repository architecture](02-repository-architecture.md) | 14 | [Documentation](14-documentation.md) |
| 03 | [Build system](03-build-system.md) | 15 | [Security and UB](15-security-and-ub.md) |
| 04 | [Public API](04-public-api.md) | 16 | [Error handling](16-error-handling.md) |
| 05 | [Internal architecture](05-internal-architecture.md) | 17 | [Coding standards](17-coding-standards.md) |
| 06 | [Memory model](06-memory-model.md) | 18 | [Milestones](18-milestones.md) |
| 07 | [Runtime dispatch](07-runtime-dispatch.md) | 19 | [Release plan](19-release-plan.md) |
| 08 | [Kernel design](08-kernel-design.md) | 20 | [Risk register](20-risk-register.md) |
| 09 | [SIMD architecture](09-simd-architecture.md) | 21 | [Future work](21-future-work.md) |
| 10 | [Benchmark architecture](10-benchmark-architecture.md) | 22 | [Final review checklist](22-final-review-checklist.md) |
| 11 | [Performance ledger](11-performance-ledger.md) | — | [REVIEW_REPORT](REVIEW_REPORT.md) |

## Conventions

Normative language (shall/must), requirement IDs (`REQ-<AREA>-NNN`), ADR IDs (`ADR-NNN`), module IDs (`MOD-*`), API IDs (`API-*`), milestones `M0`–`M10` — defined in [00 §2](00-executive-summary.md). ADRs are settled decisions; reopening one requires a PRD amendment. Conflicts anywhere trigger stop-and-report (REQ-META-004).

**Amendment process:** amendments increment the PRD revision, record what changed and why, update [01](01-traceability.md), and — where a change touches a charter-bound item — require a Design Charter amendment first (Charter §0).

## Glossary (single definitions; all chapters use these terms identically)

| Term | Definition |
|---|---|
| **batch** | A contiguous array of `n` elements of one element type, `0 ≤ n ≤ kMaxBatchLen = 2³¹−1` |
| **element type** | One of the ten types of REQ-API-004 (`i8..i64`, `u8..u64`, `f32`, `f64`) |
| **bitmap** | Bit-packed byte buffer, LSB-first, 1 = valid/selected; `⌈n/8⌉` bytes; tail bits zeroed by producers (ADR-016). Roles: *validity* bitmap (null = all-valid allowed) vs *selection* bitmap (non-null required) |
| **selection vector (selvec)** | `uint32_t` index array; *selection semantics*: strictly increasing, in-range (ADR-025); `take` accepts arbitrary/duplicate indices |
| **tail** | The final `n mod W` elements of a batch relative to a backend's vector width W; handled per ADR-015 |
| **family (K1–K10)** | One of the ten closed kernel families (Charter §6.1) |
| **kernel Tier A / Tier B** | Family groups: A = K1–K6 (first release), B = K7–K10 (v0.4) — distinct from *toolchain tier* below |
| **toolchain tier-1 / tier-2** | Support classes of [03 §7](03-build-system.md): tier-1 blocks merges; tier-2 is best-effort (MSVC) |
| **backend** | One compiled implementation of a family for one ISA (`scalar`, `neon`, `avx2`, `avx512`) |
| **variant** | A benchmark-measurable code path: a backend, or an auto-vectorized baseline (`autovec`, `autovec-avx2`, `autovec-avx512`) per ADR-011 |
| **façade / concrete symbol** | The public constrained template (`quiver::compare_bitmap<T>`) vs the per-type function it dispatches to (`quiver::detail::…`), ADR-006 |
| **dispatch entry / policy epoch** | The atomic function-pointer slot per concrete symbol, and the global counter invalidating resolutions when the ISA policy changes ([07](07-runtime-dispatch.md)) |
| **target region** | A `QUIVER_TARGET_*_BEGIN/END` pragma block enabling ISA codegen without global flags (ADR-003) |
| **`_scalar_impl.h` (scalar reference)** | The portable implementation that *is* each family's semantic specification (Charter T3); intrinsic-free (REQ-SIMD-006) |
| **oracle** | An independent source of expected results: the scalar reference (*golden*), testkit's naive re-implementations (*naive*), or the float accumulation-policy model (*policy*) — REQ-TEST-002/004 |
| **differential test** | Byte-exact comparison of a backend against the scalar reference over the REQ-TEST-003 matrix |
| **guard-page test** | Execution with buffers flush against protected pages — the executable no-over-read proof (REQ-TEST-006) |
| **evidence-gated variant** | Two implementations kept behind a compile-time switch; shipped default chosen by ledger data at a milestone gate (REQ-KERNEL-007) |
| **amalgamation** | The generated `quiver.h` + `quiver.cpp` vendoring pair (ADR-018) |
| **ledger / entry / manifest** | The performance product ([11](11-performance-ledger.md)); one measured (benchmark × variant × machine × axes) record; the captured environment description required per run |
| **machine-id / µarch** | A registered reference machine (`ledger/machines/`); its microarchitecture label |
| **QLS-n / QLM-n** | Ledger schema version / ledger methodology version (REQ-LEDGER-001/014) |
| **verdict block** | The mandatory explicit-vs-autovec conclusion on each family doc page, wins and losses alike (REQ-LEDGER-011, Charter T7) |
| **roofline class** | A family's dominant hardware bound (bandwidth / latency-MLP / compute / mixed), [08 §4](08-kernel-design.md) |
| **gate** | A milestone's objective exit checklist; recorded in `docs/releases/gates/` (REQ-MS-002) |
| **shrink point** | The pre-authorized releasable-as-final state at end of M5 (Charter §9.3) |
| **Engine Test** | Charter T1's scope test; anything requiring schemas, planning, scheduling, ownership, or I/O is out of scope permanently |
