# PRD Consistency Review Report

**Scope:** Quiver Engineering PRD revision 1.0 (all 24 files under `docs/prd/`), reviewed against `docs/prompts/prd-generation-master-prompt.md` (Parts 1–13) and `docs/design/DESIGN_CHARTER.md` (v1.0).
**Date:** 2026-07-02. **Status: all findings resolved — certified implementation-ready (§5).**

## 1. Method

Three passes, in order:

1. **Authoring-time mechanical sweep** (scripted): REQ/ADR identifier inventory (uniqueness, dangling references, range-expansion count), milestone-allocation reconciliation between [18](18-milestones.md) and the [01 §6](01-traceability.md) matrix, cross-file section-anchor audit, file-count arithmetic. Findings P-1…P-6.
2. **Independent adversarial review** by a fresh-context reviewer with no authoring state, instructed to verify every finding against the text before reporting, across seven dimensions (missing requirements, ambiguity, contradictions, scope creep, untraceable decisions, implementation risks, sections requiring revision). Findings F-1…F-18.
3. **Post-fix verification sweep** (scripted): 235 requirement IDs distinct and each covered exactly once by the [01 §6](01-traceability.md) matrix; zero stale pre-fix text; pinning (REQ-DISP-013) wired through chapters 00/01/03/07/13/18.

## 2. Findings from the authoring-time sweep (P-series) — all fixed

| ID | Class | Finding | Resolution |
|---|---|---|---|
| P-1 | Missing artifact | ADR-009 (testing stack) was referenced (00 §4) but never written | Full ADR added at [12 §8](12-testing-architecture.md) |
| P-2 | Contradiction | Production file count stated as 72; actual header inventory is 14, total 73 | [02 §8](02-repository-architecture.md), [03 §4](03-build-system.md) corrected |
| P-3 | Omission | `CMakePresets.json` mandated by REQ-BUILD-011 but absent from the canonical tree | Added to [02 §3](02-repository-architecture.md) |
| P-4 | Contradiction | Milestone REQ lists in 18 had double-allocations (REQ-CI-005, REQ-DOC-008, REQ-LEDGER-012, REQ-KERNEL-007 legs, REQ-API-009/REL ranges) and 15 unallocated IDs (REQ-API-007, -BUILD-007, -REPO-009, -STD-004, -TEST-004/-008/-010, -ERR-005/-008, -SIMD-009/-010, -DOC-005/-010, REL split, K5-004) | 18's per-milestone lists reconciled to the single-owner convention; [01 §6](01-traceability.md) declared authoritative for allocation |
| P-5 | Broken references | Several prose anchors pointed at wrong chapter-12 sections (guard-page, avalanche, differential-matrix content) | Corrected to §2 with REQ IDs appended (04/06/08/09) |
| P-6 | Tooling note | ADR-012/-014/-026 are bullet-embedded (invisible to the heading-based scan) | Verified present and complete; no change required |

## 3. Findings from the independent adversarial review (F-series) — all fixed

Severity as reported by the reviewer. "Resolution" describes the applied amendment.

| ID | Sev | Finding (abbreviated; reviewer verified against text) | Resolution |
|---|---|---|---|
| F-1 | BLOCKER | Specified compaction algorithms (unconditional-store scalar compaction; full-vector LUT-compress stores) write past the "exact count" output capacity that REQ-MEM-008/REQ-API-011/06 §6 mandated — guard-page tests would fault the specified algorithms; the two specs were unsatisfiable together | Contract redefined: REQ-MEM-008 now distinguishes **capacity region** (n elements for count-returning compaction outputs) from **defined output region** (first *count*), with the write-bound argument (output cursor ≤ processed inputs ⇒ all stores within [0, n)) made normative; [06 §6](06-memory-model.md) table rebuilt with both columns (★ rows); [04](04-public-api.md) postconditions (K1/K2/K3) and REQ-API-011, [08 §2](08-kernel-design.md) invariants, [09 §6](09-simd-architecture.md) bound argument, and REQ-TEST-003/-006 (defined-region comparison; guard page at capacity boundary) all aligned |
| F-2 | MAJOR | Dispatch memory-ordering self-contradictory (release publish + "acquire-free relaxed reads" vs §6's own acquire requirement); relaxed-only is unsound on ARM (stale/null fn after epoch match) | Protocol fixed normatively in REQ-DISP-008 and [07 §6](07-runtime-dispatch.md): publish = `fn.store(relaxed)` then `fn_epoch.store(release)`; read = `fn_epoch.load(acquire)`, on match `fn.load(relaxed)`; REQ-DISP-003 budget corrected to 3 loads (one acquire); [05 §5](05-internal-architecture.md) aligned |
| F-3 | MAJOR | Charter §7.4's strict-order float recourse ("scalar reference doubles as the strict-order variant") did not exist: ADR-013 froze scalar float sums at A=8 blocked accumulation | ADR-013 amended: **scalar A=1 strict left-fold** (the charter recourse), explicit throughput sacrifice documented; corollary noted that autovec baselines are also strict-order (no FP reassociation without fast-math), which the K6 verdict block must state |
| F-4 | MAJOR | REQ-BENCH-004 validation against the scalar reference is unimplementable for float reductions (per-ISA results differ by design) while REQ-BENCH-015 forbids using testkit's policy oracle | REQ-BENCH-004 amended: float reductions validate against a **bench-local ADR-013 policy oracle**, duplicated by design and covered by the REQ-BENCH-015 drift-alarm conformance test; [05 §8](05-internal-architecture.md) responsibilities updated |
| F-5 | MAJOR | Charter §6.3/§13-item-2 compile-time pinning was never designed anywhere | New **REQ-DISP-013** (`QUIVER_PIN_ISA` semantics: static resolution, excluded upper backends, `active_isa()`/override behavior, debug-assert hardware check) added to [07 §2](07-runtime-dispatch.md); option added to REQ-BUILD-006; pinned consumption-test leg added to REQ-CI-008; M8 objective/files/REQs/tests updated (also resolves F-18); total REQ count now 235 |
| F-6 | MAJOR | `REVIEW_REPORT.md` referenced by README/00/22 did not exist; certification evidence unverifiable | This report; 22's stale "F-1..F-4" pointer corrected to this report's actual finding IDs |
| F-7 | MAJOR | AVX-512 masked-tail validation claim not discharged by any specified CI configuration (SDE ran release-only; blocking ASan job lacks AVX-512 silicon); REQ-MEM-001 lacked the observable-access language 09 attributed to it; ASan masked-intrinsic instrumentation is Clang-specific | REQ-MEM-001 gains the architectural observable-access clause; REQ-CI-004 gains leg (b): Clang ASan+UBSan asserts-on under SDE `-spr` (PR-sampled, nightly-full); REQ-SIMD-003 names that leg and the Clang-only rationale |
| F-8 | MAJOR | K6 `compute_sma` and `reduce_count_valid` lacked the charter-Appendix-A-required optional selection parameter | SelVec overloads added to API-K6-004/-005 with participation semantics |
| F-9 | MINOR | ADR-006's "exactly 10 concrete symbols per template API" wrong for two-parameter templates (dict_decode = 30) | Restated as one symbol per admissible template-parameter combination |
| F-10 | MINOR | Plain `autovec` variant undefined on x86 (inside the T7 verdict machinery) | REQ-BENCH-002/-010 now define the platform-dependent vocabulary and the three fixed verdict pairs |
| F-11 | MINOR | Two-batch K1 validity combination rule unstated | `valid(i) = a_valid(i) ∧ b_valid(i)` added to 04 K1 postconditions and 08 K1 semantics |
| F-12 | MINOR | Short-form ADRs missing labeled template fields (ADR-016: Context/Constraints; ADR-024: Problem/Constraints); 22 §8 overclaimed | Fields added to ADR-016 and ADR-024; 22 §8 claim corrected to the verifiable statement |
| F-13 | MINOR | No ADR for master-prompt-mandated areas: memory ownership, allocator strategy, threading model | Documented **waiver** added to [01 §5](01-traceability.md): all three are charter-fixed with no PRD-level alternatives (allocators are a permanent non-goal); the waiver is the recorded decision |
| F-14 | MINOR | Six families lacked API-spec examples (master prompt Part 7 triple) | Example blocks added for K3/K4/K6/K7/K8/K9 in [04](04-public-api.md); [14 §5](14-documentation.md) template item 6 now mandates the minimal/typical/edge triple per family page, sourced per REQ-DOC-006 |
| F-15 | MINOR | 18 §1/§3 cited the matrix at "01 §5" (the ADR index) instead of 01 §6 | Both anchors corrected |
| F-16 | MINOR | Feature-mask sentinel (0 = undetected) collided with the legitimate all-false detection result | REQ-DISP-007: bit 7 = detection-complete flag |
| F-17 | MINOR | SDE release-preset job assigned assert-dependent death-test suites with no skip mechanism | REQ-TEST-014: death tests self-skip (`GTEST_SKIP`) via an asserts-enabled introspection constant; REQ-CI-004 notes it; the sanitized leg (F-7) restores assert coverage under AVX-512 |
| F-18 | MINOR | "Dispatch hardening" promised for M8 (00 §6, 07 §13) had no work item in 18 M8 | Resolved by F-5: pinning is the M8 dispatch-hardening deliverable, with files/REQs/tests enumerated |

**Dimension D (scope creep): zero findings** — independently verified against Charter §8.2 and master prompt Part 13 §14 (no SQL/storage/networking/scheduler/allocator/string-type/SIMD-abstraction/engine surface; demo layer correctly bounded). The reviewer also independently confirmed clean: kernel-catalog consistency (00/04/08), version↔milestone mapping (00 §6/18/19 §4), the 73-file arithmetic, the 04↔06 aliasing allowlist, and the 18↔01 allocation reconciliation.

## 4. Residual accepted items (documented, not defects)

1. **Family-specific requirements (REQ-K1…K10) are defined in prose** inside [08 §5](08-kernel-design.md) family sections rather than as table rows. Accepted: each carries a stable ID and normative content at its definition site, and allocation/validation live in the [01 §6](01-traceability.md) matrix. Restating 33 rows would duplicate content the master prompt's own table guidance discourages.
2. **Sanitized AVX-512 coverage is Clang-only** (GCC ASan's handling of masked intrinsics is not relied upon). Documented in REQ-SIMD-003/REQ-CI-004; risk register R-01 covers compiler diversity for correctness via the differential matrix.
3. **REQ-CI-012's 25-minute PR budget and REQ-DISP-003's <2 ns figure are targets, not gates** — both are explicitly marked informational in their chapters; gates reference behaviors, not timings (Survey §7.3 discipline).
4. **The reviewer's finding F-1 fix widens caller capacity requirements** for K1-selvec/K2-bitmap/K3-to-selvec from "count" to "n". This matches actual engine practice (selection buffers are batch-sized) and the charter's safe-bound framing (Charter §7.2 "capacity contracts documented"); no charter conflict.

## 5. Certification

- **Missing requirements:** none known after F-5/F-8/F-14 resolutions; every master-prompt module/API/milestone field is covered directly or by a documented inheritance convention; the three mandated-ADR gaps are closed by a documented waiver (F-13).
- **Ambiguous sections:** the identified ambiguities (F-2, F-4, F-10, F-11, F-17) are resolved with normative text; the stop-and-report protocol (REQ-META-004) governs any residual ambiguity discovered in-flight.
- **Contradictions:** all identified (P-2, P-4, P-5, F-1, F-2, F-3, F-6, F-9, F-15, F-18) are resolved; the post-fix sweep found no stale contradictory text.
- **Scope creep:** none found (two independent checks).
- **Untraceable decisions:** none — 26 ADRs + documented waiver; 235/235 requirement IDs in the master matrix exactly once, each with validation, milestone, and release mapping.
- **Implementation risks:** carried in [20-risk-register.md](20-risk-register.md) (R-01…R-16) with mitigations that are existing REQs/ADRs; the technically-load-bearing risks flagged by the reviewer (dispatch ordering, masked-tail sanitizer coverage, compaction write bounds) are now specified, not assumed.
- **Sections requiring revision:** none open. All revisions demanded by the review have been applied and re-verified.

**The PRD is certified implementation-ready.** The formal approval statement, including the standing amendment rule, is in [22-final-review-checklist.md](22-final-review-checklist.md). Implementation shall not begin until separately authorized, and shall treat this report as part of the PRD's audit trail.
