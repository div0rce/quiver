# Quiver — Design Charter

**Product definition for a dependency-free C++23 library of vectorized analytical kernels, with a public cross-ISA performance ledger**

Charter version 1.0 — July 2026.
Pipeline position: *Literature Review → Opportunity Analysis → **Design Charter** → Engineering PRD → Implementation.*

Upstream documents, cited throughout as "Survey §n" and "OA §n":
- *Vectorized Analytical Execution Engines: A Design-Space Survey* (July 2026)
- *Open-Source Opportunity Analysis* (July 2026)

---

## 0. Document contract

**What this document does.** It fixes Quiver's identity: the problem, the users, the adoption argument, the product tenets, the v1 scope, the public API boundaries and semantic contracts, the explicit non-goals, the success criteria, and the governance model. Once accepted, these are binding on the Engineering PRD and on implementation. The PRD translates this charter into architecture; it does not relitigate it.

**What this document does not do.** No architecture, no function signatures, no dispatch mechanism, no build-system design, no CI topology, no milestone-by-milestone plan. Those belong to the PRD (§14 lists the explicit handoff).

**Labels.** Decisions in this document are marked:

| Label | Meaning |
|---|---|
| **[DECISION]** | Binding product decision. The PRD implements it; changing it requires a charter amendment. |
| **[DEFERRED]** | Explicitly delegated to the PRD or later. Listed so its absence here is not read as an oversight. |
| **[EVIDENCE]** | Pointer to the upstream document that justifies a decision. |

**Change control.** Amendments increment the charter version and record what changed and why. Any scope addition must pass the Engine Test (§6, Tenet T1) and name the persona (§4) whose documented need it serves.

---

## 1. Identity

**Name.** **Quiver** — a quiver holds arrows: a set of sharp, interchangeable projectiles, deliberately Arrow-ecosystem-adjacent (OA §13). **[DECISION]** subject to one gate: a trademark/collision diligence check before public launch (an archived note-taking app and a math-visualization tool share the word; neither occupies the C++/data niche). Fallback names if diligence fails: `vkq`, `fletch`. C++ namespace: `quiver`.

**One-sentence definition.** Quiver is a dependency-free C++23 library of the ~10 vectorized kernels every analytical engine reimplements — predicate evaluation, filter/compaction, selection-vector and null-mask algebra, gather/dictionary decode, batch reductions, batch hashing, bit-unpacking, and overflow-safe batch arithmetic — hand-tuned per ISA (scalar, AVX2, NEON, AVX-512), runtime-dispatched, and shipped with a public, PMU-instrumented, statistically rigorous performance ledger across microarchitectures.

**Mission.** Make the hot inner loops of columnar analytics a measured, portable, importable commons — so that no engine, student, or researcher has to reimplement or guess at them again.

**Positioning in one line.** Quiver is to analytical engines what simdjson is to JSON parsers: not a competitor to the systems that need it, but the component they vendor. **[EVIDENCE]** OA §3 — the simdjson/CRoaring/StringZilla adoption path is the validated route for solo-authored kernel libraries into production engines.

**Form.** One repository, two products **[DECISION]**:
1. **The library** — the kernels, their vocabulary types, and runtime dispatch.
2. **The ledger** — the published, reproducible record of what those kernels cost on current CPUs, per ISA and microarchitecture.

The ledger is not marketing collateral for the library; it is a co-equal deliverable with its own success criteria (§10), its own consumers (researchers, per §4), and its own citable identity (Survey §11.4 #6 names it as a standing research gap).

---

## 2. The problem

**The reimplementation tax.** Every serious analytical engine independently implements the same small set of vectorized primitives: DuckDB, Velox, ClickHouse, and Arrow each carry internal filter/compaction, selection handling, dictionary decode, batch hashing, and null-mask code, converging on the same "~10 hottest kernels" pattern (Survey §4.5). This code is engine-internal plumbing: license-entangled, coupled to each engine's vector formats, untested outside its host, and unavailable to anyone building anything else. The only standalone attempt, `simdprune`, is x86-only and dormant at 68 stars (OA §2). The niche is verified empty.

**The evidence vacuum.** There is no public, cross-ISA, per-kernel accounting of what these primitives cost — the "honest SIMD ledger" the survey flags as an open research gap (Survey §11.4 #6). Adjacent unknowns compound it: whether bitmaps or selection vectors are the better intermediate representation is unresolved and ISA-dependent (Survey §11.3 #3); the field's execution-grain constants date to 2005–2014 calibrations (Survey §11.3 #1). Engine builders currently decide these questions by folklore or by reading each other's source.

**Why the gap persists.** Engines treat kernels as internal plumbing; SIMD library authors treat *abstraction* (Highway, xsimd, `std::simd`) as the product, not database-shaped kernels; the intersection — selection-vector-semantic, per-ISA, dependency-light kernels — has no owner (OA §2).

**Why now.**
- C++26 finalized `std::simd` (March 2026), permanently standardizing the *abstraction* layer away — which sharpens, rather than threatens, the unclaimed *kernel* layer (OA §5, C2 red team).
- ARM is no longer a port target but a co-equal deployment platform (Apple Silicon development machines, Graviton fleets); NEON parity is now mandatory for any credible kernel claim (Survey §4.1).
- AVX-512's economics are settled: Zen 4/5 proved the ISA (masks, compress) matters more than the width, with no downclocking penalty (Survey §4.1) — the payoff for per-ISA hand-tuning is durable, not a Skylake-era artifact.
- The ecosystem consolidated into a few funded engines plus two reusable-library ecosystems, and all of them vendor best-in-class narrow libraries rather than write their own (OA §1.3, §3).

---

## 3. Who Quiver is for

Four personas, in priority order. Each entry: who they are, the job they hire Quiver for, and what they will judge it on.

**P1 — Engine builders.** Teams building or maintaining analytical engines, established (ClickHouse-style vendorers of simdjson/CRoaring) or new (the next embedded engine, stream processor, or observability backend). *Job:* stop reimplementing and re-debugging the same ten inner loops; get NEON parity without hiring for it. *Judged on:* per-kernel throughput vs their internals, vendorability (can it live in `contrib/` untouched for two years), sanitizer cleanliness, API stability.

**P2 — C++ systems engineers outside databases.** HFT/quant data-pipeline engineers, telemetry and observability agents, feature-engineering services, embedded/edge analytics — anyone who needs to filter, compact, hash, and reduce columnar batches in C++ without adopting all of Velox (the documented integration burden, OA §1.2) or embedding a whole DBMS. *Job:* analytical primitives as a small linkable dependency. *Judged on:* zero dependencies, allocation-free hot paths, usability with exceptions disabled, tail-latency-honest documentation.

**P3 — Performance researchers.** Academics and industrial researchers who need credible, reproducible baselines for kernel-level claims (the audience of DaMoN/ADMS; cf. the ADMS 2023 Velox SIMD study, Survey §4.5). *Job:* cite the ledger instead of hand-rolling baselines; run the harness on new hardware. *Judged on:* methodology rigor (Survey §7 practices as code), machine-readable results, willingness to publish losses.

**P4 — Educators and students.** Instructors and self-teachers in the 15-721 tradition. *Job:* readable scalar references beside real per-ISA implementations of the same kernel — the missing public artifact between textbook pseudocode and engine source. *Judged on:* readability of the scalar reference, per-kernel documentation quality.

**Anti-personas** — people Quiver deliberately does not serve, with redirects:
- Anyone who wants to run SQL or DataFrame operations → DuckDB, Polars, DataFusion.
- Rust users → arrow-rs kernels, Vortex (OA §2: the Rust lane is occupied; Quiver is the C++ lane).
- Users seeking a general-purpose portable SIMD API → Highway, `std::simd`.
- Anyone needing an end-to-end query pipeline with memory governance, spilling, or scheduling → an engine, not a kernel library.

---

## 4. The adoption case: why not write it yourself?

The default competitor is not another library — it is each team's own `simd_utils.h`. The charter must state why importing Quiver beats writing it, because that argument drives every scope and contract decision below.

**1. The correctness matrix is the real cost.** The kernel logic is the easy 20%. The expensive 80% is the test matrix: {ISA} × {input alignment} × {batch length incl. tails} × {selectivity 0–100%} × {null density} × {value distribution}, fuzzed against a golden reference, sanitizer-clean, on hardware you don't own. Quiver ships that matrix as CI. A team vendoring Quiver inherits thousands of machine-hours of validation for free; a team writing its own inherits the bugs.

**2. The microarchitectural folklore is embedded, sourced, and current.** Concrete examples the survey verified: `vpcompress` directly to memory is microcoded and slow on Zen 4 — compress to register, then store (Survey §4.1); NEON has no movemask — the `shrn`/`addv` idioms substitute (Survey §4.1); Apple Silicon wants 128-byte padding granularity and ≥4 independent 128-bit ops in flight (Survey §3.1, §4.1); gathers rarely beat well-scheduled scalar loads out of cache (Survey §4.2); branchless kernels beat branchy ones except at extreme selectivities (Survey §3.4). Each such fact is a bug or a 2× someone else already paid for. Quiver is the place they are written down *as code with a benchmark attached*.

**3. The ledger converts claims into evidence.** No internal `simd_utils.h` comes with published, per-microarchitecture, statistically defensible numbers against an honest auto-vectorized baseline. Quiver's does — including the cases where the explicit SIMD variant *loses* (§7.5). That is a capability a solo team cannot cheaply replicate and the reason researchers (P3) become distribution channels.

**4. Maintenance across ISA churn is amortized.** New microarchitectures arrive yearly; each is a new ledger entry and occasionally a new tuning. One maintained commons absorbs that churn once instead of N times.

**Why not the alternatives:**
- **Highway / xsimd / EVE / `std::simd`** — abstractions over lanes, not kernels with selection-vector semantics. Precisely the operations analytical kernels live on (compress, movemask idioms, cross-lane shuffles, conflict detection) are where lowest-common-denominator APIs emulate expensively or abstain (Survey §4.5). They are substrates one *could* build kernels on; nobody has shipped the kernels. **[DECISION]** Quiver does not build on any of them: per-ISA implementations are first-party and dependency-free. Rationale: (a) the zero-dependency pledge is load-bearing for vendorability; (b) hand-per-ISA control is required to embody the folklore above; (c) the ledger's credibility — and the portfolio value (OA §11) — rests on the implementations being Quiver's own.
- **Velox** — a whole execution library with heavy, brittle builds and Meta-gated governance (OA §1.2). Its `SimdUtil` layer is exactly the shape of thing Quiver externalizes, minus the monorepo.
- **Copy-paste from DuckDB/ClickHouse** — license entanglement, coupling to their vector formats, zero tests outside the host engine, no upgrade path.

---

## 5. Product tenets

Eight tenets. Each is binding and comes with a test the PRD and every future PR can be checked against.

**T1 — Kernels, not an engine.** Quiver computes over caller-owned memory and returns. It never owns a pipeline, a plan, or a policy. *The Engine Test:* if a proposed feature requires Quiver to know about schemas, logical types beyond fixed-width primitives, query semantics, memory ownership, thread scheduling, or I/O — it is out of scope, permanently. This tenet exists because scope discipline is C2's single biggest identified risk (OA §5).

**T2 — Every number is reproducible.** No performance claim ships without a public harness invocation that regenerates it, an environment manifest, and a disclosed statistics policy (§7.5). Methodology follows Survey §7 (DBTest 2018 + Berger-school practice) as code, not as aspiration. *Test:* can a stranger with the named hardware reproduce the number from the repo alone?

**T3 — The scalar reference is the specification.** Every kernel exists first as a readable, obviously-correct scalar implementation. ISA variants must match it bit-for-bit for integer kernels, and per a documented divergence policy for floats (§8.4). The scalar form is a first-class public deliverable (persona P4), never a stub. *Test:* could a student learn the kernel's semantics from the scalar file alone?

**T4 — Vendorable above all.** Zero external dependencies in the shipped library. No mandatory build-system lock-in: consumable via standard CMake, via FetchContent, and via a single-amalgamation drop-in (the simdjson/fsst path that DuckDB and CedarDB demonstrably accept — OA §8). Tests and benchmarks are excluded from the vendored surface. *Test:* can a C++23 project adopt Quiver by copying two files?

**T5 — Cross-ISA parity is a feature, not a port.** NEON is a launch requirement co-equal with AVX2, not a follow-up (`simdprune`'s x86-only scope is a named cause of its failure — OA §3). No kernel graduates to stable with an ISA gap in its tier. Layouts and APIs never bake in a vector width (the SVE lesson, Survey §4.1). *Test:* does the kernel's Apple Silicon number appear in the ledger the day it ships?

**T6 — The API is data-shape honest.** Kernels operate on caller-owned contiguous buffers plus explicit selection/validity structures. Non-owning views everywhere; no allocation, locking, I/O, or global mutable state (beyond one-time dispatch initialization) inside any kernel; caller provides output buffers with documented capacity contracts. *Test:* is every kernel a pure function of its arguments?

**T7 — Honest performance culture.** Every kernel page shows the auto-vectorized scalar baseline compiled with identical flags. Where the explicit SIMD variant fails to beat it — an outcome the literature says will happen for some kernels (Survey §4.5: ~4% end-to-end from all of Velox's explicit SIMD) — the ledger says so in the headline, not a footnote. Losses are publishable results; they are the ledger's credibility. *Test:* does the repo contain at least one documented case where Quiver recommends the compiler over its own intrinsics?

**T8 — Boring on purpose everywhere except the kernels.** Novelty budget is spent inside the inner loops and the methodology. Everything else — naming, versioning, docs structure, contribution flow — follows the most conventional path available. *Test:* would a maintainer of simdjson or CRoaring find anything surprising outside `src/kernels/`?

---

## 6. v1 scope

### 6.1 The kernel catalog **[DECISION]**

Ten kernel families, tiered. Tier A must be complete (scalar + AVX2 + NEON, ledger entries, fuzzed) for the first public release milestone; Tiers A+B with AVX-512 constitute v1.0. Canonical semantics in Appendix A.

| # | Family | Tier | One-line semantics |
|---|---|---|---|
| K1 | `compare` | A | Predicate over a batch (vs scalar or vs batch) → bitmap or selection vector |
| K2 | `filter` | A | Apply a selection to a value batch → densely compacted output |
| K3 | `sel_convert` | A | Bitmap ↔ selection vector, both directions, with cardinality |
| K4 | `mask_algebra` | A | AND / OR / ANDNOT / XOR / NOT over bitmaps; popcount; all/any/none; validity combination for null propagation |
| K5 | `take` / `dict_decode` | A | Gather values by index vector; dictionary decode as its special case; optional selection fusion |
| K6 | `reduce` | A | min / max / sum / count-valid over a batch, with optional validity mask and selection; one-pass SMA (min + max + null count) construction |
| K7 | `hash` | B | Batch hashing of fixed-width keys → 64-bit hashes; multi-column hash-combine; cross-ISA-identical output |
| K8 | `unpack` | B | Bit-unpack 1–64-bit packed integers to 8/16/32/64-bit outputs; frame-of-reference add fusion |
| K9 | `arith` | B | Elementwise add / sub / mul with validity propagation (wrapping semantics, explicitly named) |
| K10 | `arith_guarded` | B | Overflow-checked and saturating variants of K9; overflow reporting, never silent UB |

Flagged v1.x extensions *within* these families (not v1.0-blocking): string hashing over (pointer, length) views in K7; fused unpack-plus-predicate in K8 (the predicate-pushdown-on-packed-data primitive, gateway to the compressed-execution expansion in §9.1).

This catalog is closed for v1 **[DECISION]**: new families require a charter amendment. It maps one-to-one onto the primitive list the Opportunity Analysis identified as the reimplementation commons (OA §4 C2, §13).

### 6.2 Type and representation coverage **[DECISION]**

- **Element types:** `int8/16/32/64`, `uint8/16/32/64`, `float32/64`. Nothing else in v1 — no decimals, no dates (they are integers to a kernel library), no owning string type.
- **Validity bitmaps:** Arrow-compatible — bit-packed, LSB-first bit order, 1 = valid (Survey §2.6). Rationale: Arrow is the settled boundary format; Arrow-native systems (DataFusion embedders' C++ siblings, Acero users, anyone at an Arrow boundary) adopt Quiver's masks at zero conversion cost, and non-Arrow engines pay nothing for the choice.
- **Selection vectors:** arrays of `uint32` indices. Rationale: batch-local indices are bounded far below 2³²; 64-bit indices would double the bandwidth of the most-trafficked intermediate for no benefit. For selection semantics, indices are sorted and unique; `take` (K5) additionally accepts arbitrary (unsorted, duplicated) index vectors — the two contracts are documented separately.
- **Dual selection representation is a feature, not indecision** **[DECISION]**: bitmaps and selection vectors are both first-class across the API. Which representation wins is an open, ISA-dependent research question (Survey §11.3 #3 — AVX-512 compress favors bitmaps, NEON favors index vectors); Quiver's ledger is the instrument that answers it, and the K3 conversion kernels make the answer actionable. The bitmap-vs-selection-vector study is a named v1 deliverable (§10).
- **Batch sizes:** caller-chosen; no batch constant is baked into the API (the "right execution grain on 2026 hardware" is an open question — Survey §11.3 #1). The tested and ledger-swept envelope is 256–65,536 elements, centered on the cache-resident regime (Survey §1.4).

### 6.3 ISA matrix **[DECISION]**

| Target | Status in v1 |
|---|---|
| Scalar (portable C++23) | Always present; the reference and the fallback; compiled with auto-vectorization as the honest baseline (T7) |
| AVX2 | Tier-1 from first release |
| NEON (ARMv8) | Tier-1 from first release, co-equal with AVX2 (T5) |
| AVX-512 (with masks/compress) | Tier-1 at v1.0; the K2/K3 compress paths are its showcase (Survey §4.1) |
| SVE2 | Exploratory branch only; never v1-blocking. Rationale: thin and *narrowing* availability — Graviton 4 regressed to 128-bit, Apple ships none (Survey §4.1) |

Runtime dispatch with a conservative baseline (the ClickHouse model — Survey §2.3, §9 #11) plus compile-time pinning for embedders who build per-target. Dispatch mechanism: **[DEFERRED]** to PRD.

### 6.4 The ledger **[DECISION]**

The ledger is a versioned, machine-readable dataset plus human-readable per-kernel pages.

**Each entry records:** kernel × variant (scalar / auto-vec / per-ISA) × microarchitecture × input configuration → throughput (values/s and bytes/s), cycles per value, and (Linux) PMU counters: instructions, IPC, branch misses, L1d/LLC misses, dTLB misses (Survey §7.2).

**Input configuration axes swept:** batch size (256–65,536); selectivity {1, 10, 50, 90, 99}% with both uniform-random and clustered selection patterns (cyclic patterns wildly overstate predictability — Survey §3.4); null density {0, 1, 10, 50}%; value distributions for K7 (sequential, uniform, skewed); aligned vs deliberately misaligned inputs.

**Statistics policy (per Survey §7.4):** ≥10 process-level repetitions; report median and min with nonparametric 95% CIs plus CV; investigate any CV above a few percent; never a single run, never averaged across distributions.

**Environment manifest per result:** CPU model and microarchitecture, frequency governor and turbo state, SMT state, ASLR state, compiler and exact flags, allocator, kernel version. Random interleaving of repetitions where the harness supports it (Survey §7.1).

**Microarchitecture coverage at v1.0:** Zen 4 or 5, a Golden Cove-class Intel core, Apple M-series; Graviton stretch goal. Apple entries carry no PMU columns and are labeled secondary per the platform's tooling reality (Survey §7.3) — the limitation is documented, not hidden.

**The publish-losses pledge (T7):** every kernel page leads with the comparison against the auto-vectorized baseline; explicit-SIMD losses are reported as prominently as wins.

**Reproduction:** one documented command regenerates any entry on matching hardware. Disputes are handled via an issue template requiring a harness run (§13).

### 6.5 Packaging and distribution (product requirements) **[DECISION]**

Consumable three ways from first release: standard CMake package, CMake FetchContent, single-file-pair amalgamation. Listed on vcpkg and Conan by the 18-month horizon (OA §14). No mandatory dependencies at any point; test/benchmark dependencies (e.g., a benchmark harness) never leak into the consumable surface. Mechanisms: **[DEFERRED]** to PRD.

### 6.6 The demo layer **[DECISION]**

A thin set of composition examples — filter → take → reduce pipelines over synthetic columnar data — used for end-to-end benchmark composition and documentation. This layer preserves the educational arc of the original engine idea (OA §12) without shipping an engine: it lives in `examples/` and the benchmark tree, is explicitly excluded from the public library target and from API stability guarantees, and is bound by the Engine Test. If the demo layer ever needs a schema, an operator interface, or a scheduler, it has violated T1 and gets cut, not grown.

### 6.7 Documentation as a deliverable **[DECISION]**

Per-kernel documentation page: semantics (contract, preconditions, capacity formulas), the scalar reference inline or linked, per-ISA implementation notes (which microarchitectural facts shaped it, with Survey-style sourcing), and the current ledger excerpt. Docs are the marketing (OA §10); they ship with the kernel, not after it.

---

## 7. Public API boundaries

### 7.1 The four public surfaces **[DECISION]**

| Surface | Contents | Stability |
|---|---|---|
| **A — Kernels** | The K1–K10 entry points | Frozen at v1.0; semver thereafter |
| **B — Vocabulary types** | Non-owning views: value-batch spans, validity-bitmap view, selection-vector view, SMA result struct | Frozen at v1.0 |
| **C — Dispatch & introspection** | Query detected/selected ISA; override selection; library version and feature flags | Frozen at v1.0 |
| **D — Ledger schema** | The machine-readable results format + environment manifest schema | Versioned independently of the library |

**Internal, explicitly unstable:** per-ISA implementation namespaces, the benchmark harness, tuning constants, the demo layer. Nothing in these carries compatibility promises; the PRD gives them a naming convention that makes instability obvious.

### 7.2 Memory contract **[DECISION]**

- All inputs and outputs are caller-owned; Quiver allocates nothing in any kernel path (T6).
- Every kernel documents its output-capacity formula (e.g., K2's output needs capacity ≥ selection cardinality; K3's selection output needs capacity ≥ popcount).
- **Bounds discipline:** the default contract is that kernels never read or write outside `[ptr, ptr + len)` — sanitizer-clean by construction, because target adopters run ASan/MSan in CI and a library that trips them is unvendorable. Where over-reading padded buffers buys material speed (the ClickHouse PODArray pattern — Survey §2.3), Quiver may ship separately named `*_padded` variants whose slack requirements are part of their documented contract, and only where the ledger shows the win. Padding is never an implicit assumption.
- Alignment: all kernels accept unaligned inputs correctly; the ledger reports the aligned/unaligned performance delta so callers can decide whether to align.

### 7.3 Execution contract **[DECISION]**

- Kernels are reentrant pure functions; concurrent invocation over disjoint outputs is safe by construction. Quiver never creates threads and never synchronizes (T1, T6).
- Usable with exceptions and RTTI disabled: no kernel throws; contract violations are documented preconditions, checked by debug-mode assertions, and fuzzing enforces that no input satisfying the preconditions triggers UB. (P2's HFT segment compiles `-fno-exceptions`; this is an adoption gate, not a style preference.)
- No syscalls, no I/O, no logging from kernels.

### 7.4 Determinism contracts **[DECISION]**

- **Integer kernels:** bit-identical results across all ISAs and platforms. No exceptions.
- **Float reductions (K6):** bit-identical per (library version, ISA) under a documented reassociation policy — multiple accumulators are required for throughput (Survey §3.9), so strict left-fold cannot be the default; the scalar reference doubles as the strict-order variant for callers who need it. The engine adopts an explicit reassociation policy rather than leaning on `-ffast-math` (Survey §3.9) — that survey prescription is now a contract.
- **Hashing (K7):** output is identical across ISAs and platforms and stable within a major version — engines partition and shuffle on these hashes, so cross-platform divergence is data corruption, not a quirk. Documented as non-cryptographic. Algorithm choice: **[DEFERRED]** to PRD, inside this contract.
- **Overflow (K6 sum, K9/K10):** no released kernel has silent-UB overflow behavior. Every arithmetic kernel's overflow semantics (wrapping / checked / saturating) is in its name and contract.

### 7.5 Versioning **[DECISION]**

Semantic versioning. 0.x during Tier A/B development with breaking changes allowed and changelogged. v1.0 freezes surfaces A–C for the ten families. The ledger schema (surface D) versions independently so new hardware and new counters never force a library release.

### 7.6 Language and bindings **[DECISION]**

- **C++23 is the language floor.** Acknowledged tradeoff: this excludes C++11-pinned codebases (DuckDB core) from direct vendoring. Accepted because the project's identity, portfolio thesis (OA §11), and primary adopter set (new engines, Velox-class C++17+ systems, ClickHouse-class C++23 codebases, HFT shops on current toolchains) all track recent standards, and because a C-ABI shim is the correct future escape hatch for everyone else.
- C ABI shim, Rust/Python bindings: **[DEFERRED]**; community bindings are welcomed but not core-maintained (they multiply the maintenance surface a solo project cannot carry — OA §10).

---

## 8. Explicit non-goals

### 8.1 Deferred — out of v1, revisit deliberately

| Item | Revisit condition |
|---|---|
| Compressed-predicate kernels (predicates over FOR/RLE/dictionary data without decompression — OA candidate C9) | The designated v2 expansion; gated on FastLanes/format stabilization (OA §5). K8's fused-predicate variant is the deliberate doorstep. |
| String kernels beyond (ptr, len) hashing | After v1.0; consuming views only — never an owning string type |
| SVE2 production support | If server-ARM VL economics stabilize (Survey §4.1) |
| GPU anything | Out of the 12–18-month thesis entirely (OA §13); a v2+ question at most |
| C ABI / bindings | Post-v1.0, demand-driven |
| Windows/MSVC as tier-1 | Tier-2 best-effort in v1 (OA §10 names MSVC "the usual pain"); promote on demonstrated demand |

### 8.2 Permanent — never, with reasons and redirects

| Never | Why | Redirect |
|---|---|---|
| SQL, expression language, planner, optimizer | Engine organs; violates T1 | DuckDB, DataFusion |
| Scheduler, thread pool, morsel dispatcher | Trust-gated engine organ; occupied space (OA §5 C5) | oneTBB, Taskflow, the host engine |
| Memory allocator, buffer manager, spilling | Engine organ (Survey §5.2: the buffer manager *is* the engine's allocator) | jemalloc/mimalloc, the host engine |
| Storage or file format | Format wars are someone else's business (OA §13) | Arrow, Parquet, Vortex |
| Network, distribution, IPC | Not a kernel concern | Arrow Flight, the host system |
| General SIMD abstraction API | Crowded and standardized away (OA §2); Quiver ships kernels, not lanes | Highway, `std::simd` |
| An owning string type | Adjacent occupied ground (StringZilla; German-strings designs are public commons — OA §5 C8) | CedarDB blog's design, Arrow Utf8View |
| Becoming an engine | The founding refusal (OA §12: C1 ranked 12th of 13) | — |

The permanent list is the Engine Test in table form. It is the charter's most important section for the PRD: any architecture that makes one of these rows easy to add later is the wrong architecture.

---

## 9. Success criteria and decision gates

Adopted from OA §14, restated as the charter's binding scorecard. Base rates calibrated to SimSIMD/CRoaring/fsst trajectories, not simdjson's outlier curve.

### 9.1 Milestone criteria

| Horizon | Criterion | Target | Stretch |
|---|---|---|---|
| 6 mo | Tier A complete (scalar + AVX2 + NEON, fuzzed, documented) | 6 families | + K7, K8 |
| 6 mo | Ledger v1 live (≥3 µarchs: Zen, Golden Cove-class, M-series) | yes | + Graviton |
| 6 mo | GitHub stars | 150 | 500 |
| 12 mo | Stars / external contributors (merged) | 600 / 3 | 1,500 / 10 |
| 12 mo | Workshop paper (DaMoN/ADMS) from the ledger, incl. the bitmap-vs-selection-vector study | submitted | accepted |
| 12 mo | Talk (CppCon/CppNow/ACCU or DB meetup) | 1 accepted | 2 |
| 18 mo | v1.0: Tiers A+B frozen, AVX-512 + dispatch complete, ledger ≥5 µarchs | yes | + SVE2 exploratory |
| 18 mo | Named external project vendoring/importing Quiver | 1 | 3 (one an engine >1k★) |
| 18 mo | vcpkg + Conan listed; downloads | 1k/mo | 10k/mo |
| 18 mo | Citations (paper or engineering blog) | 2 | 10 |

### 9.2 Standing quality gates (always-on, non-negotiable)

- 100% of published performance claims reproducible from the public harness (T2).
- Every benchmark dispute answered publicly with a reproducible run (OA §14).
- Zero known UB; sanitizer-clean releases; fuzzing in CI for every stable kernel.
- No ISA gap within a shipped tier (T5).

### 9.3 Pre-registered failure and pivot gates

Per the survey's own methodology ethos, failure is defined in advance (OA §14):

- **12-month gate:** if stars < 200 *and* external users = 0 *and* the ledger has attracted no engagement (no reproductions, citations, or substantive disputes), then either (a) pivot the roadmap to the compressed-kernel expansion (§8.1) if format stabilization has created a pull signal, or (b) conclude the project with a technical report. The skills demonstrated remain fully portfolio-valid in either branch.
- **6-month checkpoint (soft):** if Tier A slips badly, cut scope by kernel family — never by ISA parity, never by ledger rigor. The 9-month gracefully-shrunk version (six families, two ISAs + scalar, honest ledger) is pre-authorized as releasable (OA §10).

---

## 10. Product risks and mitigations

Product-level only; engineering risks belong to the PRD.

| Risk | Mitigation baked into this charter |
|---|---|
| Engines' NIH culture: they vendor, not depend | Design *for* vendoring: amalgamation, Apache-2.0, zero deps (T4) — the exact path simdjson took into ClickHouse (OA §3) |
| `std::simd`/Highway absorb the niche | They are abstractions; Quiver is kernels with selection semantics. Nothing in their roadmaps targets this (OA §5). The moat is the ledger + the folklore, not the lane arithmetic |
| Ledger invites adversarial scrutiny (ClickHouse will benchmark it "within a day" — OA §11) | That is the plan: methodology-as-moat (T2, T7); dispute policy answers with harness runs; losses published first by us, not discovered by them |
| Scope creep toward an engine | T1's Engine Test + the closed v1 catalog (§6.1) + the permanent-never table (§8.2) |
| Solo bus factor | Mechanical test matrix, docs-with-kernels, conventional everything (T8) — the repo must be forkable-in-anger by a stranger |
| Star growth stalls at SimSIMD scale (~2k) | Pre-accepted as a success-adjacent outcome: the ledger, paper, and hiring signal survive it (OA §5) |
| Name collision | Diligence gate pre-launch (§1); fallbacks reserved |
| ARM/Apple CI and PMU access | Coverage commitment scoped to what CI can honestly provide; Apple entries labeled secondary with the limitation documented (§6.4, Survey §7.3) |

---

## 11. Positioning summary

| Against | One-line differentiation |
|---|---|
| Highway / xsimd / EVE / `std::simd` | They sell portable lanes; Quiver sells finished, measured, database-shaped kernels |
| Velox | Velox is an engine you embed; Quiver is a file you vendor |
| DuckDB / ClickHouse internals | Their kernels are excellent and unexportable; Quiver is the exportable commons |
| simdprune | The dormant proof of demand; Quiver is what it needed to be: cross-ISA, maintained, evidenced |
| FastPFOR / streamvbyte | Codecs without predicates or selection semantics; Quiver is the execution layer beside them |
| arrow-rs / Vortex | The Rust lane, already served; Quiver is the empty C++ lane |

**Message house.** Tagline: *the kernels every engine rewrites, written once and measured everywhere.* Three proof points: (1) six-then-ten kernel families at parity on x86 and ARM; (2) the only public cross-ISA analytical-kernel ledger, reproducible to the flag; (3) vendorable in two files with zero dependencies.

---

## 12. Governance, license, and community

- **License: Apache-2.0.** **[DECISION]** Rationale: explicit patent grant, and it is the precedent license of the exact adoption path — simdjson and CRoaring are vendored into ClickHouse/Doris/StarRocks under it (OA §3). MIT was considered and rejected on patent-clarity grounds; dual-licensing adds process for no identified adopter need.
- **Contribution model:** DCO sign-off, no CLA. Solo maintainer at launch; the 12-month contributor target (§9.1) is a governance goal, not just a vanity metric — per-kernel modularity (independent shippability, OA §10) is what makes external contribution tractable.
- **Benchmark-dispute policy:** a standing issue template requiring hardware manifest + harness invocation; all disputes resolved in public with reproducible runs (OA §14: 100%).
- **Brand:** the naming diligence gate (§1) is a launch blocker, owned by the maintainer.

---

## 13. What this charter binds — and what the PRD must now decide

**Bound by this charter:** the name and namespace; the two-product form (library + ledger); the four personas and the anti-personas; the eight tenets; the ten-family closed catalog and its tiers; type coverage, Arrow-compatible bitmaps, `uint32` selection vectors, dual selection representation; the ISA matrix and SVE2's non-blocking status; the ledger's content, sweep axes, statistics policy, and publish-losses pledge; the four public surfaces and the memory/execution/determinism contracts; Apache-2.0; C++23 floor; the deferred and permanent non-goals; the success scorecard and pre-registered pivot gates.

**Explicitly handed to the Engineering PRD:**

1. Physical architecture: header-only vs compiled core + amalgamation generation; file/namespace layout; how per-ISA translation units are organized.
2. Runtime dispatch mechanism and compile-time pinning design.
3. Exact function signatures, view-type designs, and tail-handling conventions inside the §7 contracts.
4. Hash algorithm selection (inside §7.4's stability contract).
5. Testing architecture: golden-reference harness, fuzzing strategy, sanitizer matrix, CI topology across x86/Graviton/Apple runners.
6. Benchmark harness architecture: statistics implementation, PMU integration, environment-manifest capture, ledger data model (surface D schema).
7. Toolchain support matrix (GCC/Clang versions; MSVC tier-2 scope) and flag policy.
8. Milestone plan mapped to OA §10's M1–M15 skeleton, with the 9-month shrink point marked.
9. Documentation tooling and site.
10. Repository bootstrap: layout, CONTRIBUTING, issue templates (including the dispute template), release process.

The PRD treats every §13 "bound" item as fixed and every numbered item above as its work. Where the PRD discovers a genuine conflict between an architecture necessity and a charter decision, the resolution is a versioned charter amendment — never a silent divergence.

---

## Appendix A — Canonical kernel family definitions

Normative semantics for §6.1. Signatures are the PRD's job; these contracts are not.

| Family | Inputs | Outputs | Contract notes |
|---|---|---|---|
| K1 `compare` | value batch; comparand (scalar or batch); op ∈ {eq, ne, lt, le, gt, ge, between} | bitmap **or** selection vector | Branch-free by default; both output representations available (§6.2). NULL handling via optional input validity mask: invalid lanes compare to "not selected" |
| K2 `filter` | value batch; selection (bitmap or selvec) | densely packed values; count | Output capacity ≥ selection cardinality. Order-preserving |
| K3 `sel_convert` | bitmap ↔ selection vector | the other representation; cardinality | Round-trip is lossless. Selvec output is sorted unique by construction |
| K4 `mask_algebra` | 1–2 bitmaps | bitmap; or scalar (popcount / all / any / none) | Arrow bit order (§6.2). Defined for arbitrary batch lengths including non-multiple-of-8 tails |
| K5 `take` / `dict_decode` | value (or dictionary) batch; index vector (or codes); optional selection | gathered values | `take` permits unsorted/duplicate indices; all indices must be in-bounds (documented precondition, debug-asserted). Decode-with-selection touches only selected positions |
| K6 `reduce` | value batch; optional validity; optional selection; op ∈ {min, max, sum, count_valid, sma} | scalar result / SMA struct | Sum variants named by overflow semantics (§7.4). Float policy per §7.4. SMA = min + max + null count in one pass |
| K7 `hash` | fixed-width key batch(es); v1.x: (ptr, len) string views | 64-bit hash batch | Cross-ISA/platform identical; stable per major version; non-cryptographic (§7.4). Hash-combine defined for multi-column keys |
| K8 `unpack` | bit-packed buffer; bit width 1–64; count; optional FOR base | widened integer batch | v1.x: fused predicate variant emitting bitmap/selvec directly |
| K9 `arith` | two value batches (or batch + scalar); optional validity masks | value batch; combined validity | Wrapping semantics, explicit in the name. Validity propagation: result valid iff both inputs valid |
| K10 `arith_guarded` | as K9 | as K9 + overflow report | Checked (report, never trap/UB) and saturating variants. Overflow reporting granularity: **[DEFERRED]** to PRD (flag vs first-index vs mask) within the no-silent-UB contract |

---

*End of Design Charter v1.0. Next document in the pipeline: the Engineering PRD, which treats this charter as a fixed contract.*
