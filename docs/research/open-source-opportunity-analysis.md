# Open-Source Opportunity Analysis

**Where to invest 12–18 months of solo systems-engineering effort in the analytical-execution ecosystem**

Prepared July 2026. Companion to *Vectorized Analytical Execution Engines: A Design-Space Survey* (cited below as "Survey §n"). GitHub figures pulled live from the GitHub API on 2026-07-02. Claims labeled as in the survey: [FACT], [IMPL], [INFERENCE], [SPECULATION].

**Headline conclusion, stated up front:** building another standalone execution engine is the *weakest* of thirteen candidates evaluated (§6). The strongest opportunity is a dependency-light, per-ISA **vectorized analytical kernel library** — the "~10 kernels every engine reimplements" — following the empirically validated simdjson/CRoaring/StringZilla adoption path. Full reasoning in §12.

---

## 1. Ecosystem Map

### 1.1 Vitals (GitHub API, 2026-07-02; open-issues count includes PRs)

| Project | Stars | Forks | License | Language | Status | Backing |
|---|---|---|---|---|---|---|
| ClickHouse | 48,419 | 8,598 | Apache-2.0 | C++ | Very active | ClickHouse Inc. ($350M Series C, May 2025, $6.35B valuation) |
| DuckDB | 39,152 | 3,378 | MIT | C++ | Very active (v1.5.4 / v1.4.5 LTS, Jun 2026) | DuckDB Foundation (IP) + DuckDB Labs (VC-free); MotherDuck as commercial cloud |
| Polars | 38,906 | 2,926 | MIT | Rust | Very active | Polars Inc. ($21M Series A, Accel, Sep 2025); Polars Cloud beta |
| Apache Arrow | 16,897 | 4,162 | Apache-2.0 | C++ (multi) | Very active (24.0.0, Apr 2026) | ASF, vendor-neutral |
| DataFusion | 8,927 | 2,191 | Apache-2.0 | Rust | Very active | ASF TLP (2024); InfluxData, Apple, dbt Labs contributors |
| Velox | 4,166 | 1,535 | Apache-2.0 | C++ | Very active | Meta-controlled (velox-lib.io governance page: merges executed only by Meta oncall; **no** Linux Foundation transfer found) |
| arrow-rs | 3,514 | 1,205 | Apache-2.0 | Rust | Very active | ASF |
| MonetDB | 471 (mirror) | 62 | MPL-2.0 | C | Active (Dec2025/11.55) | MonetDB Foundation + MonetDB Solutions |
| Umbra | — closed | — | proprietary | C++ | Research-active | TUM; commercialized as CedarDB (seed ~$6M, 2025; free tier capped at 64 GB stored data) |
| HyPer | — closed | — | proprietary | C++ | Alive inside Tableau (Hyper API v0.0.25080, May 2026) | Salesforce/Tableau |
| Vectorwise | — closed | — | proprietary | C++ | Declining: Vector-in-Hadoop discontinued 2024; renamed "Actian Analytics Engine" at v8.0 (2026) | Actian/HCLSoftware |
| Acero (in Arrow) | (within apache/arrow) | — | Apache-2.0 | C++ | Maintained, deliberately non-competitive: "Acero isn't intended to be a cutting-edge query engine competing with... DuckDB or Velox" (Arrow discussion #47331, Aug 2025); core fixes driven by an outside contributor | ASF |
| Gandiva (in Arrow) | (within apache/arrow) | — | Apache-2.0 | C++/LLVM | **Deprecation path**: `pyarrow.gandiva` deprecated in Arrow 24.0.0 (Apr 2026); no feature development in 2024–26 dev traffic | ASF |

### 1.2 Purpose, strengths, weaknesses, biggest limitation

**DuckDB** — in-process OLAP DBMS (Survey §2.2). Strengths: vertical integration, larger-than-memory execution, portability, the standard-setting embedded UX. Adoption: "millions of downloads/month" (vendor figure); embedded across BI tools, MotherDuck, Fivetran, Hugging Face. Biggest engineering limitation: single-node, single-writer concurrency model; scale-out delegated to third parties. [IMPL]

**ClickHouse** — columnar OLAP server (Survey §2.3). Strengths: scan/aggregation throughput at fleet scale, pruning stack, operational maturity. Users (vendor materials): Cloudflare, Uber, eBay, Microsoft Clarity, Tesla. Biggest limitation: historical JOIN weakness / denormalization-first culture — acknowledged in its own optimizer-investment blog series; heavyweight mutations. [IMPL]

**Velox** — reusable C++ execution library (Survey §2.4). Strengths: encoding-aware execution, hierarchical memory arbitration, production deployment across Presto C++/Gluten/IBM/NVIDIA-cuDF backends. Weaknesses: no parser/optimizer; notoriously heavy and brittle builds; unstable APIs (own blog documents dependency-update SEGFAULTs); Meta-gated commit rights limit external committers. Biggest limitation: integration burden — it is infrastructure that itself is hard to embed. [IMPL]

**Apache Arrow** — the columnar interchange standard + kernels + transport (Survey §2.6). Strengths: universal adoption at the boundary (pandas, Spark, Polars, DuckDB interop, Snowflake/BigQuery transport). Weakness: its execution ambitions (Acero, Gandiva) are second-tier citizens — one maintained-but-not-competitive, one being phased out. Biggest limitation: the C++ *execution* vacuum inside the most widely adopted data ecosystem. [FACT/IMPL]

**DataFusion** — modular Rust engine toolkit (Survey §2.5). Strengths: extension-first architecture, ~dozen production downstream systems (InfluxDB 3.x, Comet, GreptimeDB, LanceDB, dbt Fusion via SDF), ClickBench-Parquet leadership. Biggest limitation: single-node core with distribution delegated (Ballista/Comet); optimizer maturity still behind DuckDB/ClickHouse in places. [IMPL]

**Polars** — DataFrame engine (Survey §2.11). Strengths: enormous Python reach (vendor: 250K→26M monthly users since seed), lazy optimizer, new streaming engine. Biggest limitation: memory governance (no global limit comparable to DuckDB's); two engines in flight; distribution only via commercial cloud. [IMPL]

**MonetDB** — the column-store pioneer (Survey §2.7). Strengths: historical importance; still-active releases. Weaknesses: operator-at-a-time full materialization (the architecture X100 was invented to fix — Survey §1.3/1.4); 471-star mindshare. Biggest limitation: an execution model two generations old. [FACT]

**Umbra/CedarDB** — the compiled-engine state of the art (Survey §2.9). Strengths: best published latency+throughput; German strings and variable-size-page buffer manager now industry-copied. Biggest limitation for the ecosystem: closed source; the free tier caps stored data at 64 GB. [IMPL]

**HyPer** — origin of data-centric compilation and morsel scheduling (Survey §2.8); survives as Tableau's Hyper engine, externally reachable only through the .hyper-file API. Not an embeddable general engine. [IMPL]

**Vectorwise** — the productized X100 (Survey §2.10). Declining commercial footprint; its durable legacy is conceptual (vectors, selection vectors, micro-adaptivity). [IMPL]

**Acero / Gandiva** — Arrow's execution layers. Acero: maintained, community-driven, explicitly not competing. Gandiva: deprecated in Python bindings, dormant in C++. [FACT — cited discussions/releases]

### 1.3 Ecosystem events 2024–2026 relevant to strategy

[FACT/IMPL] ClickHouse $350M C ($6.35B); Polars $21M A + Polars Cloud; CedarDB launch (Umbra commercialization); dbt Labs acquired SDF, shipped Rust "dbt Fusion" leaning on the DataFusion ecosystem (medium confidence); Apple donated Comet (Spark accelerator on DataFusion, releases every ~6 weeks); Ballista revived; Voltron Data pivoted to proprietary GPU engine Theseus (not acquired; Accenture strategic investment); StarRocks/Doris winning JOIN-heavy "ClickHouse-alternative" deals; Vector-in-Hadoop discontinued. **[INFERENCE] Read on the tape:** execution is consolidating into a few heavily funded engines plus two reusable-library ecosystems (Velox C++, DataFusion Rust); the money and mindshare have moved *up* (products) and *sideways* (composable libraries) — not toward new standalone engines.

---

## 2. Market Gap Analysis

### Already solved (do not compete)

- **Embedded single-node OLAP SQL** — DuckDB (39k stars, foundation-owned, VC-immune). [FACT]
- **Fleet-scale OLAP serving** — ClickHouse + StarRocks/Doris. [FACT]
- **DataFrame UX on a query engine** — Polars (38.9k). [FACT]
- **Rust modular engine toolkit** — DataFusion (8.9k, ASF, dozen+ downstream systems). [FACT]
- **Columnar interchange format** — Arrow; the format war is over (Survey §2.6). [FACT]
- **General SIMD abstraction** — Highway 5,650★, xsimd 2,718★, EVE 1,353★, SIMDe 3,047★, and C++26 `std::simd` finalized March 2026 standardizing the baseline away. CROWDED. [FACT]
- **General task scheduling** — Taskflow 12,032★, oneTBB 6,691★, HPX 2,865★. [FACT]
- **General hash tables** — abseil 17,363★, parallel-hashmap 3,192★, unordered_dense 1,416★, boost concurrent maps. [FACT]
- **Microbenchmark harnesses** — google/benchmark 10,266★. [FACT]

### Partially solved

- **C++ execution-as-a-library** — Velox exists but is heavy, API-unstable, Meta-gated; Acero is deliberately non-competitive; Gandiva is dying. *Why the gap persists:* Meta builds for Meta-scale embedders, not for approachability; ASF lacks maintainer bandwidth for Acero. *Who benefits from a fix:* every C++ system needing analytical primitives without a 1,500-fork monorepo dependency. [IMPL/INFERENCE]
- **Integer codec kernels** — FastPFOR 973★, TurboPFor 835★, streamvbyte 417★ cover bit-packing/varint; codec-only, no predicates/selection/hash. [FACT]
- **Compressed execution** — Vortex (3,146★, Rust, LF AI incubation) owns the Rust lane; C++ capability is engine-internal (DuckDB) or CWI research artifacts (FastLanes 682★ morphing into a file format, ALP 176★ paper artifact, fsst 529★ genuinely importable). *Why:* CWI publishes formats, engines internalize them; nobody packages the *execution* layer in C++. [FACT/INFERENCE]
- **Operator-level, statistically rigorous cross-engine benchmarking** — ClickBench (1,024★) is end-to-end and self-admittedly join-free; db-benchmark (182★) is end-to-end per-framework; nanobench dormant. The DBTest 2018 methodology (Survey §7.5) has never been codified into tooling. *Why:* benchmarks star poorly and reward maintenance thanklessly. [FACT/INFERENCE]
- **Teaching engines** — BusTub (~4,900★) is OLTP; RisingLight (~1,800★, Rust) is low-activity; CMU 15-721 OLAP projects are private. No public "build a vectorized OLAP engine in C++" artifact. [FACT]

### Poorly solved

- **Analytical SIMD kernels as an importable library.** The ~10 primitives every engine reimplements — filter/compaction, selection-vector ↔ bitmap conversion, vectorized hashing, dictionary decode, bit-unpack-with-predicate, saturating/overflow-checked arithmetic on batches, null-mask combination — exist only *inside* DuckDB/Velox/ClickHouse/Arrow (Survey §4.5: the convergent "~10 hottest kernels" pattern). The sole standalone attempt, lemire/simdprune, is x86-only, dormant since 2024, 68★. Survey §11.4 gap #6 ("an honest SIMD ledger") and the ADMS 2023 Velox study (Survey §4.5) both point here. *Why it exists:* engines treat kernels as internal plumbing; library authors treat SIMD abstraction (not kernels) as the product; the intersection — database-shaped kernels, per-ISA, dependency-light — has no owner. *Who benefits:* engine builders (established and new), embedded/edge analytics, quant/HFT data plumbing, teaching, and researchers needing credible baselines. [FACT/INFERENCE]
- **Apple Silicon performance tooling** — the canonical kperf reference is a *gist*; wrappers are ≤25★ or archived. Real gap, tiny addressable audience, private-API fragility (Survey §7.3). [FACT]

### Not solved

- **Re-derived execution constants on current hardware** — vector size, morsel size, partition fan-outs still run on 2005–2014 calibrations (Survey §11.3 #1, #5). No published toolkit.
- **P/E-core-aware analytical scheduling** — no engine publishes a policy (Survey §11.3 #2).
- **Selection-representation adaptivity (bitmap vs selection vector) as a function of ISA/selectivity** — engines pick one globally (Survey §11.3 #3).
- **A standalone parallel-aggregation library** — the contested two-phase-vs-global design space (Survey §6.5, "Global Hash Tables Strike Back!" 2025) has no importable artifact; closest is a paper-artifact repo. [FACT]

---

## 3. Competitive Landscape: is another execution engine justified?

**As a product — no.** [INFERENCE, from §1–2] The evidence is one-directional:

1. Funding asymmetry: a solo 12–18-month engine enters against DuckDB (foundation-owned, ubiquitous), ClickHouse ($650M raised), Polars (Accel-backed), CedarDB (the Umbra team itself). The survey's own conclusion (Survey §11.1 #8) is that engine competition is decided by engineering economics, and a solo engineer has the worst economics in the field.
2. The differentiation slots are occupied: "embedded" (DuckDB), "server" (ClickHouse/StarRocks), "DataFrame" (Polars), "modular Rust" (DataFusion), "C++ library" (Velox), "compiled state-of-the-art" (Umbra/CedarDB). A new engine's honest elevator pitch would be "like X but less complete."
3. The graveyard is instructive: Acero (backed by the Arrow brand!) could not sustain competitive ambitions; Gandiva is deprecated; sneller (AVX-512 SQL engine) — repo deleted. Engine-shaped projects without a company die of maintenance load.
4. History rhymes: every surviving engine embodies 50+ person-years. The survey's pattern list (Survey §9) is now *table stakes* — a solo engine that implements all 17 patterns adequately is years of work that demonstrates breadth, not depth or novelty.

**As infrastructure — yes, with exact differentiation.** [INFERENCE] The same evidence points at what *is* viable solo: the simdjson precedent. simdjson (23,916★) is a narrow, best-in-class kernel library authored initially by an academic and a small group, now vendored by ClickHouse, Velox, Doris, StarRocks, Milvus, Node.js. CRoaring (1,854★): same story (Doris, ClickHouse, Redpanda, YDB). StringZilla (3,510★) and SimSIMD (1,844★): a solo author, 2023–2026, absorbed by databases. Narrow kernels with rigorous benchmarks get *adopted by* the engines that a solo engineer cannot compete *with*. The differentiation requirements, derived from why simdprune failed and simdjson succeeded: (a) database-shaped scope, not general SIMD abstraction (that lane is crowded and being standardized away); (b) cross-ISA from day one (NEON is mandatory — Survey §4.1 — simdprune was x86-only); (c) dependency-light, vendorable, amalgamation-friendly (the anti-Velox); (d) benchmarks as a first-class deliverable, since per-kernel claims are exactly what the field's methodology literature (Survey §7) can make credible; (e) C++ — the Rust lane has arrow-rs/DataFusion/Vortex, the C++ lane has a vacuum between "all of Velox" and "nothing."

---

## 4. Candidate Products

Thirteen candidates, each traceable to a survey gap or ecosystem observation. Difficulty = for one strong engineer.

**C1. Standalone vectorized execution engine** (the original "Vector Engine" concept: columnar memory → operators → SIMD → scheduler → benchmarks, no SQL). *Novelty:* low — recombines Survey §9's table-stakes patterns. *Difficulty:* very high (breadth). *Engineering value:* high as personal education. *Research value:* low. *Hiring value:* good but generic — "mini-DuckDB" is a recognized student-project genre. *GitHub potential:* weak (see C1 red team). *Long-term:* low; unmaintained engines rot fast.

**C2. Analytical SIMD kernel library.** The ~10 hot primitives every engine reimplements — filter/compact, selection-vector↔bitmap conversion and combination, vectorized hash (integer/string batches), dictionary decode with selection, bit-unpack with predicate pushdown, null-mask arithmetic, batch min/max/SMA — per-ISA (NEON, AVX2, AVX-512, scalar fallback), runtime dispatch, header-only or amalgamation-vendorable, zero dependencies, with a PMU-instrumented per-kernel benchmark ledger across microarchitectures. Traces to Survey §4.5 ("~10 hottest primitives" convergence), §11.4 gaps #3 and #6, and niche scan: EMPTY with validated precedent (simdjson 23.9k★). *Novelty:* high as an artifact (nothing maintained exists); moderate as research (kernels known, cross-ISA ledger is new). *Difficulty:* high but bounded — kernels are independently shippable. *Engineering/hiring value:* maximal — exactly the memory-layout/SIMD/measurement skills the project is meant to demonstrate. *GitHub potential:* strongest of all candidates (precedent-based).

**C3. Execution measurement laboratory** ("executable textbook"): one repo where each operator exists in naive → cache-aware → SIMD → parallel variants with PMU counters and flamegraphs wired in, reproducing classic results (X100 vector-size sweep, branch-misprediction tent, false-sharing cliff) on modern hardware. Traces to Survey §7 + §11.4 #1/#7. *Novelty:* moderate (pedagogy + reproduction). *Hiring value:* high (demonstrates measurement maturity). *GitHub potential:* decent educational audience.

**C4. Hardware calibration & cost-model toolkit**: measures cache/TLB/bandwidth/branch parameters and re-derives engine constants (vector size, morsel size, partition fan-out) per machine — Manegold/Boncz 2002 methodology (Survey §3.2) modernized. Traces to Survey §11.3 #1/#5, §11.4 #1. *Novelty:* high research-wise. *Difficulty:* moderate. *GitHub potential:* niche.

**C5. NUMA/P-E-core-aware morsel scheduler library**: Leis 2014 as an importable C++ library plus the unclaimed heterogeneous-core policy. Traces to Survey §11.3 #2, §11.4 #5; niche scan PARTIAL. *Novelty:* moderate-high (P/E angle is unclaimed). *Risk:* "why not Taskflow?" adoption friction.

**C6. Operator-level cross-engine benchmark suite** codifying DBTest 2018 + Berger-school methodology (Survey §7.5, §11.4 #7) with adapters for DuckDB/DataFusion/Polars and controlled cardinality/selectivity/skew sweeps. *Novelty:* high methodologically. *Research value:* high (benchmark papers cite well). *GitHub potential:* historically poor for benchmarks (ClickBench 1,024★ despite ClickHouse's brand).

**C7. Parallel aggregation library**: standalone two-phase/global-adaptive group-by over columnar batches. Traces to Survey §6.5 contested design space; niche scan: EMPTY for the specific artifact. *Novelty:* moderate-high. *Risk:* API surface is engine-shaped (types, nulls, spill) — scope creeps toward being an engine.

**C8. German-strings library**: 16-byte view string type + arena + kernels for C++ (Survey §2.12 convergence). *Novelty:* low-moderate (design is published and widely copied; StringZilla covers adjacent ground at 3.5k★). *Difficulty:* moderate.

**C9. Compressed-predicate kernel library** ("Vortex-lite for C++"): predicates/aggregates over dictionary/FOR/bit-packed/RLE columns without decompression, SMA/zone-map pruning included. Traces to Survey §11.3 #9, §2 (Data Blocks lineage); niche scan PARTIAL (Rust taken, C++ open). *Novelty:* high. *Risk:* CWI's active research path (FastLanes) is a moving target; overlaps C2 substantially.

**C10. Batch-probe prefetch library**: AMAC/group-prefetch (Survey §3.6) re-evaluated on 512-entry-ROB hardware, packaged as a batch-lookup interface. Traces to Survey §11.3 #6. *Novelty:* high research-wise. *Audience:* narrow.

**C11. Vectorized expression evaluator**: interpreted, per-batch adaptive (NULL-free/ASCII fast paths à la Photon/Velox — Survey §9 #12), no LLVM — a Gandiva successor without the JIT. *Novelty:* moderate. *Risk:* useful only embedded; embedders already have one.

**C12. Apple Silicon PMU benchmarking harness** (kperf-based). Niche scan: EMPTY. *Novelty:* moderate. *Risk:* private-API fragility, tiny audience, Apple could break it any release.

**C13. C++ vectorized-OLAP teaching engine** ("BusTub for OLAP"): public skeleton + test suite + course notes. Niche scan: slot genuinely unoccupied. *Novelty:* low technically, high pedagogically. *Hiring value:* moderate (signals teaching, not frontier engineering).

---

## 5. Red Team

**C1 engine.** Competitors: DuckDB/Polars/DataFusion occupy every user-facing slot; "engine without SQL" has no users by construction — its only consumers are benchmarks. Demand: nobody imports a solo engine; trust in correctness/maintenance is unearnable at n=1 (the DBTest "TimDB" critique — Survey §7.5 — applies with full force: a kernel-only engine beating DuckDB is *expected*, not impressive). Scope: implementing Survey §9's 17 table-stakes patterns alone consumes the entire timeline with zero novelty. Maintenance: engines rot; abandoned engine repos are a dime a dozen. Differentiation: none articulable beyond "educational." **Fails as infrastructure; survives only as a learning artifact.**

**C2 kernels.** Might fail because: (a) engines have NIH culture and vendor rather than depend — mitigated by designing for vendoring (amalgamation, permissive license), which is exactly how simdjson entered ClickHouse; (b) Highway/std::simd could absorb the niche — but they are abstractions, not database-shaped kernels with selection-vector semantics; nothing in their roadmaps targets this (niche scan); (c) per-ISA correctness/perf testing matrix is expensive solo — bounded by CI on x86 + Graviton + Apple runners; (d) the benchmark-ledger claim invites adversarial scrutiny — that is a feature (methodology is the moat) but demands rigor; (e) star growth could stall at SimSIMD levels (~2k) rather than simdjson levels — acceptable downside. Biggest real risk: **scope discipline** — the library must refuse to become an engine.

**C3 lab.** Educational repos have soft ceilings; "reproductions" invite "so what's new?"; overlap with existing course materials (15-721 notes); no dependency potential. Also: measurements without a reusable artifact age into blog posts.

**C4 calibration.** Audience is researchers + a handful of engine tuners; results are machine-specific and perishable; risks becoming a one-paper artifact repo (the fate of ALP at 176★).

**C5 scheduler.** Adoption friction is brutal: schedulers are load-bearing and trust-gated; TBB/Taskflow own mindshare; DataFusion's documented reversal (Survey §6.3) shows even *engines* decline bespoke schedulers. P/E research angle is real but thin for 12–18 months.

**C6 bench suite.** Benchmarks earn enemies and maintenance, not stars (ClickBench 1,024★ *with* ClickHouse marketing). Engine version churn makes adapters a treadmill. Vendor pushback is guaranteed; solo neutrality is hard to defend.

**C7 par-agg.** The API cannot stay small: group-by needs types, nulls, strings, spill, memory limits — it re-derives an engine's guts. Competes with "just use DuckDB" for every practical user.

**C8 strings.** Design is already public commons (CedarDB blog, Arrow spec); the remaining work is glue; StringZilla adjacency invites unfavorable comparison. Thin novelty.

**C9 compressed.** Moving-target risk (FastLanes evolving into a format), Vortex halo in the adjacent lane, and the honest kernel subset overlaps C2 — as a *first* project it is C2 with extra format risk. Strong *second act*, weak opener.

**C10 prefetch.** Research-grade audience only; results are microarchitecture-fragile; coroutine interfaces are contentious; likely terminal state is one good workshop paper.

**C11 expr eval.** Embedders needing one already have one (DuckDB/Velox internal, DataFusion in Rust); Gandiva's deprecation signals weak demand for standalone expression layers, not strong demand.

**C12 kperf.** Apple can break private APIs silently any release; no institutional user can depend on it; ceiling ≈ a few hundred stars and a great blog post.

**C13 teaching.** Requires course-grade docs/tests/CI — a second full-time job; adoption depends on instructors; low frontier-engineering signal for the target hiring audience.

---

## 6. Opportunity Matrix

Scores 1–10. Weights in parentheses; "Maintenance" scored as sustainability (10 = light, survivable solo). Weighted totals computed programmatically.

| Criterion (weight) | C1 | C2 | C3 | C4 | C5 | C6 | C7 | C8 | C9 | C10 | C11 | C12 | C13 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Technical depth (.10) | 9 | 9 | 8 | 8 | 8 | 6 | 8 | 6 | 8 | 8 | 8 | 5 | 7 |
| Novelty (.08) | 3 | 8 | 7 | 7 | 6 | 7 | 6 | 4 | 7 | 6 | 5 | 6 | 5 |
| Industry usefulness (.10) | 3 | 9 | 5 | 6 | 6 | 7 | 6 | 5 | 7 | 4 | 6 | 4 | 4 |
| OSS adoption (.08) | 4 | 8 | 6 | 5 | 5 | 5 | 5 | 5 | 6 | 3 | 5 | 5 | 7 |
| Resume impact (.12) | 8 | 9 | 8 | 7 | 7 | 6 | 7 | 6 | 8 | 6 | 7 | 5 | 7 |
| Research potential (.07) | 3 | 7 | 6 | 8 | 6 | 8 | 7 | 4 | 8 | 7 | 5 | 3 | 4 |
| Benchmarkability (.07) | 7 | 10 | 9 | 9 | 7 | 10 | 9 | 7 | 9 | 8 | 7 | 8 | 6 |
| Maintenance (.05) | 4 | 7 | 7 | 6 | 5 | 3 | 6 | 7 | 5 | 6 | 4 | 4 | 3 |
| External contributors (.04) | 3 | 7 | 5 | 4 | 4 | 6 | 4 | 4 | 5 | 3 | 4 | 4 | 7 |
| GitHub growth (.05) | 4 | 8 | 6 | 4 | 5 | 5 | 5 | 5 | 6 | 3 | 5 | 5 | 7 |
| Becomes infrastructure (.06) | 2 | 9 | 3 | 4 | 6 | 5 | 6 | 6 | 7 | 4 | 6 | 4 | 2 |
| NVIDIA recognition (.03) | 6 | 8 | 6 | 7 | 5 | 4 | 5 | 4 | 6 | 6 | 5 | 3 | 4 |
| Meta recognition (.03) | 6 | 8 | 6 | 6 | 7 | 6 | 7 | 6 | 7 | 5 | 7 | 3 | 4 |
| DB researchers respect (.04) | 4 | 8 | 7 | 8 | 7 | 9 | 8 | 7 | 9 | 8 | 6 | 3 | 7 |
| HPC respect (.03) | 6 | 9 | 7 | 8 | 7 | 5 | 6 | 4 | 6 | 7 | 5 | 6 | 4 |
| Citability (.05) | 3 | 7 | 6 | 8 | 5 | 8 | 6 | 4 | 8 | 7 | 4 | 3 | 5 |
| **Weighted total** | **4.91** | **8.34** | **6.53** | **6.64** | **6.15** | **6.39** | **6.47** | **5.33** | **7.19** | **5.71** | **5.80** | **4.63** | **5.39** |

Ranking: **C2 (8.34)** > C9 (7.19) > C4 (6.64) > C3 (6.53) > C7 (6.47) > C6 (6.39) > C5 (6.15) > C11 (5.80) > C10 (5.71) > C13 (5.39) > C8 (5.33) > **C1 (4.91)** > C12 (4.63).

Two results deserve emphasis: C2 wins by 1.15 points — a wide margin under any reasonable weight perturbation [INFERENCE: it leads or ties on 13 of 16 criteria, so no plausible re-weighting overturns it]. And the original idea, C1, ranks 12th of 13: high technical depth and resume score cannot rescue near-zero novelty, usefulness, and infrastructure potential.

---

## 7. Infrastructure vs Application

| Candidate | Classification | Why |
|---|---|---|
| C1 | Engine / research artifact | Executes queries end-to-end; nobody embeds it |
| C2 | **Infrastructure + library** | Imported/vendored by other systems; no standalone use; the definition of a dependency |
| C3 | Research artifact + developer tooling | Consumed by reading and running, not linking |
| C4 | Developer tooling + benchmark suite | Produces calibration data; possible tiny library surface |
| C5 | Library / runtime | Linked component; load-bearing at runtime |
| C6 | Benchmark suite | Pure methodology artifact |
| C7 | Library | Linked component with engine-shaped API |
| C8 | Library | Vocabulary type + kernels |
| C9 | Library / infrastructure | Same adoption physics as C2, plus format coupling |
| C10 | Library + research artifact | Narrow linked component |
| C11 | Library / framework | Embedded evaluator; framework-like host contract |
| C12 | Developer tooling | Measurement harness |
| C13 | Framework + educational artifact | Skeleton others fill in |

---

## 8. API Reusability

**C2** — the only candidate with unambiguous yes on all five questions. Import: engines, file-format readers, stream processors, feature pipelines. Dependency: designed for vendoring (simdjson/fsst precedent — fsst is vendored by DuckDB and CedarDB at 529★). Companies embed: the simdjson user list (ClickHouse, Velox, Doris, StarRocks, Milvus, Node.js) is the demonstrated absorption path for exactly this shape of library. Researchers benchmark against it: a per-kernel, per-ISA public ledger becomes the reference baseline the literature currently lacks (Survey §11.4 #6). Ecosystem effects: a neutral kernel commons lowers the floor for every new engine — including Rust/other-language ports binding to it.

**C9** — same physics, gated by format standardization risk. **C5/C7/C11** — importable in principle; trust-gated and engine-shaped in practice. **C4/C6** — benchmarked *with*, not linked *against*; citation-reusable rather than API-reusable. **C3/C13** — read, not imported. **C1** — no import story: engines are destinations, not dependencies (Velox, the exception, has Meta behind it). **C8/C10/C12** — narrow import audiences.

---

## 9. Research Contribution

| Candidate | Workshop paper | Conference paper | Tech report | Eng. blog | Benchmark paper |
|---|---|---|---|---|---|
| C1 | unlikely | no | yes | yes | no — engine benchmarks vs unequal systems are the DBTest anti-pattern |
| C2 | **yes — DaMoN/ADMS is precisely this venue** (cf. Benson et al. ADMS 2023, Survey §4.5) | plausible (VLDB industrial/experiment track if the cross-ISA ledger yields surprising results, e.g. SVE/NEON vs AVX-512 compress economics) | yes | yes — per-kernel posts are the SimSIMD/simdjson growth engine | yes — the ledger *is* one |
| C3 | yes (reproduction studies are publishable at DaMoN) | unlikely | yes | yes | partial |
| C4 | yes | plausible (experiments-and-analysis track; Manegold-lineage) | yes | yes | yes |
| C5 | yes (P/E-core scheduling is unclaimed — Survey §11.3 #2) | plausible | yes | yes | no |
| C6 | yes | plausible (DBTest itself) | yes | yes | yes by definition |
| C7 | yes (extends the 2025 global-vs-partitioned dispute) | plausible | yes | yes | yes |
| C8 | no | no | yes | yes | no |
| C9 | yes | plausible | yes | yes | yes |
| C10 | yes (AMAC-on-modern-ROBs is a clean question) | unlikely | yes | yes | partial |
| C11 | unlikely | no | yes | yes | no |
| C12 | no | no | yes | yes | no |
| C13 | no (CS-ed venues at most) | no | yes | yes | no |

---

## 10. Implementation Risk

Estimates for one strong engineer; months are focused-effort months.

| | C2 (recommended) | C1 | C9 | C4 | C6 |
|---|---|---|---|---|---|
| Months | 12–15 | 18+ (floor) | 14–18 | 8–12 | 10–14 + treadmill |
| Milestones | M1–2: scalar reference kernels + correctness suite + benchmark rig; M3–5: AVX2 set + ledger v1; M6–8: NEON set + Apple/Graviton CI; M9–11: AVX-512 (masks/compress) + dispatch; M12–15: selection-representation study, docs, amalgamation, v1.0, workshop paper | storage→vectors→exprs→agg→join→scheduler→bench (each a project) | formats → predicates → pruning → ledger | probe design → calibration → constants → report | harness → adapters ×3 → stats → referee disputes |
| Expected LOC | 15–30k src + comparable test/bench | 50–100k | 20–35k | 8–15k | 10–20k |
| Testing burden | High but mechanical: golden scalar reference × ISA × alignment × selectivity; fuzzing pays off well | Very high (correctness of joins/aggs, spilling) | High + format conformance | Moderate | High (cross-engine drift) |
| Maintenance | Moderate: stable scope, additive ISA work | High: everything breaks everywhere | Moderate-high (format churn) | Low-moderate | High (version treadmill) |
| Cross-platform | The product itself — x86/ARM CI from month 1; Windows MSVC is the usual pain | High | High | High (PMU access variance) | Moderate |
| SIMD complexity | Maximal — that is the point; bounded per-kernel | Diffuse | High | Low | Low |
| Benchmark complexity | High and central; PMU + statistics rig is a deliverable, reusing Survey §7 practices | High | High | Maximal | Maximal |
| Documentation | High: per-kernel docs + ledger methodology; docs are the marketing | Very high | High | Moderate | High |

Key risk asymmetry [INFERENCE]: C2 is the only high-scoring candidate whose scope *shrinks gracefully* — a 9-month version (scalar + AVX2 + NEON, 6 kernels, honest ledger) is already shippable, citable, and adoption-eligible. C1's 9-month version is an unfinished engine; C6's is a benchmark nobody has agreed to yet.

---

## 11. Hiring Analysis

What each reviewer sees in C2 (the recommendation), and what they would criticize. [INFERENCE throughout — informed by each company's public engineering practice.]

- **NVIDIA** — impressed: per-ISA kernel engineering, occupancy-style throughput reasoning, roofline honesty; kernels are the shared vocabulary of CUDA and SIMD work (and NVIDIA already backs Velox/cuDF integration). Criticism: CPU-only; would probe whether you understand the GPU analogue of every design choice.
- **Meta** — impressed: it is a de-Meta'd slice of Velox's SimdUtil with better packaging and a public ledger; Velox contributors would recognize every kernel. Criticism: where is the encoding-awareness (dictionary/constant vectors) beyond decode; API stability discipline.
- **Apple** — impressed: NEON-first parity, Apple Silicon numbers in the ledger, 128-byte-line correctness (Survey §3.1). Criticism: any x86-first bias in API shape; missing AMX/SME awareness.
- **Intel** — impressed: AVX-512 mask/compress exploitation (Survey §4.1), dispatch hygiene, uops-level analysis. Criticism: will demand icc/oneAPI comparisons and question Zen-4-centric tuning.
- **AMD** — impressed: Zen 4 double-pump nuance, compress-to-memory gotcha handled (Survey §4.1). Criticism: cache-hierarchy assumptions tuned on Intel topologies.
- **Databricks** — impressed: Photon-adjacent taste — batch adaptivity hooks, NULL-fast paths, the vectorized-interpreted worldview (Survey §1.10). Criticism: no expression-level or operator-level story; single-node only.
- **Snowflake** — impressed: X100-lineage fluency (their execution ancestry — Survey §1.4); compressed-execution hooks. Criticism: no spill/memory-governance surface; would ask how kernels behave under multi-tenancy.
- **ClickHouse** — impressed: dispatch-per-cpuid mirrors their own multitarget framework; they vendor exactly such libraries (simdjson, CRoaring). Criticism: they will benchmark it against their internal kernels within a day — the ledger must survive that.
- **Jane Street** — impressed: measurement rigor, statistics policy, reproducibility manifests (Survey §7 practices as code). Criticism: C++ not OCaml; would grill on correctness proofs and UB discipline.
- **Hudson River Trading** — impressed: latency-grade attention to branch behavior, cache lines, allocation-free hot paths. Criticism: throughput-oriented framing; they'd ask for tail-latency characterization.
- **Citadel Securities** — impressed: hardware-conscious C++23, honest perf claims (rare in candidate portfolios). Criticism: single-author bus factor; will test whether the numbers were understood or copied.

C1, by contrast, reads to all eleven as "ambitious coursework": familiar shape, no adoptable surface, benchmarks against strawmen (the DBTest trap). C2 reads as "this person builds the part of the engine we actually argue about."

---

## 12. Final Recommendation

**Build C2: the analytical SIMD kernel library.** One project, exactly.

**Why it dominates every alternative.** It leads or ties on 13 of 16 scored criteria and wins the weighted matrix by 1.15 points (8.34 vs 7.19 for the runner-up); no defensible re-weighting overturns that margin. Each rival loses on a fatal axis: C1 (the engine) has no adoption story and near-zero novelty — it is 17 table-stakes patterns (Survey §9) reimplemented worse than the incumbents; C9 (compressed kernels) is C2 plus format-churn risk and is better as a v2 expansion; C4/C6 (calibration, benchmarking) are citation artifacts, not dependencies; C5/C7/C11 are trust-gated engine organs nobody links from a solo repo; C3/C13 are pedagogy; C8/C10/C12 are too narrow to carry 12–18 months.

**Why it has the highest expected value.** It sits at the intersection of a verified-EMPTY niche (the only prior attempt, simdprune, is dormant at 68★ and x86-only) and a verified adoption precedent (simdjson 23,916★ vendored by ClickHouse, Velox, Doris, StarRocks, Milvus; CRoaring 1,854★; StringZilla 3,510★ — solo author). The demand signal is structural, not speculative: every engine in the survey independently reimplements the same ~10 primitives (Survey §4.5), and the literature explicitly flags the missing cross-ISA evidence base (Survey §11.4 #6, §11.3 #3). Downside is bounded: even at SimSIMD-scale adoption (~2k★) it remains a workshop paper, a citable ledger, and the strongest possible demonstration of the exact skills the project exists to demonstrate.

**Why it is realistically buildable.** Kernels are independently shippable; the scope shrinks gracefully (a 9-month cut — scalar + AVX2 + NEON, six kernels, honest ledger — is already releasable and adoption-eligible, per §10). No component requires the multi-quarter, all-or-nothing integration that kills solo engines. Correctness is mechanically testable against golden scalar references; CI on x86/Graviton/Apple runners covers the ISA matrix.

**Why it has lasting open-source potential.** It is infrastructure by construction: vendorable, dependency-free, permissively licensed, with the benchmark ledger as continuously renewable content (new microarchitectures arrive every year and each one is a new result). The niche cannot be "standardized away" — C++26 std::simd absorbs *abstractions*, not database-shaped kernels with selection-vector semantics.

**Why it is the strongest portfolio project.** It converts the original Vector Engine thesis — cache-aware analytical execution, evidence over assumption — into the one artifact form that working engineers at the target companies actually import, benchmark, and argue about. §11's company-by-company review reads uniformly better for C2 than for any alternative: it is "the part of the engine we argue about," not "another engine."

**On rejecting the original premise.** The instruction was to challenge "build another execution engine" if weaker. It is weaker — 12th of 13 (4.91). The engine idea's genuine value (learning breadth) is preserved anyway: the kernel library's demo layer (below) composes kernels into filter→aggregate pipelines for end-to-end benchmarks, delivering the educational arc without shipping an unmaintainable engine.

---

## 13. Product Definition

**Project name:** **Quiver** — a quiver holds arrows: a set of sharp, interchangeable projectiles, deliberately Arrow-ecosystem-adjacent. (Name collision diligence required before launch: an archived note-taking app and a math-visualization tool share the word; neither occupies the C++/data niche. Fallbacks: `vkq`, `fletch`.)

**One sentence:** Quiver is a dependency-free C++23 library of the ~10 vectorized kernels every analytical engine reimplements — filter/compaction, selection-vector and null-mask algebra, batch hashing, dictionary decode, bit-unpacking — hand-tuned per ISA (NEON, AVX2, AVX-512, scalar), runtime-dispatched, and shipped with a public, PMU-instrumented, statistically rigorous performance ledger across microarchitectures.

**Mission:** Make the hot inner loops of columnar analytics a measured, portable, importable commons — so that no engine, student, or researcher has to reimplement or guess at them again.

**Target users:** engine and database builders (established and new); C++ systems needing analytical primitives without adopting Velox; performance researchers needing credible baselines (the missing "SIMD ledger" of Survey §11.4 #6); quant/HFT data-pipeline engineers; educators.

**Core philosophy:** Every kernel exists in a readable scalar reference form and ISA-specialized forms; every performance claim is reproducible from a published harness with disclosed methodology (DBTest 2018 + Berger-school practice as code — Survey §7); layouts are SIMD-legible by design (contiguous, padded, validity-separated — Survey §4.7); the library refuses to grow an engine.

**What it is:** a vendorable kernel library + dispatch layer + benchmark ledger + a thin pipeline-demo layer used only for end-to-end benchmark composition.

**What it is NOT:** not a SQL engine, not a DataFrame library, not a storage format, not a general SIMD abstraction (Highway/std::simd exist), not a scheduler, not a DuckDB/Velox competitor — it is what those systems would vendor.

**Major goals:** (1) the ten core kernels at state-of-the-art throughput on x86 and ARM with correctness fuzzed against scalar golden references; (2) the cross-ISA ledger as a citable artifact, including the bitmap-vs-selection-vector study (Survey §11.3 #3); (3) amalgamation/vendoring support and CMake/FetchContent hygiene; (4) one workshop-grade publication (DaMoN/ADMS) derived from the ledger; (5) at least one external system importing or vendoring it.

**Major non-goals:** query planning, expression languages, spilling, distributed anything, GPU kernels (v1), Windows-exotic ISA paths, format wars.

**Long-term vision:** v2 grows toward compressed-predicate kernels (C9's scope, once FastLanes stabilizes) and encoding-aware primitives; the ledger becomes the standing public record of "what analytical kernels cost on current CPUs," updated each hardware generation — infrastructure whose value compounds as microarchitectures churn.

---

## 14. Success Criteria

Measurable, time-boxed, honest about base rates (benchmarked against SimSIMD/CRoaring/fsst trajectories, not simdjson's outlier curve):

| Horizon | Criterion | Target | Stretch |
|---|---|---|---|
| 6 mo | Kernels shipped (scalar+AVX2+NEON) | 6 | 10 |
| 6 mo | Ledger v1 published (≥3 µarchs: Zen 4/5, Golden Cove-class, Apple M-series) | yes | + Graviton |
| 6 mo | GitHub stars | 150 | 500 (one successful launch post) |
| 12 mo | Stars | 600 | 1,500 |
| 12 mo | External contributors (merged PRs) | 3 | 10 |
| 12 mo | Workshop paper submitted (DaMoN/ADMS) | submitted | accepted |
| 12 mo | Conference talk (CppCon/CppNow/ACCU or DB meetup) | 1 accepted | 2 |
| 18 mo | External adoption: a named project vendoring/importing Quiver | 1 | 3 (one being an engine with >1k★) |
| 18 mo | Package presence (vcpkg + Conan) and downloads | listed; 1k/mo | 10k/mo |
| 18 mo | Forks | 60 | 200 |
| 18 mo | Citations (paper or ledger cited in papers/engineering blogs) | 2 | 10 |
| 18 mo | AVX-512 + dispatch complete; ledger covering ≥5 µarchs | yes | + SVE2 exploratory |
| Ongoing | Benchmark disputes resolved publicly with reproducible harness | 100% | — |
| Ongoing | Hiring signal: repo discussed in ≥1 interview loop at a §11 company | realized | offer influenced |

Failure criteria, defined in advance (per the survey's own methodology ethos): if at 12 months there are <200 stars, zero external users, and the ledger has attracted no engagement, the project pivots to its C9 expansion or is written up as a technical report and concluded — the skills demonstrated remain fully portfolio-valid.

---

*End of analysis. Next step, when authorized: architecture design and PRD for Quiver — explicitly out of scope here.*
