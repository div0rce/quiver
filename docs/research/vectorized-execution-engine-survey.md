# Vectorized Analytical Execution Engines: A Design-Space Survey

**A literature review and engineering survey preceding the design of a CPU-first, C++23 analytical execution engine**

Prepared July 2026. This document surveys the state of the art; it does not propose an architecture.

---

## How to read this document

Every substantive claim carries one of four epistemic labels:

| Label | Meaning |
|---|---|
| **[FACT]** | Established in a peer-reviewed paper, normative specification, or vendor documentation; primary source cited |
| **[IMPL]** | Public implementation detail from official docs, engineering blogs, or source code of a specific system |
| **[INFERENCE]** | Informed synthesis from cited facts; defensible but not directly stated in any single source |
| **[SPECULATION]** | Plausible but weakly sourced; flagged so it is never mistaken for fact |

Claims verified directly against primary-source PDFs during preparation of this survey include: the MonetDB/X100 CIDR 2005 paper (full text), the ClickHouse VLDB 2024 paper, the morsel-driven parallelism SIGMOD 2014 paper, the Photon SIGMOD 2022 paper, the "Fair Benchmarking Considered Difficult" DBTest 2018 paper, the Kersten et al. 2018 compiled-vs-vectorized paper, Umbra CIDR 2020, and the official docs/config pages of DuckDB, Velox, DataFusion, Polars, Arrow, and Google Benchmark. An adversarial verification pass was run on the highest-risk quantitative claims; corrections from that pass are incorporated inline.

Numbers describing CPU microarchitecture vary by chip; the microarchitecture is named wherever a number is given.

---

## Contents

1. Historical evolution of execution models
2. Existing systems (DuckDB, ClickHouse, Velox, Arrow, DataFusion, MonetDB, HyPer, Umbra, Vectorwise, Polars)
3. CPU architecture practicalities
4. SIMD
5. Memory allocators
6. Parallel execution
7. Benchmark methodology
8. Common mistakes
9. Recurring design patterns
10. Curated reading list
11. Synthesis: shared philosophies, disagreements, open questions, research gaps
12. Bibliography

---

# 1. Historical evolution of execution models

## 1.1 Timeline

```
1990 ── Volcano (Graefe): iterator model, exchange operator
        │   tuple-at-a-time, open/next/close
1993 ── MonetDB (CWI): column-at-a-time BAT algebra
        │   full materialization, operator-at-a-time
1999 ── "New Bottleneck: Memory Access" (Boncz/Manegold/Kersten)
        │   memory wall diagnosed; cache-conscious radix joins
2005 ── MonetDB/X100 (CIDR): vector-at-a-time          C-Store (VLDB)
        │   cache-resident ~1K vectors, selection vectors
2008 ── Vectorwise spin-off      "Column vs Row Stores" (SIGMOD)
2011 ── HyPer compilation (Neumann, VLDB): data-centric pipelines
        │   produce/consume, LLVM, pipeline breakers
2013 ── Micro Adaptivity in Vectorwise (SIGMOD)
2014 ── Morsel-Driven Parallelism (SIGMOD): NUMA-aware elastic scheduling
2016 ── Data Blocks (SIGMOD): SIMD scans over compressed cold data
2017 ── Relaxed Operator Fusion (VLDB): fusion is a dial, not a switch
2018 ── "Everything You Always Wanted to Know..." (VLDB): the settlement
        │   Adaptive Execution of Compiled Queries (ICDE)
2019 ── DuckDB (SIGMOD demo): embedded vectorized OLAP
2020 ── Umbra (CIDR): SSD-based, variable-size pages, tiered codegen
2021 ── Tidy Tuples & Flying Start (VLDBJ): millisecond codegen
2022 ── Velox (VLDB): execution as a reusable library     Photon (SIGMOD)
2023 ── "These Rows Are Made for Sorting" (ICDE): per-operator layouts
2024 ── DataFusion (SIGMOD): modular Arrow-native engine
        │   ClickHouse paper (VLDB); Arrow adds Utf8View/ListView/REE
2025 ── Polars streaming engine rewrite (async state machines + morsels)
```

## 1.2 Generation 1: Volcano (1990–1994)

**[FACT]** Graefe's Volcano system introduced two separable ideas. The 1990 SIGMOD paper contributed the **exchange operator**: parallelism encapsulated in a single meta-operator interposed between producer and consumer, leaving every other operator oblivious to parallelism (Graefe 1990). The 1994 TKDE paper consolidated the **iterator model**: every operator implements a uniform open/next/close interface, each `next()` producing one tuple, so operators compose into arbitrary trees (Graefe 1994).

**Why it appeared.** **[INFERENCE]** In the disk-bound early 1990s, per-tuple virtual-call overhead was noise relative to I/O. The model bought composability, pipelining (small intermediate footprint), and demand-driven scheduling — the right trade at the time.

**Why it aged badly.** **[FACT]** The X100 paper diagnosed the cost precisely: the Volcano iterator model "leads to tuple-at-a-time execution, which causes both high interpretation overhead, and hides opportunities for CPU parallelism from the compiler" (Boncz, Zukowski, Nes 2005). Neumann similarly notes iterator-style processing "shows poor performance on modern CPUs due to lack of locality and frequent instruction mispredictions" (Neumann 2011). **[FACT]** Ailamaki et al. had shown in 1999 that commercial DBMSs of the era spent roughly half their cycles stalled, dominated by L2 data misses and L1 instruction misses (Ailamaki et al. 1999).

**What survived.** **[INFERENCE]** The two contributions had very different lifespans. Tuple-at-a-time iteration is dead in analytical engines; the exchange operator lives on as every shuffle/repartition operator in distributed engines — and, notably, as DataFusion's defended single-node parallelism model (§2.5).

## 1.3 Generation 2: column-at-a-time — MonetDB (1993–)

**[FACT]** MonetDB stores each column as a BAT (Binary Association Table) and executes a closed two-column algebra (MIL) where each operator runs *to completion* over its entire input before the next begins. Interpretation overhead is paid once per column operation, not per tuple, and operator kernels become tight, function-call-free loops that compilers optimize well (Boncz & Kersten 1999; Idreos et al. 2012). **[FACT]** The companion 1999 VLDB paper "Database Architecture Optimized for the New Bottleneck: Memory Access" is the founding document of hardware-conscious database architecture: it demonstrated that main-memory access had become the bottleneck and introduced cache-conscious radix-partitioned hash joins (Boncz, Manegold, Kersten 1999).

**The materialization problem.** **[FACT]** X100's own measurements of MonetDB/MIL on TPC-H Q1 showed nearly all MIL operators memory-bound rather than CPU-bound: full column materialization forces every intermediate through RAM. Running the same query on cache-resident data made MonetDB/MIL almost twice as fast; MIL "materializes much more data than strictly necessary" (Boncz, Zukowski, Nes 2005).

**[INFERENCE]** Generation 2's lesson: column-at-a-time killed per-tuple interpretation overhead but replaced it with a bandwidth tax. The obvious synthesis — column execution with pipelined incremental materialization — is exactly Generation 3.

**Parallel lineage.** **[FACT]** C-Store (Stonebraker et al. 2005) independently established read-optimized column storage (compression, projections) and became Vertica; the Abadi et al. survey (2013) unifies the MonetDB / VectorWise / C-Store lineages. **[FACT]** Abadi, Madden & Hachem (SIGMOD 2008) showed that simulating a column store on a row store does not recover column-store performance: the gains come from the execution architecture — compression-aware operators, late materialization, block iteration — not merely storage layout. **[INFERENCE]** That result is the intellectual justification for treating "the execution engine" as a field of its own, separable from storage.

## 1.4 Generation 3: vector-at-a-time — MonetDB/X100 → Vectorwise (2005–2012)

**[FACT]** X100's core idea, quoting the paper: "combine the column-wise execution of MonetDB with the incremental materialization offered by Volcano-style pipelining." Execution is Volcano-like pipelining at the granularity of a vector (~1000 values) of cache-resident data — "the CPU cache is the only place where bandwidth does not matter" (Boncz, Zukowski, Nes 2005). Key mechanisms, all from the primary source:

- **Primitives.** Hundreds of generated, typed, loop-shaped kernels (e.g. `map_mul_flt_col_flt_col`) whose bodies expose tuple independence to the compiler. Measured: vectorized multiplication at 2.2 CPU cycles/tuple vs. 49 cycles/tuple in bandwidth-constrained MonetDB/MIL. Compound (fused) primitives gave a further ~2× by keeping intermediates in registers.
- **Selection vectors.** Filters emit an index vector of qualifying positions; all primitives accept selection vectors and compute only at those positions, avoiding compaction copies.
- **Vector size.** Performance improves with vector size until all live vectors of a query no longer fit in cache, then degrades; at vector size 1 the engine behaves like MySQL, at 4M-tuple vectors like MonetDB. The optimum was roughly 1K–4K values. Full materialization is just vectorized execution with infinite vector size.

**[FACT]** On 100 GB TPC-H, X100 reported raw execution one to two orders of magnitude beyond prior technology. The product story — Vectorwise, later Actian Vector — and production refinements are described in Zukowski & Boncz (2012); Zukowski's 2009 PhD thesis is the definitive long-form treatment, including the PFOR compression family.

**Tradeoffs.** **[INFERENCE]** Vectorization amortizes interpretation, restores compiler optimizability, and keeps intermediates cache-resident — but still materializes small intermediates at operator boundaries and runs each primitive as a separate pass. That residual cost is what Generation 4 attacked.

## 1.5 Generation 4: data-centric compilation — HyPer (2011)

**[FACT]** Neumann's VLDB 2011 paper inverted the frame: with data in memory, performance is determined by raw CPU cost, so compile each query to machine code organized around **data-centric pipelines**. A *pipeline breaker* is an operator that must take a tuple out of registers (hash-join build, aggregation, sort); everything between breakers is fused into one tight loop keeping the tuple in registers. Code generation walks the plan with a **produce/consume** interface — a push model established at codegen time — emitting LLVM IR mixed with precompiled C++ (Neumann 2011). **[IMPL]** Spark 2.0's whole-stage code generation explicitly credits this paper (Databricks 2016); HyPer itself was acquired by Tableau.

**Tradeoffs.** **[FACT]** Compilation latency (hundreds of milliseconds for complex queries even with LLVM — measured in Kohn et al. 2018), harder debugging and profiling of generated code, and less runtime adaptivity.

## 1.6 The debate and its settlement (2011–2018)

**[FACT]** Sompolski, Zukowski & Boncz (DaMoN 2011) argued early that many claimed benefits of compilation were already delivered by vectorization, that compilation without vectorization can be inferior (branch-heavy fused loops vs. branch-free vectorized primitives), and that the best designs combine both.

**[FACT]** The definitive controlled comparison is Kersten et al. (VLDB 2018), which reimplemented both paradigms — "Typer" (HyPer-style) and "Tectorwise" (Vectorwise-style) — with identical algorithms and parallelization. Findings, verified against the paper: both are efficient and within the same performance envelope; **data-centric compilation executes fewer instructions and wins computation-heavy, cache-resident work; vectorization is better at hiding cache-miss latency** (inter-tuple parallelism creates concurrent outstanding loads, e.g. hash-join probes), compiles the engine once, and is easier to profile and adapt. **[FACT]** A sobering datapoint from the same line of work, highlighted in CMU 15-721's notes: hand-optimized SIMD gives 10–130% gains on individual operators but roughly 10% end-to-end, because the vectorizable fraction of a query is limited.

**[FACT]** Menon, Mowry & Pavlo's Relaxed Operator Fusion (VLDB 2017) showed the dichotomy is false at the architectural level: total fusion forecloses SIMD and software prefetching; decomposing compiled pipelines into stages separated by cache-resident buffers ("tactical materialization") beat both pure approaches by up to 2.2×. **[INFERENCE]** Fusion is a dial, not a switch — an idea visible today in Photon's fused operators for common cases and in Umbra-style engines emitting vectorized kernels from generated code.

## 1.7 Morsel-driven parallelism (2014)

**[FACT]** Leis, Boncz, Kemper & Neumann (SIGMOD 2014): work is broken into ~100,000-tuple **morsels** dispatched to a fixed pool of workers pinned to cores. Degree of parallelism is not baked into the plan but changes elastically mid-query; the dispatcher preserves NUMA locality (workers prefer NUMA-local morsels, write NUMA-local results, and steal — preferring nearby sockets — only when local work runs out). Average speedup exceeded 30× on 32 cores on TPC-H/SSB. **[INFERENCE]** Volcano's exchange parallelism fixes parallelism at plan time, suffers skew-induced imbalance, and is NUMA-blind; morsel scheduling replaces "parallelism as an operator" with "parallelism as a runtime scheduling policy," and composes orthogonally with either execution model — compiled pipelines (HyPer, Umbra) or vectors (DuckDB, Velox, Polars' new engine).

## 1.8 Compilation with low latency: adaptive execution and Umbra (2018–2021)

**[FACT]** Kohn, Leis & Neumann (ICDE 2018): generate LLVM IR, begin executing immediately on a custom bytecode interpreter for LLVM IR, compile optimized code in the background, and switch over at morsel boundaries — beating every statically chosen mode across 10 MB–30 GB workloads. **[FACT]** Umbra (Neumann & Freitag, CIDR 2020) dropped HyPer's pure in-memory assumption: a low-overhead buffer manager with variable-size pages (exponentially growing size classes, each backed by a reserved virtual-memory region the size of the whole pool) delivers in-memory performance for cached working sets on SSD-resident data. **[FACT]** Tidy Tuples and Flying Start (Kersten, Leis, Neumann, VLDBJ 2021) distilled the codegen lessons: a single-pass, type-safe code generator into a custom lightweight IR, plus a direct-emit x86 backend compiling roughly as cheaply as bytecode generation while executing on par with LLVM-generated code. Flying Start became the default tier.

**[INFERENCE]** Umbra removed compilation's last practical objection — latency — via JavaScript-VM-style tiering. In 2026, "compilation is too slow for ad hoc queries" is no longer a valid blanket claim; the real remaining barrier is the engineering cost of maintaining a codegen stack, which is exactly the reason Photon's authors give for choosing vectorization (Behm et al. 2022).

## 1.9 Push vs. pull, formally

**[FACT]** Shaikhha, Dashti & Koch (JFP 2018) connect query pipelining to loop-fusion theory: pull (iterator) and push (data-centric) engines are dual fusion strategies with distinct limitations — pull handles limit and merge-join naturally but has costly nested control flow; push struggles with limits and merging multiple inputs. **[INFERENCE]** Direction and granularity are independent axes, and the field has occupied all four quadrants: Volcano = pull × tuple; X100 = pull × vector; HyPer = push × tuple-in-register; DuckDB/Velox = push × vector.

## 1.10 The modern generation (2019–2026)

**[FACT]** Among engines started in the last decade, vectorized interpretation is the default: DuckDB (2019), Velox (2022), Photon (2022), DataFusion (2024 paper), ClickHouse (which describes itself as using "the same vectorization model as MonetDB/X100" — Schulze et al. 2024). Photon's authors state they chose "the vectorized-interpreted model in lieu of code generation, unlike Spark SQL," despite acknowledging codegen sometimes wins, because the interpreted engine was easier to build, profile, and debug, and because dynamic dispatch made **batch-level runtime adaptivity** natural (per-batch specialization for NULL-free or ASCII-only data, crucial for uncurated lakehouse data) (Behm et al. 2022, read from the PDF). Data-centric compilation survives where compiler expertise is concentrated: Umbra/CedarDB, Hyper/Tableau, and JVM-level whole-stage codegen in Spark.

**[FACT]** Refinements continue inside the vectorized consensus: row-format conversion for sorting beats columnar comparators (Kuiper & Mühleisen 2023, implemented in DuckDB); execution over compressed/dictionary vectors (DuckDB, Velox); engine componentization (Velox and DataFusion as libraries; Arrow as the boundary format).

**[INFERENCE]** The generational driver has always been **bottleneck migration**: disk I/O (Volcano era) → memory latency/bandwidth (MonetDB's 1999 diagnosis) → CPU interpretation overhead (X100) → residual instruction count and register locality (HyPer) → cores and NUMA (morsels) → compilation latency and engineering cost (adaptive execution, Umbra, Photon). Each generation is a synthesis, not a replacement. **[SPECULATION]** Given that Photon, Velox, DuckDB, and DataFusion all chose interpretation-plus-vectors while Umbra-class codegen remains confined to teams with compiler depth, the likely equilibrium is vectorized engines that JIT only small hot expressions — the Sompolski 2011 "combine both" position winning on engineering economics rather than microarchitectural grounds.

## 1.11 Execution-model comparison

| Model | Grain | Control flow | Interpretation overhead | Cache behavior | SIMD path | Weakness |
|---|---|---|---|---|---|---|
| Volcano iterator | tuple | pull | per-tuple virtual calls | poor locality, mispredicts | none | dominated by dispatch |
| Column-at-a-time (MonetDB) | full column | operator-at-a-time | per-operator (negligible) | intermediates spill to RAM | auto-vec of kernels | bandwidth tax of materialization |
| Vector-at-a-time (X100) | ~1–4K values | pull (originally) | per-vector (amortized ~1000×) | intermediates stay in L1/L2 | auto-vec + explicit primitives | per-operator pass overhead |
| Data-centric compiled (HyPer) | tuple in registers | push (codegen) | none after compile | best register locality | LLVM auto-vec | compile latency, debuggability, adaptivity |
| Relaxed fusion (ROF) | staged vectors | push | low | staged cache-resident buffers | SIMD within stages | complexity |
| Modern vectorized push (DuckDB/Velox) | ~1–10K vectors in ~100K morsels | push | per-vector | cache-resident vectors | mixed (§4) | residual interpretation cost |


---

# 2. Existing systems

## 2.1 At a glance

| System | Scope | Execution model | Batch unit | Internal format | SIMD philosophy | Parallelism | Memory governance |
|---|---|---|---|---|---|---|---|
| **DuckDB** | full in-process DBMS | push-based vectorized pipelines | 2048-tuple DataChunk | own vectors (flat/const/dict/seq) + selection vectors, `string_t` | auto-vectorization only | morsel-driven-style task scheduler | global limit + buffer manager + adaptive spill |
| **ClickHouse** | full server DBMS | vectorized processors/ports state machines + optional JIT | Block/Chunk (granule 8192 rows) | IColumn/PODArray, LowCardinality, Nullable | explicit intrinsics + auto-vec, cpuid runtime dispatch | plan-baked lanes + work stealing, elastic DOP | jemalloc + Arena + per-query tracking |
| **Velox** | C++ execution library | push drivers/pipelines, async futures | variable RowVector | Velox vectors (out-of-order writable), StringView, lazy vectors | xsimd + hand-written kernels | drivers per pipeline; embedder schedules | hierarchical pools + arbitration + spill |
| **Arrow (C++/Acero)** | format + kernels + streaming engine | push ExecNode DAG | ExecBatch | Arrow columnar spec | kernel-level, mixed | per-node parallelism | MemoryPool (jemalloc/mimalloc) |
| **DataFusion** | embeddable Rust engine toolkit | pull-based async streams (Volcano + exchange) | 8192-row RecordBatch | Arrow (arrow-rs), StringView | auto-vectorization (removed explicit SIMD) | RepartitionExec partitions on Tokio work-stealing | reservation MemoryPool + per-op spill |
| **MonetDB** | full DBMS (research lineage) | operator-at-a-time BAT algebra | whole column | BATs, mmap-backed, C arrays | auto-vec of bulk kernels | MAL-level partitioning | OS paging via mmap |
| **HyPer** | main-memory hybrid DBMS | data-centric LLVM-compiled pipelines | tuple-in-registers (morsels for scheduling) | in-memory tuples + compressed Data Blocks (cold) | LLVM auto-vec + hand-SIMD Data Block scans | morsel-driven, NUMA-aware (origin) | in-memory; NUMA-local areas |
| **Umbra** | SSD-based DBMS | compiled pipelines as modular state-machine steps; tiered backends | morsels; steps | B+-trees of PAX pages, German strings | codegen (LLVM tier) | morsel-driven steps | variable-size-page buffer manager (mmap size classes) |
| **Vectorwise** | commercial DBMS (X100) | vector-at-a-time primitives | ~1024-value vectors | PAX-based storage, NULLs as separate bool columns | explicit SIMD in primitives; micro-adaptive flavor choice | exchange operators (plan-driven) | buffer pool with compressed blocks |
| **Polars** | DataFrame engine (Rust) | new: morsel-driven + async state machines (hybrid push/pull); old: materialized + rayon | morsels (~10⁵ rows, unofficial) | polars-arrow fork; view strings | Rust std::simd (nightly) + auto-vec | rayon (old); own scheduler (new) | backpressure + spillable sinks |

Details and sources follow. All entries are [IMPL] unless marked otherwise.

## 2.2 DuckDB

**Scope.** [FACT] A full in-process analytical DBMS — parser (Postgres-derived), optimizer, vectorized engine, MVCC, columnar storage — positioned as "SQLite for analytics" (Raasveldt & Mühleisen 2019). [IMPL] Zero external dependencies; single-file amalgamation build (duckdb.org/why_duckdb).

**Execution model.** [IMPL] Originally pull-based "Vector Volcano"; reworked to a **push-based model** starting with GitHub issue #1583 (2021) because pipeline parallelism had made pull-based control flow unmanageable. Plans decompose into pipelines (source → operators → sink); sinks are pipeline breakers; DataChunks of up to 2048 tuples are pushed through; operators expose state (`NEED_MORE_INPUT`, `HAVE_MORE_OUTPUT`). Stated motivations: centralized scheduling, operator-level parallelism, scan sharing, async I/O, cancellation (github.com/duckdb/duckdb/issues/1583; Raasveldt, "Push-Based Execution in DuckDB," DSDSD talk).

**Memory layout.** [IMPL] Vector formats: Flat, Constant, Dictionary (child + selection), Sequence. Decompression can emit constant/dictionary vectors directly — compressed execution. A generic `UnifiedVectorFormat` view (data pointer + selection vector + validity mask) tames the combinatorial explosion of format-specialized kernels. Strings are 16-byte `string_t` (Umbra-style: 4-byte length; ≤12 bytes inlined, else 4-byte prefix + pointer). Nested types are recursive child vectors (duckdb.org/docs/internals/vector).

**SIMD.** [IMPL] Deliberately **no explicit intrinsics** — the FAQ states explicit SIMD "greatly complicate[s] portability and compilation"; DuckDB writes "implicit SIMD" code shaped for auto-vectorizers, citing a ~10-minute port to Apple Silicon as the payoff (duckdb.org/faq). [INFERENCE] This works because vectorized kernels — tight typed loops over 2048-value arrays — are exactly the shape auto-vectorizers handle; the trade is a few percent of peak for portability across x86/ARM/Wasm.

**Parallelism.** [IMPL] Pipelines are split among worker threads consuming source partitions (morsel-driven-inspired); parallel sinks combine thread-local state — e.g. radix-partitioned parallel aggregate hash tables (duckdb.org/2022/03/07/aggregate-hashtable). [INFERENCE] "Morsel-driven-ish": full HyPer-style NUMA-aware stealing is not replicated, but row-group-granular dispatch captures most of the benefit single-node.

**Memory management.** [IMPL] Three legs: streaming execution; adaptive operator spilling (external hash agg/sort/window); a buffer manager caching fixed-size 256 KB blocks — all under one global `memory_limit`, **default 80% of RAM** (verified; duckdb.org/2024/07/09/memory-management). jemalloc is bundled on Linux to fight fragmentation.

**Benchmarks.** [IMPL] In-repo regression benchmark runner; operates the revived H2O.ai db-benchmark — notably moved to a bare-metal c6id.metal instance after diagnosing noisy cloud-VM results; longitudinal self-benchmarking across releases (duckdb.org/2023/04/14/h2oai; /2024/06/26/benchmarks-over-time).

**Lessons.** [INFERENCE] (1) Pull-based control flow and intra-query parallelism mix badly — issue #1583 is the canonical war story. (2) A unified vector view is the practical answer to encoding combinatorics. (3) Memory limits must be global across buffer pool and operators, with graceful spill.

## 2.3 ClickHouse

**Scope.** [FACT] Columnar OLAP server for trillion-row tables; MergeTree LSM-like storage of immutable sorted parts, one file per column, 8192-row granules in ~1 MB compressed blocks; sparse primary index plus min-max/set/Bloom skipping indices (Schulze et al., VLDB 2024 — verified).

**Execution model.** [FACT] "The same vectorization model as MonetDB/X100," plus opportunistic JIT: LLVM compilation of hot expressions and aggregation, triggered after a recurrence threshold, cached across queries. [IMPL] The processors/ports rework: each operator is a **state machine** with input/output ports; `prepare()` does O(1) bookkeeping and returns a status (`NeedData`, `PortFull`, `Ready`, `Async`, …); `work()` does the CPU-heavy part; processors can rewrite the pipeline at runtime, e.g. to spawn external-memory variants (IProcessor.h). [FACT] Worker threads traverse the plan performing state transitions; there is no central scheduler; plans are unfolded into parallel lanes with exchange-style `Repartition`/`Gather` operators. The paper explicitly contrasts this with morsel-driven parallelism: similar work stealing and decentralization, but much larger ranges than 100K-row morsels, and max DOP baked into the plan (with mid-query DOP changes supported).

**Memory layout.** [IMPL] `IColumn` with immutable transforms (`filter`, `permute`, `cut`); numeric columns are contiguous `PODArray`s — uninitialized-by-default, padded 15 bytes on the right so unaligned SIMD reads/writes over the tail are safe; `ColumnString` = contiguous bytes + offsets; `LowCardinality(T)` dictionary wrapper; `Nullable(T)` adds a separate byte-mask column. "Leaky abstractions" let functions specialize on concrete layouts (docs; PODArray.h).

**SIMD.** [FACT] Manual intrinsics plus auto-vectorization, compiled into multiple kernels (e.g. scalar, auto-vec AVX2, hand-written AVX-512) selected at runtime via cpuid — portable binary with SSE4.2 floor. [IMPL] The multitarget framework reports typical speedups of 1.15–2×, up to ~7× (maksimkita.com/blog/cpu-dispatch-in-clickhouse).

**Parallelism & resources.** [FACT] Elastic per-query CPU slots (`ConcurrencyControl`), per-server/user/query memory tracking with overcommit, spill-triggering limits, I/O scheduling. Parallel hash join via shared partitioned tables with `Gather` synchronization; >30 hash-table instantiations from one template (two-level 256-subtable layout, string hash tables per length class, size prediction from history, `__builtin_prefetch`). [FACT] Self-acknowledged weakness (as of v24.6): optimizer immaturity — no join reordering or subquery decorrelation.

**Allocators.** [FACT] Switched tcmalloc→jemalloc (PR #2773, verified: "some queries will run faster up to 20%... RSS about 10% lower"); `Arena` bump allocator for aggregation states and short strings (chunked, exponential growth to a 128 MB linear threshold); `ArenaWithFreeLists` where partial frees are needed. [INFERENCE] Three tiers — global jemalloc, PODArray buffers, lifetime-batched arenas — a pattern that recurs across engines.

**Benchmarks.** [FACT] ClickBench (43 queries, ~100M-row denormalized web table): 3 runs, hot = min of runs 2–3; per-query ratio to best +10 ms; geometric mean; explicitly documented limitations (single flat table, no joins to speak of, no concurrency). The paper concedes Umbra achieves the best overall hot runtime while ClickHouse leads production-grade systems (verified). VersionsBench tracks per-release regressions (1.72× improvement 2018–2024).

**Lessons.** [INFERENCE] Interpretation plus selective JIT of hot expressions captures most of compilation's benefit at a fraction of the complexity. Runtime ISA dispatch is the price of shipping one binary. A state-machine operator protocol subsumes async I/O, spilling, and runtime pipeline rewriting.

## 2.4 Velox

**Scope.** [FACT] Not a database: a C++ execution **library** — type system, vectors, expression eval, operators, I/O connectors, memory management — with no parser or optimizer, built to consolidate redundant engines across Presto, Spark (Gluten), streaming, and ML preprocessing at Meta (Pedreira et al., VLDB 2022).

**Execution model.** [FACT/IMPL] Task → pipelines → **Drivers** (threads of execution); push-style within a driver; drivers move on/off threads when blocked, expressed via futures — explicitly easier than suspending a Volcano iterator tree (velox docs: task.html). [INFERENCE] A middle point: pipeline decomposition like DuckDB, plus async-first blocking for remote-storage-heavy deployments; global scheduling is externalized to the embedder.

**Memory layout.** [IMPL] Vectors "similar to Arrow, but feature more encodings and a different layout for strings, arrays and maps which support **out-of-order writes**" — the docs call out-of-order writability the main difference from Arrow (needed for conditional expression evaluation). Encodings: Flat, Constant, Dictionary, Bias, Sequence, arbitrarily nested. Strings: 16-byte StringView (≤12 B inlined; 4-byte prefix + pointer), multiple string buffers, zero-copy substr/split. Arrays/maps carry offsets **and sizes** (permitting out-of-order/overlapping ranges). Dictionary vectors represent filter results without copying ("peeling" shared encodings in expression eval). [FACT] **Lazy vectors** defer materialization so columns eliminated by earlier conjuncts are never decoded (paper). [FACT] Meta and Voltron Data drove Arrow 15's StringView/ListView/REE additions to make Velox↔Arrow zero-copy (engineering.fb.com, Feb 2024).

**SIMD.** [IMPL] Explicit SIMD throughout via **xsimd**, plus hand-written primitives (e.g. lane compaction in SimdUtil.h). [FACT — corrected during verification] The quantitative evidence comes from an independent ADMS 2023 study (Benson, Ebeling & Rabl): removing all explicit SIMD from Velox cost only ~4% overall on TPC-H SF1, with the largest single-query benefit ~25% (Q18 on x86); the same study found compiler intrinsics and xsimd within ~1% of each other. [INFERENCE] Explicit SIMD is a targeted tool: a handful of primitives carry nearly all the benefit.

**Parallelism.** [IMPL] Multiple drivers per pipeline; local exchanges repartition inside a task; the host system supplies inter-fragment exchange and admission control.

**Memory management.** [IMPL] The most componentized in this survey: MemoryManager → hierarchical MemoryPools (query → task → node → operator, giving per-operator accounting) → MemoryArbitrator (dynamic capacity transfer between queries, spill triggering) + AsyncDataCache (RAM + SSD). Arena allocation (`HashStringAllocator`) for accumulators and hash-table payloads (velox docs: memory.html, arena.html).

**Benchmarks.** [FACT] Paper evaluation: TPC-H-derived plus production shadow testing against Presto Java (2–10× on CPU-bound fragments). [IMPL] Per-kernel Google Benchmark microbenchmarks in-repo.

**Lessons.** [INFERENCE] (1) Encodings as first-class execution citizens (dictionary peeling, constant propagation, lazy vectors) pay off across operators. (2) Out-of-order-writable vectors are the concrete technical reason an execution engine may reject vanilla Arrow internally. (3) Hierarchical pools + arbitration is the scalable answer to memory accounting. (4) A library without an optimizer inherits its embedder's plan quality.

## 2.5 Apache DataFusion

**Scope.** [FACT] An embeddable, modular Rust query engine using Arrow as its native memory model: SQL + DataFrame frontends, optimizers, streaming executor, and 10+ extension APIs; top-level Apache project since 2024 (Lamb et al., SIGMOD 2024).

**Execution model.** [IMPL] `ExecutionPlan::execute(partition)` returns an async **pull-based stream** of RecordBatches (default batch size **8192**, verified). Parallelism is Volcano-with-exchange: `RepartitionExec` rebalances partitions (target = core count); Tokio's work-stealing runtime multiplexes the tasks. The docs defend this explicitly, verified: "in practice DataFusion achieves similar scalability as systems that use push driven schedulers such as DuckDB." [FACT] The community prototyped a morsel-driven push scheduler (issues #2199, #2504) and abandoned it — a rare documented reversal. [IMPL] Using Tokio for CPU-bound work is a deliberate, argued position (Lamb: async/await continuations + work stealing are competitive), with a documented footgun: CPU work must be isolated from I/O runtimes or it starves network I/O.

**Memory layout.** [IMPL] Arrow arrays are the execution format; no selection vectors — filters materialize. Compensations: high-quality kernels in arrow-rs, late materialization in the Parquet reader, StringView adoption. [INFERENCE] DataFusion is the strongest counterexample to "you cannot execute efficiently on Arrow" — it accepts Arrow's constraints and pushes the work into kernels and the scan.

**SIMD.** [IMPL] arrow-rs **removed** its explicit `packed_simd` kernels after benchmarks showed auto-vectorization matched or beat them (apache/arrow-rs PR #1221). Same camp as DuckDB.

**Memory management.** [IMPL] Reservation-based `MemoryPool` (greedy/fair-spill variants) + DiskManager for spilling; no bundled allocator (deployments commonly enable mimalloc — [INFERENCE from build configs]).

**Benchmarks.** [IMPL] ClickBench as an explicit engineering program — fastest single-node engine querying Parquet on ClickBench as of Nov 2024, credited to Parquet reader work (predicate pushdown, page index, bloom filters, late materialization, StringView); in-repo TPC-H-derived + criterion microbenchmarks + sqllogictest.

**Lessons.** [INFERENCE] (1) You can outsource scheduling to a general async runtime if operators are streams — enormous engineering savings. (2) "Volcano-with-exchange scales fine on modern single nodes" is now an empirically defended position complicating morsel orthodoxy. (3) Investing in the file-format reader can buy more end-to-end performance than operator micro-optimization.

## 2.6 Apache Arrow (format + compute)

**Format.** [FACT] The columnar spec guarantees data adjacency, O(1) random access (except run-end-encoded layouts), SIMD-friendly layouts, and relocatability without pointer swizzling. Buffers should be allocated at 8- or 64-byte alignment with padding to multiples of 8/64 bytes; the stated rationale is AVX-512 register width and cache-line granularity. Validity is bit-packed, 1 = valid, omittable when null count is 0. Variable-size binary uses signed 32/64-bit monotone offsets; **Utf8View/BinaryView** (Format 1.4) adopts the 16-byte Umbra-style view ("adapted from TU Munich's UmbraDB" per the spec); ListView adds offsets+sizes; run-end encoding and dictionary encoding are first-class (arrow.apache.org/docs/format/Columnar.html).

**Interchange vs execution.** [IMPL] Arrow positions itself as a shared foundation — interchange via C Data Interface, IPC, Flight, ADBC — not an execution engine. DuckDB keeps its own vectors and scans Arrow zero-copy at the boundary; Velox documents precisely why (no out-of-order writes; string/list layout); Polars forked its Arrow implementation to change the string type ahead of the spec. [FACT] An academic treatment of Arrow's execution-time weaknesses: "Data Formats in Analytical DBMSs: Performance Trade-offs and Future Directions" (arXiv 2411.14331). [INFERENCE] Consensus circa 2026: Arrow won as the *boundary* format; purpose-built engines keep private freedoms internally. The 2023–2024 spec additions (views, ListView, REE) are the format chasing the engines.

**Compute layers.** [IMPL] C++ compute kernels; **Acero** streaming ExecNode DAG (explicitly "not a full database"); **Gandiva** LLVM expression compiler (projections/filters, donated by Dremio 2018). [INFERENCE] Neither Acero nor Gandiva achieved the adoption of DuckDB/Velox/DataFusion; the ecosystem's execution gravity moved to the engines, with Arrow retaining format + kernels + transport. [SPECULATION] Acero's maintenance investment appears modest relative to DataFusion's, judging by release-note volume.

## 2.7 MonetDB

Covered historically in §1.3. Additional engineering profile: [FACT] fixed-width columns are plain C arrays; strings are dictionary-like (heap + offsets); memory-mapped files unify on-disk and in-memory representation, delegating buffer management to OS paging; late tuple reconstruction. Runtime operational optimization: operators see whole inputs plus properties, choosing scan vs binary search vs hash per call. Contributions layered on the kernel: radix-cluster joins, generic cache-hierarchy cost models (Manegold), database cracking (adaptive indexing), intermediate recycling (Idreos et al. 2012). [FACT] Umbra's 2020 evaluation found MonetDB spending large fractions of runtime in optimization/codegen and occasionally picking "extremely bad" plans. [INFERENCE] Lessons: operator-at-a-time is a dead end for large intermediates, but bulk kernels + property-based runtime algorithm selection survived into every successor; ceding paging to the OS is elegant but surrenders eviction control.

## 2.8 HyPer

Covered historically in §1.5–1.7. Engineering profile: [FACT] data-centric LLVM compilation (produce/consume); morsel-driven NUMA-aware scheduling with pinned workers; two-phase join build (thread-local materialization, then a perfectly sized global lock-free table built with CAS; tagged pointers double as an embedded Bloom-like filter); two-phase aggregation (cache-resident thread-local pre-aggregation with overflow partitions, then partition-wise merge, following IBM BLU). [FACT] **Data Blocks** (SIGMOD 2016): compressed, byte-addressable cold storage (single-value, ordered dictionary, truncation) with SMAs and Positional SMAs; SARGable predicates evaluated with SIMD directly on compressed data; an *interpreted vectorized scan* feeds the compiled pipelines because compiling every physical representation would explode generated code — an explicit vectorization/compilation hybrid within one system. [INFERENCE] SIMD split: codegen (LLVM auto-vec) for control-heavy pipeline logic; hand-SIMD for scan kernels. Weaknesses that motivated Umbra: in-memory economics and compile latency (Tableau observed multi-second compiles).

## 2.9 Umbra

Covered historically in §1.8. Engineering profile: [FACT] Variable-size-page buffer manager: size classes 64 KiB…pool-size, each class backed by a reserved anonymous-mmap virtual region as large as the whole pool; eviction uses `madvise(MADV_DONTNEED)`; pointer swizzling (tagged 64-bit swips, single owner per page); versioned optimistic latches (59-bit version + 5 state bits); relations as B+-trees over synthetic TIDs with PAX leaves. Buffer-manager overhead ≤~6% vs raw mmap; geometric-mean speedups vs HyPer of 3.0× on JOB (mostly avoided compilation) and 1.8× on TPC-H (CIDR 2020). [FACT] Pipelines decompose into **steps** (single- or multi-threaded), each a generated function orchestrated by the executor, enabling suspension between steps and per-step morsel dispatch. Code is generated into a custom lightweight **Umbra IR** (LLVM-IR-like, linear-time lowering), not LLVM directly. Backends: bytecode VM → **Flying Start** direct x86 emitter (default; ~bytecode-cheap compilation, ~LLVM-grade code) → optimizing LLVM for long-running steps (Tidy Tuples, VLDBJ 2021). [FACT] **German strings**: 16-byte header, 4-byte length, ≤12 B inline, else 4-byte prefix + pointer with 2 storage-class bits (persistent/transient/temporary) — adopted by DuckDB, Arrow (views), Polars, Velox (cedardb.com/blog/german_strings). Statistics: online reservoir samples + updateable per-column HyperLogLog. [SPECULATION] Per-query scratch is likely arena-organized per pipeline step, consistent with the HyPer lineage, but no public document details Umbra's query-memory allocator.

## 2.10 Vectorwise (Actian Vector)

Covered historically in §1.4. Engineering profile: [FACT] Product-era refinements (Zukowski & Boncz 2012): PAX-based generalized row/column storage; NULLs as separate boolean columns explicitly because "the presence of NULLs prevents the use of SIMD instructions"; scan-speed-oriented compression (PFOR/PFOR-DELTA/PDICT), data kept compressed in the buffer pool; automatic MinMax indices on all columns; Positional Delta Trees for snapshot-isolated updates merged by position; cooperative scans. NSM (row) layout used inside execution where access patterns favor it — mostly hash tables. Selective JIT of complex predicates only where it beats vectorized execution (Sompolski et al. 2011). [FACT] **Micro adaptivity** (Răducanu, Boncz, Zukowski, SIGMOD 2013): keep multiple compiled "flavors" of each primitive (compilers, flags, algorithmic variants — branching vs predicated) and choose per call with an ε-greedy multi-armed bandit, because flavor ranking depends on hardware, selectivity, and distribution in ways impossible to model statically. [FACT] Parallelism via exchange operators — the plan-driven model the morsel paper criticizes. [INFERENCE] Lessons: ~1K vectors sized to cache is the load-bearing constant; per-primitive adaptive dispatch is a cheap, general mechanism; strict columnarity inside execution is not sacred.

## 2.11 Polars

**Scope.** [IMPL] "A query engine with a DataFrame frontend": lazy plans, optimizer (pushdowns, CSE), physical planner, Rust kernels; Python/R/JS bindings; GPU backend (cuDF) and distributed offering on the same API (pola.rs/posts/polars_birds_eye_view).

**Execution model.** [IMPL] Three generations: classic in-memory engine (materialized, rayon-parallel); deprecated first streaming engine; and the **new streaming engine** (2024–2025, tracking issue #20947): **morsel-driven parallelism + Rust async state machines** — operators written as async functions that rustc compiles into resumable state machines, morsels flowing through a graph with bounded channels for exact backpressure; a "hybrid push/pull" design per the team. Unsupported operators fall back per-subplan to the in-memory engine. ~3–7× faster than the in-memory engine on PDS-H as data grows (pola.rs blog posts; issue #20947). [SPECULATION] Third-party analyses report ~10⁵-row morsels and spillable sink partitions; official docs don't confirm exact defaults.

**Memory layout.** [IMPL] Built on **polars-arrow**, an in-repo fork of the Arrow2 crate — forked precisely to change the string type ahead of the spec. The January 2024 string rewrite (verified) replaced offsets+data with 16-byte Umbra-style views, citing O(n·k) gather/filter cost on offset-based strings; documented costs: +8 B per long string and garbage-collection heuristics for dead buffer regions (pola.rs/posts/polars-string-type).

**SIMD.** [IMPL] Mix of auto-vectorization and explicit portable SIMD via Rust `std::simd` (nightly feature, used by official wheels). [INFERENCE] Between DuckDB (pure auto-vec) and Velox (systematic xsimd).

**Parallelism.** [IMPL] Old engine: rayon work-stealing, per-expression parallelism. New engine: its own scheduler over the async task graph, explicitly citing the morsel paper.

**Allocators.** [IMPL] Ships jemalloc (Linux/macOS) / mimalloc (Windows) as global allocator; docs claim up to ~25% runtime impact from allocator choice. [INFERENCE] No global query-memory governance comparable to DuckDB/Velox — discipline is per-operator + backpressure; a documented-by-omission weakness.

**Benchmarks.** [IMPL] PDS-H — TPC-H-derived, explicitly "not an officially audited TPC-H benchmark" — run against DuckDB/DataFusion/pandas/Dask/PySpark; also a headline participant in db-benchmark. [INFERENCE] Note the symmetry: DuckDB operates db-benchmark, Polars operates PDS-H, ClickHouse operates ClickBench. Treat each project's flagship numbers as adversarially collocated evidence.

**Lessons.** [INFERENCE] (1) Offset-based varlen layouts are hostile to row-movement-heavy operators — adopt view strings from day one. (2) Letting the compiler build operator state machines (async/await) eliminates a class of hand-written resumption code. (3) Forking your format implementation is viable when interop is confined to the boundary.

## 2.12 Cross-system observations

1. **[INFERENCE] Control flow:** push pipelines (DuckDB, Velox, ClickHouse processors) and pull async streams (DataFusion) are both proven at scale; Polars' hybrid suggests the invariant requirement is *resumable operator state + centralized scheduling*, however obtained.
2. **[FACT] Strings:** independent convergence on the 16-byte Umbra view (Umbra → DuckDB `string_t`, Velox StringView, Arrow Utf8View, Polars 2024) is the clearest single layout lesson in the field.
3. **[INFERENCE] Batch sizes:** 1–2K (X100, DuckDB) vs 8K (DataFusion, ClickHouse granule) vs ~100K morsels — engines increasingly separate the cache-residency unit (vector) from the scheduling unit (morsel).
4. **[INFERENCE] SIMD:** two defensible philosophies — structure code for auto-vectorization (DuckDB, arrow-rs) vs. targeted explicit SIMD behind an abstraction (Velox, ClickHouse hot spots). Measured end-to-end deltas are small (~4% TPC-H for all of Velox's explicit SIMD); per-kernel deltas can be 2–10×.
5. **[INFERENCE] Memory governance maturity ladder:** global limit + buffer manager + adaptive spill (DuckDB) ≥ hierarchical pools + arbitration (Velox) > reservation pools + per-op spill (DataFusion) > backpressure + sinks (Polars).


---

# 3. CPU architecture practicalities

Each topic: what it is, then the practical implications for execution-engine design. Microarchitecture named wherever a number appears.

## 3.1 Cache hierarchy

**[FACT]** Representative measured values (Chips and Cheese; 7-cpu.com):

| Level | AMD Zen 4 (5.7 GHz) | Intel Golden Cove (~5.2 GHz) | Apple M1 Firestorm (3.2 GHz) |
|---|---|---|---|
| L1D | 32 KB, 4 cyc (~0.7 ns) | 48 KB, 5 cyc | 128 KB, 3–4 cyc |
| L2 | 1 MB, 14 cyc (~2.4 ns) | 1.25 MB, 15 cyc | 12 MB shared per 4 P-cores, ~18 cyc |
| L3/SLC | 32 MB/CCD, ~46–50 cyc (~8–9 ns) | 30 MB, ~65–70 cyc (~13.7 ns measured w/ 4 KB pages) | 8 MB SLC, +10–15 ns |
| DRAM | ~73 ns (DDR5-6000) | higher on JEDEC DDR5-4800 | ~96 ns |

Zen 5 raises L1D to 48 KB at 4 cycles; Raptor Lake grew L2 to 2 MB at +1 cycle. **[FACT]** Cache lines are 64 B on x86; Apple Silicon uses 128 B lines at L2/SLC while L1D measurements report 64 B — so "cache-line-sized" constants must be per-platform parameters, and 128 B is the safe padding granularity on Apple (7-cpu; verification pass).

**[IMPL] Implications.** (a) Vector size in a vectorized engine should keep all live vectors of a pipeline in L1/L2 — the explicit X100 design rule. (b) L1 latency is essentially free under out-of-order execution; L3 (~50–70 cycles) is not: hash tables spilling from L2 to L3 lose several × on probe latency, and DRAM-resident tables lose ~8× more. (c) Intel's L3 is markedly slower than AMD's in recent generations — cache-resident tuning ported across vendors shifts break-even points (e.g. partition-then-probe vs direct probe).

## 3.2 Locality: sequential vs random; why scans are bandwidth-bound

**[FACT]** On M1: sequential streaming ~64 GB/s from DRAM; a dependent pointer chase pays full ~96 ns per line (~1.3 GB/s effective per chain) — a ~50× gap; MLP-saturated random access lands in between (7-cpu). **[FACT]** Ailamaki et al. 1999: ~50% of commercial-DBMS execution time was stall time, dominated by L2 data and L1 instruction misses. Manegold, Boncz & Kersten built full cost models around memory access rather than I/O (VLDB 1999, 2002); the radix-partitioned join exists to convert random access into cache/TLB-sized partitions.

**[IMPL]** A columnar scan with a cheap predicate does ~1–4 instructions per 8-byte value; at 4–6 IPC that demands far more bytes/cycle than DRAM supplies — scans saturate bandwidth, not ALUs. Consequences: compress columns (decode ALU work traded for bandwidth is usually a win — the Data Blocks premise); don't micro-optimize ALU code in DRAM-resident scan loops until a roofline check says you are compute-bound.

## 3.3 False sharing

**[FACT]** Two threads writing different variables in one cache line force MESI ownership bouncing; each transfer costs on the order of core-to-core latency — tens of ns within a die, ~100+ ns across CCDs/sockets (Chips and Cheese Infinity Fabric / Xeon 6 measurements). **[INFERENCE]** A falsely-shared per-thread counter degrades a ~1 ns L1 write into a ~40–150 ns coherence transaction — two orders of magnitude. **[FACT]** C++17 provides `std::hardware_destructive_interference_size` (P0154R1); GCC warns against using it in public ABI since the value follows `-mtune`; P0154 documents why 128 B can be correct even on Intel x86 — the L2 adjacent-line prefetcher pairs 64 B lines.

**[IMPL]** Pad per-thread accumulators, queue indices, and scheduler atomics to 64 B on x86, 128 B on Apple/ARM (or 128 B everywhere — the cost is trivial); prefer thread-local partials merged at pipeline end over shared atomic accumulators; detect regressions with `perf c2c` (§7.2).

## 3.4 Branch prediction and misprediction; predication

**[FACT]** Misprediction costs ~13 cycles on Zen 3 (AMD optimization guide) and M1 (7-cpu), somewhat more on long-pipeline Intel cores — call it 13–20 cycles, roughly 75–120 issue slots on a 6-wide machine [INFERENCE]. **[FACT]** For a data-dependent selection predicate, mispredict rate peaks near 50% selectivity; Ross built selection cost models around branch behavior and predication (PODS 2002 / TODS 2004). Zhou & Ross (SIGMOD 2002) observed *superlinear* SIMD speedups on selections because SIMD eliminates the per-tuple branch. X100's selection vectors institutionalize the branch-free approach: data dependency replaces control dependency, making cost flat in selectivity rather than tent-shaped.

**[IMPL]** Evaluate predicates branchlessly (`out_idx += (pred)` or SIMD compare + compress); keep a branchy variant only for extreme selectivities (predictor nearly always right — crossover empirically near <10% / >90% [INFERENCE from Ross's model]). Branchless code always does full work, so adaptive kernel choice (as Vectorwise's micro-adaptivity does) is worth the complexity for expensive predicates. Beware: predictors learn repeating patterns extremely well, so microbenchmarks with cyclic data wildly overstate real-world predictability.

## 3.5 TLB and huge pages

**[FACT]** Zen 4: 72-entry L1 DTLB, 3072-entry L2 TLB (+7–8 cycles per L2-TLB hit); with 4 KB pages that covers only 288 KB / 12 MB. Page walks push L3 latency from ~9.5 ns to >15 ns (Golden Cove) while still inside the cache (Chips and Cheese). M1: 16 KB base pages, ~3K-entry L2 TLB. **[FACT]** 2 MB huge pages multiply TLB reach 512×; measured effects include order-of-magnitude TLB-miss reductions and 1.14–1.25× end-to-end on large hash workloads (johnnysswlab.com).

**[IMPL]** Back large hash tables, sort buffers, and buffer pools with 2 MB (or 1 GB) pages — a random probe into a multi-GB table with 4 KB pages pays a TLB miss on essentially every probe, stacking a page walk on the data miss. Prefer explicit `MAP_HUGETLB`/madvise-based hugepage arenas; system-wide transparent huge pages set to `always` cause compaction stalls (ClickHouse operational guidance recommends `madvise`). Radix-partitioning fan-out is limited by TLB entries as well as cache associativity — the classic Manegold/Boncz result.

## 3.6 Hardware prefetchers and software prefetch

**[FACT]** L1/L2 prefetchers cover forward/backward streams and constant strides (Intel: DCU streamer + IP-stride at L1; streamer + adjacent-line at L2 — Intel Optimization Reference Manual). They cannot follow pointer chases or hash-distributed access. On Zen 4 the L3 is a victim cache that never prefetches. **[IMPL]** Columnar scans, even over several parallel column streams, are near-optimally hardware-prefetched — a core reason columnar layouts win. Hash probes and tree descents get zero help.

**[FACT]** Software prefetching for hash joins: Chen, Ailamaki, Gibbons & Mowry showed >80% of hash-join user time stalled on misses; **group prefetching** and **software-pipelined prefetching** yield 2.0–2.9× join-phase speedups (TODS 2007). AMAC (Kocberber et al., VLDB 2016) generalizes to per-lookup state machines for irregular chains, up to 2.3× over group prefetching. **[IMPL]** Software prefetch pays only when (a) the access stream is computable in advance — exactly what a vectorized batch of keys provides (a deep synergy between vectorization and prefetching), (b) targets are DRAM/L3-resident so there is ≥40–100 ns to hide, and (c) distance is tuned. Prefetching cache-resident data is pure overhead — a consistent finding in both papers.

## 3.7 NUMA

**[FACT]** Remote-vs-local: ~1.5–3× latency rule of thumb; measured worst cases <140 ns cross-node EPYC, >180 ns cross-die Xeon 6 (Chips and Cheese). Linux places pages on the first-touching thread's node (Drepper 2007). **[FACT]** The canonical engine answer is morsel-driven NUMA-aware scheduling (§1.7). **[IMPL]** Corollaries: initialize hash-table memory from the threads that will use it (first-touch); keep per-node build partitions for large joins; treat multi-CCD Zen parts as "NUMA-lite" even single-socket, since each CCD has a private L3 [INFERENCE].

## 3.8 Memory bandwidth: per-core ceilings, roofline

**[FACT]** Bandwidth = outstanding misses × 64 B / latency (Little's law); one core rarely saturates a socket. Skylake-class cores top out around 10–15 GB/s vs 60–100+ GB/s per socket (Lemire; Travis Downs "speed limits"). Exceptions exist: desktop Zen 4 single-core exceeds 57 GB/s of ~73 GB/s achievable (verified — Chips and Cheese; note their charts warn against direct cross-vendor comparison due to different memory setups); M1 Max single core streams ~100 GB/s, CPU cluster saturating ~224–243 GB/s (Anandtech). Server sockets: 8–12 DDR5 channels ≈ 300–600 GB/s, but per-core share on a 96-core EPYC is ~4–6 GB/s [INFERENCE from published STREAM results].

**[IMPL]** Roofline thinking: `SUM(int64)` is ~0.1–0.25 ops/byte — hopelessly bandwidth-bound; grouped aggregation is latency/MLP-bound; string processing, decompression, and expression-heavy projections are compute-bound. Consequences: provision scan parallelism to saturate bandwidth (a handful of cores per socket suffices; extra threads burn power and pollute L3 [INFERENCE]); lightweight compression converts bandwidth-bound scans into compute-assisted ones and is nearly always a win at DRAM residency [FACT — Data Blocks premise]; on desktop chips where one core nearly saturates DRAM, intra-query parallelism yields little on pure scans but still helps cache-resident operators [INFERENCE].

## 3.9 ILP, out-of-order execution, memory-level parallelism

**[FACT]** Reorder-buffer growth: Skylake 224 → Golden Cove 512; Zen 4 320 → Zen 5 448; M1 Firestorm ~630. Big ROBs exist to keep hundreds of instructions in flight across L3/DRAM misses. **[FACT]** A single-accumulator reduction is bound by add *latency* (FP add 2–3 cycles) while the core's *throughput* is 2–3 adds/cycle — a dependent chain leaves 6–12× on the table; 6–10 independent partial accumulators restore throughput (Agner Fog; Travis Downs). **[IMPL]** Every aggregation kernel should use multiple accumulators; for floats this changes rounding, so the engine must adopt an explicit reassociation policy rather than leaning on `-ffast-math`.

**[FACT]** The same principle applied to misses is MLP: a pointer-chasing probe sustains one outstanding miss; batching N independent probes sustains ~10–16+ (bounded by fill buffers/miss queues; Lemire measured Apple cores sustaining notably more than Skylake). **[IMPL]** This is the microarchitectural argument for batch-at-a-time hash operations even without SIMD: compute all hashes for a batch, issue all bucket loads, then resolve — misses overlap. Tuple-at-a-time interpreters serialize misses and run at a tenth the throughput [FACT — Chen et al./AMAC baselines].

---

# 4. SIMD

## 4.1 ISA survey

| ISA | Width | Registers | Gather | Scatter | Masking | Compress | Notes |
|---|---|---|---|---|---|---|---|
| NEON (ARMv8) | 128-bit fixed | 32 | no | no | via `BSL` selects | `TBL` shuffle emulation | baseline on all ARMv8; Apple has 4×128-bit pipes |
| AVX2 | 256-bit | 16 | yes (Haswell+) | no | 32/64-bit `vmaskmov` only | movemask + shuffle-table emulation | two semi-independent 128-bit lanes; lane-crossing permutes ~3 cyc |
| AVX-512 | 512-bit | 32 + 8 mask regs | yes | yes | full per-lane, zeroing/merging | `VPCOMPRESS*` native (B/W with VBMI2) | the ISA matters more than the width |
| SVE/SVE2 | 128–2048-bit VLA | 32 Z + 16 predicate | yes | yes | full predication | yes | first-fault loads enable early-exit loops; thin real-world availability |

**NEON.** **[FACT]** Fixed 128-bit, no gather/scatter, no native movemask (idioms `shrn`/`addv` substitute); universal on ARMv8. Apple Firestorm executes 4×128-bit NEON pipes — aggregate width equal to contemporary x86 desktops (dougallj's Firestorm tables). **[IMPL]** On Apple Silicon, throughput kernels need ≥4 independent 128-bit ops in flight (more unrolling than width suggests); no gather means hash probing and dictionary decode stay scalar-load-based (which costs less than it sounds — §4.2); compaction uses precomputed `TBL` shuffle masks (cf. Lemire's simdprune).

**AVX2.** **[FACT]** Adds gather but no scatter; per-element masked stores only at 32/64-bit granularity; no compress — filter compaction is movemask → shuffle-table lookup → `vpermd`. **[IMPL]** The safe x86 baseline; keep kernels within 128-bit lanes where possible; budget the shuffle port (typically 1/cycle on Intel) as the scarce resource in compaction-heavy code.

**AVX-512.** **[FACT]** Mask registers make nearly every op maskable; adds scatter, compress/expand, `VPCONFLICTD`, `VPOPCNTDQ`. Compress is the single most valuable DB primitive — selection-to-compaction in one instruction, very hard to emulate (Lemire). Gotcha: on Zen 4, `vpcompress` directly to memory is microcoded and slow — compress to register, then store (Lemire 2025). **Downclocking history:** **[FACT]** Skylake-X had license-based frequency levels with multi-hundred-MHz drops (Travis Downs' measurements); Ice Lake client reduced this to a ~100 MHz single-core dip; Zen 4 double-pumps 256-bit datapaths with no downclocking — and AVX-512 code is still faster and more energy-efficient than AVX2 equivalents *because of the ISA, not the width* (Mysticial's Zen 4 teardown; Phoronix). Zen 5 goes native 512-bit with benign frequency behavior. **[IMPL]** "Avoid AVX-512 in mixed workloads" is obsolete advice on Ice Lake+/Zen 4+, but binaries shipping to Skylake-X-era fleets still need dispatch that avoids 512-bit-heavy kernels there.

**SVE/SVE2.** **[FACT]** Vector-length-agnostic: one binary runs at any hardware VL (128–2048-bit powers of two); full predication; gather/scatter; first-fault register permits speculative vector loads, so early-exit loops (strcmp-like) can be vectorized — inexpressible on x86. Availability is thin and *narrowing*: A64FX 512-bit, Graviton 3 256-bit, but Graviton 4 (Neoverse V2) and NVIDIA Grace regressed to 128-bit SVE2; Apple ships no SVE through M4. Even at equal width, SVE2 predication/gather beats NEON on some kernels (2× on hashing — Vardanian) and underperforms on others (zingaburga's analysis). **[IMPL]** ARM targets cannot treat SVE as "the" path: NEON remains mandatory, SVE an optional specialization; never bake VL into data layouts; do not assume VL grows over time.

## 4.2 Gather/scatter reality

**[FACT]** Gathers rarely beat well-scheduled scalar loads: Haswell's implementation was micro-op emulation (~12+ cycles even L1-resident); Skylake reached roughly element-per-cycle-class throughput; Zen 4's gathers are comparatively weak and scatters weaker (Agner Fog; uops.info). Scalar pipelines meanwhile do 2–3 loads/cycle. **[INFERENCE]** Gather's value is keeping data in vector registers (avoiding insert/extract) and code density — not load speed. **[FACT]** Polychroniou, Raghavan & Ross (SIGMOD 2015): gather-based vertical vectorization of hash probing wins clearly when tables are cache-resident; out-of-cache, performance converges to memory-bound equality. **[IMPL]** Benchmark gathers per microarchitecture before committing; for DRAM-resident probes prefer MLP techniques (§3.9) over gather micro-optimization.

## 4.3 Masked execution in filter/select

**[FACT]** AVX-512/SVE masks let kernels evaluate predicates into mask registers, suppress inactive-lane side effects (no faults, no visible writes), and feed compress/expand — making filter→project→compact fully branch-free (Zhou & Ross's superlinear speedups came precisely from mispredict elimination). **[IMPL]** A real design fork: represent intermediate selection as **bitmaps** (1 bit/row; AND-able across predicates; popcount for cardinality; SIMD-friendly) vs **selection vectors** (index lists; better at low selectivity; cheaper downstream random access). AVX-512 turns bitmap→compacted-vector into one `vpcompressd`; NEON/AVX2 emulate via shuffle tables. ISA capabilities visibly shape the optimal internal representation [INFERENCE — reflected in DuckDB's selection vectors vs ClickHouse's byte-mask filters].

## 4.4 Auto-vectorization: what compilers do and don't

**[FACT]** Reliably vectorized: straight-line maps over contiguous arrays, reductions (integer freely; FP only with reassociation enabled), simple if-conversion to blends, limited strided/interleaved patterns (LLVM vectorizer docs). Not reliably vectorized: compaction/left-packing (output index depends on data), compile-time-chosen cross-lane shuffles, hash probing, early-exit loops (LLVM still reports "cannot vectorize early exit loops" in common cases as of clang 21–23; GCC 16 handles more via versioning). **[FACT]** Pragmas: `#pragma omp simd` (`-fopenmp-simd`), `#pragma clang loop vectorize(enable|assume_safety)`, GCC `ivdep`. Verification: clang `-Rpass=loop-vectorize` / `-Rpass-missed=...` / `-Rpass-analysis=...`, GCC `-fopt-info-vec-*`, godbolt, llvm-mca.

**[IMPL]** The engine practice that works: tiny, alias-free, fixed-trip-count kernels over `restrict`-qualified column pointers — the vectorized-engine batch model produces exactly this shape, which is why auto-vectorization works far better inside X100-style engines than in tuple-at-a-time code [INFERENCE]. Put `-Rpass-missed` checks in CI for hot kernel files: silent devectorization after refactoring is a classic regression.

## 4.5 Explicit intrinsics vs abstraction layers

**[FACT]** The options: **Google Highway** (length-agnostic ops, multi-target compilation + runtime dispatch across SSE4→AVX-512/NEON/SVE/RVV; adopted by NumPy per NEP 54); **xsimd** (header-only, arch as template parameter; used by Velox; no SVE); **EVE** (expressive C++20; fixed-size SVE only; no dispatch); **std::simd** (`std::experimental::simd` in libstdc++; P1928 merged into C++26; criticized by practitioners for fixed-width model, absent dispatch story, and missing mask/compress expressiveness — see Highway's own comparison document).

**When abstraction helps.** **[IMPL]** Portability across the x86/NEON matrix; correct-by-construction runtime dispatch (every distributed-binary engine needs it — cf. ClickHouse's machinery); tail/mask handling.

**When it hurts.** **[IMPL]** Precisely the operations DB kernels live on — movemask, compress/expand, conflict detection, cross-lane shuffles — have semantically different costs and encodings per ISA (AVX-512 k-masks vs NEON byte-lane masks vs SVE predicates), so lowest-common-denominator APIs either emulate expensively or omit them. **[INFERENCE]** The convergent industry pattern: hand-written per-ISA kernels for the ~10 hottest primitives (filter-compact, hash-mix, dictionary decode, bit-unpack), portable or auto-vectorized code everywhere else — visible in ClickHouse (intrinsics + dispatch), Velox (xsimd + specializations), DuckDB (portable C++ shaped for auto-vec).

**Evidence on magnitude.** **[FACT]** Removing all explicit SIMD from Velox cost ~4% on TPC-H SF1 overall, with the largest per-query benefit ~25% (Benson, Ebeling & Rabl, ADMS 2023 — corrected attribution from verification pass); the same study found compiler-generic intrinsics and xsimd within ~1.3%. Kersten et al. 2018's end-to-end SIMD finding (~10%) is consistent. **[INFERENCE]** Explicit SIMD earns its keep in a small set of kernels; the architectural choice that matters more is making data layouts SIMD-legible (contiguous, padded, NULL-separated).

## 4.6 SIMD in the DB literature — canon

**[FACT]** Zhou & Ross (SIGMOD 2002): first systematic SIMD database operators; superlinear speedups via branch elimination. Polychroniou, Raghavan & Ross (SIGMOD 2015): vertical vectorization of scans, hash tables, Bloom filters, partitioning using gather/scatter/compress; establishes that full vectorization changes optimal algorithm choice, not just constants. Lang et al. (SIGMOD 2016, Data Blocks): SIMD predicate evaluation over compressed data with positional SMAs. Lemire et al.: Roaring bitmaps, simdprune, AVX-512 filtering engineering and its microarchitectural pitfalls.

## 4.7 Cross-cutting synthesis

**[INFERENCE]** Four durable laws: (1) batch-at-a-time execution is as much about MLP and prefetchability as about SIMD — batches turn dependent misses into overlapped ones; (2) branch-free/masked kernels dominate branchy ones except at extreme selectivities; (3) the memory hierarchy (line size, TLB reach, per-core bandwidth ceiling, NUMA distance) sets nearly all constants in operator cost models — Manegold/Boncz-style calibration remains the right methodology on 2026 hardware; (4) ISA breadth (masks, compress, gather) now matters more than raw vector width — Zen 4 proved 512-bit semantics at 256-bit width still wins; Graviton 4 proved width can regress.


---

# 5. Memory allocators

## 5.1 Taxonomy and tradeoffs

| Strategy | Alloc cost | Free cost | Fragmentation | Fits | Fails |
|---|---|---|---|---|---|
| Monotonic/bump | pointer increment | no-op (bulk reset) | none internally; holds everything until reset | per-batch/per-operator scratch | long-lived phases, churn |
| Arena (chunked bump) | pointer increment + rare chunk alloc | bulk free at lifetime end | low | aggregation states, string payloads, per-query scratch | partial reclamation |
| Pool/slab (size classes) | free-list pop | free-list push | low for uniform sizes | hash-table nodes, fixed-width states | varied sizes |
| pmr (polymorphic) | virtual call + underlying resource | same | inherits resource | configuration boundaries, testability | per-tuple hot loops (unless devirtualized) |
| Fixed-capacity containers | none (embedded storage) | none | none | bounded metadata | data payloads |
| Modern malloc (jemalloc/tcmalloc/mimalloc) | fast-path TLS cache | same | managed | everything else; the global backstop | lifetime-structured bulk patterns |

**Monotonic/bump.** **[FACT]** `std::pmr::monotonic_buffer_resource` releases memory only on destruction and is intentionally not thread-safe; ~10× allocation-cost improvements over the default heap are demonstrated in standard references (cppreference; modernescpp benchmarks).

**Arena.** **[FACT]** Lakos's CppCon 2017 talks and WG21 papers (N4468, P0089R1) present measured evidence that lifetime-matched local allocators can produce order-of-magnitude runtime differences via locality and contention, not just allocation cost. **[FACT]** The adversarial datapoint: Berger et al. (OOPSLA 2002, "Reconsidering Custom Memory Allocation") found most custom allocators fail to beat a good general-purpose malloc — *except regions/arenas*, which win on speed but risk blowup because nothing frees early. The debate continues: "There Ain't No Such Thing as a Free Custom Memory Allocator" (arXiv 2206.11728) and van Kempen & Berger's ISMM 2026 revisit (arXiv 2605.17119, verified). **[INFERENCE]** Query engines are the workload where arenas win: operator and query lifetimes give natural bulk-free points. This is why every surveyed engine has one (ClickHouse `Arena`, Velox `HashStringAllocator`, HyPer NUMA-local storage areas).

**Pool/slab.** **[FACT]** Fixed-size classes with per-class free lists; O(1); low fragmentation for uniform objects (jemalloc's slab design). **[IMPL]** ClickHouse points users needing partial frees to `ArenaWithFreeLists`; a ClickHouse issue discusses fixed-block allocation for large hash tables. **[INFERENCE]** Pools fit hash-table nodes and fixed-width aggregation states; arenas fit append-only varlen payloads.

**pmr.** **[FACT]** `std::pmr::memory_resource` is an abstract base with virtual `do_allocate`/`do_deallocate`; concrete resources include monotonic and (un)synchronized pool resources; the synchronized variant measurably lags due to locking. The virtual-call debate: Lakos's measurements say the indirection is small relative to locality wins; **[INFERENCE]** the residual objection for hot loops is that the indirect call inhibits inlining of the bump fast path unless devirtualized — which is why engines use concrete arena types in inner loops and reserve pmr polymorphism for configuration boundaries. **[SPECULATION]** LTO/PGO devirtualization probably closes most of the gap, but no surveyed engine relies on it.

**Fixed-capacity containers.** **[FACT]** C++26 adopts `std::inplace_vector` (P0843) — embedded-storage, compile-time capacity, `try_push_back`. **[INFERENCE]** Suits small bounded metadata (per-operator child lists, scratch selection buffers), not data payloads.

## 5.2 Global allocators and why engines swap them

**[FACT]** Durner, Leis & Neumann (DaMoN 2019) measured allocators under TPC-DS inside a high-performance DBMS: the allocator decisively affects throughput and many-core NUMA scalability; jemalloc/tbbmalloc gave large speedups over glibc malloc; they adopted jemalloc as their default. **[FACT]** Mechanisms: jemalloc — per-CPU-count arenas + thread caches + size classes; tcmalloc — lock-free per-CPU caches on restartable sequences plus the hugepage-aware Temeraire backend (OSDI 2021 fleet results); mimalloc — free-list sharding per 64 KiB page with thread-local and thread-delayed lists (MSR TR 2019). **[IMPL]** In the wild: ClickHouse jemalloc (PR #2773: up to 20% faster, ~10% lower RSS — verified); DuckDB bundles jemalloc on Linux; Polars ships jemalloc (Linux/macOS)/mimalloc (Windows) and documents up to ~25% runtime impact; Arrow C++ prefers mimalloc for cross-platform consistency.

**Huge pages nuance.** **[FACT]** Hugepage-aware allocation helps (Temeraire); system-wide transparent huge pages set to `always` hurts allocator-heavy workloads via kernel compaction stalls — ClickHouse recommends `madvise`. **[INFERENCE]** The right engine pattern is explicit hugepage-backed arenas, not THP=always.

**The buffer-manager-as-allocator pattern.** **[IMPL]** DuckDB routes memory through a buffer manager of fixed 256 KB blocks with global tracking, eviction, and spill — an allocator-of-blocks unifying cache and working memory. Umbra's variable-size-page buffer manager plays the same role with mmap-reserved size-class regions. **[INFERENCE]** In engines that own storage, the buffer manager *is* the top-level allocator; a standalone execution engine must decide early whether operator memory is tracked centrally (enabling limits/spill) or ad hoc.

---

# 6. Parallel execution

## 6.1 Thread pools

**[FACT]** TBB/oneTBB: fixed worker pool with arena-based concurrency limits; composing two pools oversubscribes the machine (oneTBB docs/issues). Taskflow: fixed workers + work stealing (TPDS 2022). **[INFERENCE]** For CPU-bound analytical work the consensus is one fixed pool sized to hardware concurrency; oversubscription classically arrives via nested parallel libraries. **[IMPL]** DataFusion's counterpoint: Tokio (a general async runtime) serves as the CPU pool, with documented discipline required to keep I/O on a separate runtime.

## 6.2 Work stealing

**[FACT]** Blumofe & Leiserson (JACM 1999): randomized work stealing executes a computation with work T₁ and span T∞ in expected T₁/P + O(T∞) with provable space bounds. Cilk-5's THE protocol: owners push/pop deque tails nearly synchronization-free; thieves steal heads. Non-blocking deques trace to Arora–Blumofe–Plaxton. **[INFERENCE]** Morsel dispatchers, rayon, Tokio, and Taskflow are all instantiations; the theory is settled, the engineering variable is granularity (§6.3).

## 6.3 Chunk vs morsel scheduling

**[FACT — verified against the paper]** Leis et al. 2014: morsel size ~100,000 tuples "yields good tradeoff" between elasticity, load balancing, and overhead — the smallest value where dispatch overhead is negligible; NUMA-local dispatch with locality-preferring stealing; elastic DOP at morsel granularity; the shared dispatch structure can itself bottleneck (mitigations: per-thread local ranges, larger morsels). **[IMPL]** DuckDB pulls ~100K-row morsels processed internally as 2048-row vectors; Polars' new engine cites the paper directly. **[INFERENCE]** Static chunking (OpenMP `schedule(static)` style) is competitive only for perfectly uniform work; filters, skew, and cache effects make dynamic self-scheduling the analytical-engine default. **[FACT]** The documented counterexample: DataFusion added a morsel-driven scheduler in 8.0.0 and later abandoned it, defending exchange-based parallelism as equal in practice on single nodes (issues #2199/#2504) — morsel-driven is not a free win when it must integrate with an async I/O runtime.

## 6.4 Synchronization costs

**[FACT]** ETH SPCL's atomics study: CAS/FAA/SWP instruction latencies are mostly identical; the dominant cost is cache-line state and contention. Queue costs grow superlinearly under contention; failed CAS retries burn cycles. **[INFERENCE]** Design arithmetic: one atomic fetch-add per 100K-tuple morsel is noise; one atomic per tuple is a design bug. Everything between is a measurement question.

## 6.5 Reductions and parallel aggregation

**[FACT]** The standard shape is two-phase: thread-local pre-aggregation into private tables, then radix-partitioned merge with each thread owning partitions — DuckDB's parallel GROUP BY documents this explicitly (2022 blog; ICDE 2024 out-of-core follow-up); HyPer's morsel paper describes the same with cache-resident pre-aggregation and overflow partitions (following IBM BLU). **[FACT]** The design space remains contested: "Global Hash Tables Strike Back!" (arXiv 2505.04153, 2025) argues shared global concurrent tables win in relevant regimes and notes the local-preaggregation design's adoption by DuckDB and DataFusion. **[INFERENCE]** For scalar reductions (SUM/COUNT) the pattern degenerates to per-thread partials merged once; tree merges matter only at very high core counts. FP reductions force an explicit associativity policy (§3.9).

## 6.6 False sharing in practice

See §3.3 for mechanics. **[FACT]** Detection is a solved problem: `perf c2c` reports HITM (loads hitting modified lines in remote caches) with a per-cacheline Pareto (Joe Mario's guide; Red Hat docs). **[IMPL]** Standard fixes: alignas(64/128) padding of per-thread slots; structure-of-arrays for shared counters; thread-local accumulation.

## 6.7 Affinity and thread count

**[FACT]** Pinning improves cache/NUMA locality (a documented example cut load misses from ~7.8% to ~0.6%) but over-constraining the scheduler hurts when pinned cores are busy with other load; hyperthread siblings share execution resources, so pairing two compute-bound engine threads on one physical core is harmful (Arm learning path; Bendersky). **[FACT]** Memory-bound kernels saturate DRAM with a fraction of cores (~4–6 on documented systems); extra threads add nothing or regress (HPC-Wiki bandwidth saturation; STREAM/NUMA studies). **[INFERENCE]** "How many threads" should be answered by measuring against per-socket bandwidth ceilings, not `hardware_concurrency()`; morsel-driven elasticity is partly a hedge against this being workload-dependent.

---

# 7. Benchmark methodology

## 7.1 Google Benchmark

**[FACT — all from the official user guide and docs]**
- `benchmark::DoNotOptimize(x)` forces a value to be materialized (and on GNU compilers acts as a read/write barrier); `ClobberMemory()` flushes pending writes — the standard defenses against dead-code elimination and constant folding of benchmark inputs.
- Repetitions (`--benchmark_repetitions=N`) are process-level independent samples with reported mean/median/stddev/CV; distinct from iterations (inner-loop sizing to reach min time). Warm-up via `MinWarmUpTime`/`--benchmark_min_warmup_time`.
- `--benchmark_enable_random_interleaving=true` randomly interleaves repetitions across benchmarks; documented as lowering run-to-run variance ~40% on average (verified; note the figure is asserted in the docs/issue #1051, not a published study).
- `--benchmark_perf_counters=cycles,instructions,...` samples PMU counters per benchmark via libpfm.
- `PauseTiming()`/`ResumeTiming()` are documented as expensive per-iteration — harnesses that pause around per-iteration setup end up measuring the harness.
- The "Reducing Variance" doc prescribes: performance governor, disable turbo boost, `taskset` pinning, priority, disable SMT, and warns that shrinking working sets to L1 "may lead you to optimize for an unrealistic situation"; provides `MaybeReenterWithoutASLR()`.

## 7.2 perf, PMU counters, flamegraphs

**[FACT]** `perf stat` for cycles, instructions, IPC, cache-misses, LLC-load-misses, branch-misses, dTLB-load-misses, stalled-cycles (`-d` for the detailed set); `perf record` for sampling; `perf c2c` (Linux 4.10+) for false sharing (Gregg's perf reference). CPU flamegraphs expose hot paths; **off-CPU flamegraphs** (eBPF `offcputime`) capture blocked time. **[INFERENCE]** For parallel engines, off-CPU graphs are what catches "threads idle at the pipeline barrier" — invisible in on-CPU profiles. **[IMPL]** Interpretation discipline: IPC < ~1 on a modern big core suggests memory-bound; check LLC misses and dTLB misses before touching ALU code; branch-miss rate on selection kernels is directly actionable (§3.4).

## 7.3 Environment control for reproducibility

**[FACT]** LLVM's Benchmarking Tips checklist: disable frequency scaling and turbo (`intel_pstate/no_turbo`), disable ASLR (`randomize_va_space=0`), `cset shield` to reserve cores, disable the SMT sibling of benchmark cores, kill background services, static linking, tmpfs; achievable variance <0.1% — with the explicit warning that "low noise is required, but not sufficient. It does not exclude measurement bias" (citing Mytkowicz et al.).

**The ASLR debate.** **[FACT]** Disabling ASLR reduces variance but fixes one arbitrary memory layout, which is itself a bias axis: Stabilizer (ASPLOS 2013) showed code/stack/heap placement shifts results enough to flip conclusions, and randomizes layout so effects become Gaussian; Mytkowicz et al. (ASPLOS 2009, "Producing Wrong Data Without Doing Anything Obviously Wrong!") showed environment-variable size and *link order* alone change SPEC conclusions. **[INFERENCE]** Best practice combines both schools: disable ASLR for low-variance A/B comparisons *and* randomize/interleave (random interleaving, link-order shuffling) to sample layout space. Emery Berger's "Performance Matters" talk and Coz (causal profiling) are the accessible entry points.

**Apple Silicon caveats.** **[FACT]** No public PMU API on macOS; counters go through private `kperf` frameworks (what Instruments uses; root required; 2 fixed + 8 configurable counters); community tooling exists but automated PMU pipelines are Linux-first. **[INFERENCE]** Laptop benchmarking adds thermal throttling and asymmetric P/E cores; a CPU-first engine project should treat a Linux desktop/server with pinned frequencies as the reference platform and macOS numbers as secondary.

## 7.4 Statistical rigor

**[FACT]** Two legitimate camps. *Min-is-best*: Chen & Revels (2016) argue the minimum is the robust location estimator because timing noise is almost entirely additive and positive (assumes memoryless sparse noise). *Distribution-aware*: Kalibera & Jones (ISMM 2013) give methodology for choosing repetition counts across levels and computing speedups with confidence intervals; Raasveldt et al. (DBTest 2018) report median + nonparametric 95% CIs; Hoefler & Belli (SC 2015) condemn plain means under variance. **[INFERENCE]** A defensible engine-project policy: report median and min with CIs from ≥10 process-level repetitions, plus CV; investigate any CV above a few percent (a hard "CV<5%" rule is folklore, not standard). Never report a single run; never average across different data distributions.

## 7.5 Benchmark pitfalls (with evidence)

**[FACT — each demonstrated in Raasveldt et al., DBTest 2018, "Fair Benchmarking Considered Difficult," verified from the PDF]**
- **Non-reproducibility:** with hidden configuration they construct a rock-paper-scissors cycle (MariaDB < PostgreSQL < SQLite < MariaDB*) where the starred variant merely used DOUBLE instead of DECIMAL.
- **Unoptimized baselines:** MonetDB debug build 1.58 s vs optimized 0.87 s on TPC-H Q1; PostgreSQL Q9 0.47 s vs 0.27 s from config flags alone.
- **Apples vs oranges:** a hand-written standalone kernel ("TimDB") runs Q1 in 0.03 s vs MonetDB's 0.87 s — comparing a kernel without overflow checking, transactions, or generality to a full DBMS is "clearly unfair and misleading." (Directly relevant to any standalone engine comparing itself to DuckDB.)
- **Cold/warm/hot confusion:** OS page cache makes fake-cold runs warm; true cold requires dropping OS caches; report cold and hot separately.
- **Overly specific tuning** to known benchmark distributions; **ignoring preprocessing** (index build, dictionary encoding at load); **incorrect code** that skips work and looks fast — verify results against a reference system.

**Additional pitfalls, sourced:** dead-code elimination and constant folding of inputs (Google Benchmark docs); frequency scaling mid-run (reducing-variance doc); NUMA placement variance (STREAM/NUMA studies); denormal floats costing hundreds of cycles via microcode assists — control FTZ/DAZ when benchmarking float kernels (arXiv 1506.03997); first-touch page faults — freshly mmapped/calloc'd buffers make the first iteration measure the kernel's page-fault path, not the algorithm [INFERENCE — special case of warm-run discipline]; key-distribution effects — aggregation performance is a function of group count and skew, so all-unique or all-identical keys benchmark different code paths (DuckDB aggregation blog; arXiv 2505.04153). **[FACT]** The generalization: Heiser's "Systems Benchmarking Crimes" and van der Kouwe et al. (arXiv 1801.02381) — 96% of surveyed systems papers contained preventable benchmarking flaws.

**Benchmark-suite landscape.** **[FACT]** ClickBench: 43 queries, single denormalized 100M-row table; hot = min of runs 2–3; geometric mean of per-query ratios (+10 ms shift); explicitly no joins, no concurrency — its own README says all benchmarks are liars. TPC-H/TPC-DS: normalized, heavily over-targeted, uniform generators unrealistic. JCC-H adds join-crossing correlations and skew to TPC-H specifically to defeat over-tuning. Star Schema Benchmark: denormalized TPC-H variant. JOB (Join Order Benchmark): real IMDB data built to expose cardinality-estimation failure. db-benchmark (H2O.ai, now DuckDB Labs): groupby/join at 0.5–50 GB, moved to bare metal after cloud-VM noise. **[INFERENCE]** For a standalone execution engine, the honest benchmark set is operator-level microbenchmarks (scan, filter, hash aggregation, join) across controlled cardinalities/selectivities/skew, plus a TPC-H-derived subset clearly labeled non-audited, with methodology (repetitions, statistics, environment) published alongside.


---

# 8. Common mistakes in student engines, hobby HPC, and performance-oriented repositories

Each entry: the mistake, why it is one, and the evidence.

1. **Per-tuple virtual dispatch while claiming a fast engine.** [FACT] The canonical measurement: X100 found MySQL spending ~90% of TPC-H Q1 cycles outside actual computation; per-call misprediction penalties (~13–20 cycles) stack per tuple per operator. Vectorization exists to amortize dispatch over ~1000 values. [INFERENCE] `shared_ptr<Value>` per tuple compounds this with atomic reference-count traffic that contends across threads.

2. **Row-at-a-time iteration over "columnar" storage.** [INFERENCE] Storing columns but iterating row-wise forfeits SIMD legibility, cache-line utilization, and prefetcher friendliness — the layout is columnar, the execution is Volcano. The fix is batch-oriented operators, not storage changes.

3. **`std::string` per value.** [INFERENCE, backed by engine practice] Per-value heap allocation and pointer chasing; every serious engine arena-allocates string payloads (ClickHouse Arena "for example, short strings") and/or uses 16-byte view structs (§2.12), and dictionary-encodes on ingest where possible.

4. **`std::unordered_map` in aggregation/join hot paths.** [FACT] The standard's bucket-and-chain contract forces pointer chasing per lookup; open-addressing flat tables are consistently faster in published benchmarks (boost::unordered_flat_map internals; Tessil benchmarks). Real engines build custom tables: radix-partitioned (DuckDB), templated 30+ variants (ClickHouse), perfectly sized post-build (HyPer).

5. **Benchmarking debug builds or sanitizer builds.** [FACT] Quantified by DBTest 2018: MonetDB 1.58 s (debug) vs 0.87 s (release) on the same query. Sanitizer builds also exercise different code paths (ClickHouse's Arena carries explicit ASan poisoning hooks).

6. **Measuring the first run — or never measuring cold.** [FACT] First runs include page faults, allocation warm-up, and cache population; fake-cold runs hit the OS page cache. Report cold and hot separately, deliberately (DBTest 2018).

7. **No baseline, or a strawman baseline.** [FACT] "Failure to optimize the baseline" is DBTest 2018's headline pitfall; the inverse (comparing a bare kernel to a full DBMS — their "TimDB" demonstration) is equally misleading. [FACT] The pandas variant: beating single-threaded, eagerly materializing pandas with a multithreaded engine is expected, not evidence of a fast engine (Polars' own methodology notes; pandas-side optimization closes much of naive gaps).

8. **Micro-optimizing before profiling.** [FACT] Causal profiling results (Coz: +9% memcached, +25% SQLite from single-line changes found by the profiler, not intuition) quantify how non-obvious real bottlenecks are.

9. **Ignoring the bandwidth ceiling.** [FACT] Scan-bound operators saturate DRAM with a few cores (§6.7). [INFERENCE] Chasing SIMD wins or complaining about "poor scaling" in a bandwidth-bound kernel misreads the roofline; check bytes/s against STREAM first.

10. **False sharing in parallel accumulators.** [FACT] Adjacent per-thread counters on one line; detect with `perf c2c`, fix with padding (§3.3, §6.6).

11. **Premature SIMD on AoS/strided layouts.** [FACT] AoS forces gathers with far worse cost than contiguous loads; SoA conversion alone yields 2–5× on CPUs in published cases (Intel layout-transformation guidance). [INFERENCE] Fix layout before writing intrinsics.

12. **Unrealistic data distributions.** [FACT] All-unique or all-identical keys exercise degenerate hash paths; aggregation cost varies strongly with group count and skew (DuckDB; arXiv 2505.04153); DBTest flags hard-coded benchmark cardinalities as an "incorrect code" pitfall. TPC-H's uniformity is itself the reason JCC-H exists.

13. **Ignoring NUMA on multi-socket.** [FACT] Remote-access asymmetry motivates the entire morsel-driven design; allocator NUMA behavior alone swings benchmarks (Durner et al. 2019).

14. **The harness measures allocation, not compute.** [FACT/INFERENCE] Per-iteration setup inside the timed region, or `PauseTiming()` misuse (documented as expensive). Note the flip side: Durner et al. show allocation can legitimately dominate query time — decide explicitly whether allocator cost is inside the system under test.

15. **Over-templating into code bloat.** [SPECULATION→INFERENCE] Monomorphizing every operator × type × encoding combination inflates instruction-cache footprint; no rigorous public study specific to hobby engines exists, but code-placement sensitivity is well established (Stabilizer), and production engines deliberately balance interpretation vs specialization — DuckDB's UnifiedVectorFormat and HyPer's interpreted Data Block scans both exist precisely to cap specialization explosion.

16. **Claiming vectorized execution while spilling intermediates to RAM.** [INFERENCE] Choosing a batch size of, say, 1M rows reintroduces the MonetDB materialization tax — the X100 vector-size sweep is the canonical evidence that too-large vectors converge to column-at-a-time behavior.

---

# 9. Recurring design patterns

Identified across the surveyed systems; identification, not recommendation.

1. **Cache-sized columnar batches as the unit of interpretation amortization.** X100 ~1K vectors [FACT]; DuckDB 2048 [IMPL]; ClickHouse chunks over ports [FACT]; Data Blocks vectorized scans [FACT].
2. **Separation of scheduling grain from execution grain.** Morsels (~100K rows) contain many vectors (1–8K rows): HyPer/Umbra, DuckDB, Polars-stream [FACT/IMPL].
3. **Selection vectors / validity masks instead of eager compaction.** X100 selection vectors accepted by all primitives [FACT]; ClickHouse byte-mask `filter` [IMPL]; Velox dictionary-wrap of filter results [IMPL]; Arrow validity bitmaps [FACT].
4. **Encoding-preserving execution.** Dictionary/constant/RLE representations flow through operators: DuckDB compressed vector formats, Velox encodings + peeling + lazy vectors, ClickHouse LowCardinality, Data Blocks predicate-on-compressed [FACT/IMPL].
5. **Pipeline decomposition with explicit breakers.** Neumann's produce/consume pipelines [FACT]; DuckDB source→ops→sink [IMPL]; Umbra step state machines [FACT]; ClickHouse processor DAGs [IMPL].
6. **Task-based parallelism with shared-state build phases and work stealing.** Morsel scheduling; two-phase join build (thread-local materialization, then perfectly sized global structure); ClickHouse's coarser stealable lanes [FACT].
7. **Two-phase (thread-local → merge) aggregation.** HyPer (after IBM BLU), DuckDB radix-partitioned merge, Umbra explicit steps, ClickHouse partial aggregation states as first-class (persistable) objects [FACT/IMPL].
8. **Contention-avoiding hash joins, two competing subpatterns.** Radix/shared-partitioned tables (MonetDB, ClickHouse) vs single global lock-free CAS-built table with tagged pointers (HyPer) [FACT]. Both exist to kill latch contention.
9. **16-byte view strings with inline prefix ("German strings").** Umbra origin; adopted by DuckDB, Velox, Arrow Utf8View, Polars [FACT].
10. **Arena/region allocation keyed to operator or query lifetime; bulk free.** ClickHouse Arena, Velox HashStringAllocator, HyPer NUMA-local areas, Umbra size-class pool [FACT/IMPL].
11. **Runtime CPU-feature dispatch with a conservative baseline.** ClickHouse cpuid multi-versioning (SSE4.2 floor) [FACT]; Highway's model [IMPL]; compiled engines get the equivalent by JIT-ing for the host [INFERENCE].
12. **Adaptivity at multiple granularities.** Per-primitive flavor bandits (Vectorwise micro-adaptivity), per-batch specialization (Photon NULL-free/ASCII paths, Velox ASCII fast paths and adaptive conjunct reordering), per-morsel backend switching (HyPer/Umbra adaptive compilation), runtime pipeline rewriting for spill (ClickHouse) [FACT].
13. **Scan-time pruning via lightweight statistics.** MinMax/zone maps (Vectorwise, ClickHouse skipping indices, Data Blocks SMAs/PSMAs, Snowflake-style pruning) [FACT].
14. **Late materialization, in modern form.** Ordered predicate evaluation touching survivors only; lazy column decode (Velox lazy vectors); in-scan SARGable filtering [FACT/IMPL].
15. **Per-operator layout conversion.** Rows inside hash tables (Vectorwise, most engines); row-format sort keys (DuckDB after Kuiper & Mühleisen 2023) — strict columnarity inside operators is not sacred [FACT].
16. **Vectorization and compilation as complements.** ClickHouse vectorized + expression JIT; Data Blocks vectorized scans feeding compiled pipelines; Vectorwise selective predicate JIT; Umbra compiled pipelines with tiered backends [FACT]. The pure dichotomy is settled in favor of hybrids; the open choice is which side is the default [INFERENCE].
17. **Global memory governance with graceful degradation.** Global limits + spill (DuckDB), hierarchical pools + arbitration (Velox), per-query tracking + overcommit (ClickHouse) [IMPL].

---

# 10. Curated reading list

Ordered chronologically. Format: citation — summary — why it mattered — what remains relevant.

**Foundations**

1. **Graefe, G. "Encapsulation of Parallelism in the Volcano Query Processing System." SIGMOD 1990.** Introduces the exchange operator: parallelism as a plug-in operator leaving all other operators sequential. Made parallelizing any iterator engine mechanical. Ancestor of every shuffle/repartition operator; still DataFusion's single-node model.
2. **Graefe, G. "Query Evaluation Techniques for Large Databases." ACM Computing Surveys 25(2), 1993.** The 100-page map of classical execution algorithms (sort, hash, join, parallelism). The baseline against which later execution-model papers define themselves.
3. **Graefe, G. "Volcano — An Extensible and Parallel Query Evaluation System." IEEE TKDE 6(1), 1994.** The open/next/close iterator model. Defined the operator abstraction for 15+ years; the interface (not the granularity) survives everywhere.
4. **Ailamaki, A., DeWitt, D., Hill, M., Wood, D. "DBMSs on a Modern Processor: Where Does Time Go?" VLDB 1999.** Measured commercial DBMSs spending ~half their cycles stalled (L2 data + L1i misses). Opened database microarchitecture analysis; its methodology (decompose CPI, then redesign) is now universal.
5. **Boncz, P., Manegold, S., Kersten, M. "Database Architecture Optimized for the New Bottleneck: Memory Access." VLDB 1999.** Diagnoses the memory wall; cache-conscious radix-partitioned joins; hardware-conscious design rules. The founding document of the field this survey covers.
6. **Boncz, P., Kersten, M. "MIL Primitives for Querying a Fragmented World." VLDB Journal 8(2), 1999.** MonetDB's column algebra: operator-at-a-time bulk processing. Proved per-tuple interpretation eliminable; its materialization weakness motivated X100.
7. **Zhou, J., Ross, K. "Implementing Database Operations Using SIMD Instructions." SIGMOD 2002.** First systematic SIMD database operators; superlinear speedups from branch elimination. Still the cleanest demonstration that SIMD's value in selection is mispredict removal.
8. **Manegold, S., Boncz, P., Kersten, M. "Generic Database Cost Models for Hierarchical Memory Systems." VLDB 2002.** Cost models parameterized by measured cache/TLB characteristics. The calibration methodology remains correct on 2026 hardware.
9. **Stonebraker, M. et al. "C-Store: A Column-Oriented DBMS." VLDB 2005.** Read-optimized columnar storage: compression, projections. The US lineage that became Vertica; established columnar commercially.
10. **Boncz, P., Zukowski, M., Nes, N. "MonetDB/X100: Hyper-Pipelining Query Execution." CIDR 2005.** *The* vectorized-execution paper: cache-resident ~1K vectors, typed primitives, selection vectors; 1–2 orders of magnitude on TPC-H. Every modern vectorized engine is a descendant. If one reads a single paper before designing an engine, this is it.
11. **Abadi, D., Madden, S., Hachem, N. "Column-Stores vs. Row-Stores: How Different Are They Really?" SIGMOD 2008.** Layout tricks on a row store don't recover column-store performance; the execution architecture is decisive. The justification for treating execution as its own field.
12. **Zukowski, M. "Balancing Vectorized Query Execution with Bandwidth-Optimized Storage." PhD thesis, U. Amsterdam, 2009.** The complete X100/Vectorwise treatment, including PFOR compression. Deepest single source on vectorized engine design.

**The compilation era and the synthesis**

13. **Neumann, T. "Efficiently Compiling Efficient Query Plans for Modern Hardware." PVLDB 4(9), 2011.** Data-centric compilation: produce/consume, pipeline breakers, tuples in registers, LLVM. The counter-paradigm; inspired Spark's whole-stage codegen and a decade of JIT DBMS work.
14. **Sompolski, J., Zukowski, M., Boncz, P. "Vectorization vs. Compilation in Query Execution." DaMoN 2011.** First head-to-head microarchitectural comparison; argued for combining both. Set the terms of the decade's debate — and its eventual resolution.
15. **Zukowski, M., Boncz, P. "Vectorwise: Beyond Column Stores." IEEE Data Eng. Bulletin 35(1), 2012.** The productionization story: PAX storage, compressed buffer pool, MinMax indices, NSM-in-execution where useful. Reality-checks academic purity.
16. **Idreos, S. et al. "MonetDB: Two Decades of Research in Column-oriented Database Architectures." IEEE Data Eng. Bulletin 35(1), 2012.** Retrospective of the CWI lineage including cracking and recycling. Best single entry point to MonetDB.
17. **Abadi, D., Boncz, P., Harizopoulos, S., Idreos, S., Madden, S. "The Design and Implementation of Modern Column-Oriented Database Systems." Foundations and Trends in Databases 5(3), 2013.** The canonical survey unifying MonetDB/VectorWise/C-Store. Ideal scaffolding for any literature review, including this one.
18. **Răducanu, B., Boncz, P., Zukowski, M. "Micro Adaptivity in Vectorwise." SIGMOD 2013.** ε-greedy multi-armed bandit choosing among compiled primitive "flavors" per call. The cheapest general mechanism for data/hardware-dependent kernel choice; conceptual ancestor of Photon/Velox batch adaptivity.
19. **Leis, V., Boncz, P., Kemper, A., Neumann, T. "Morsel-Driven Parallelism." SIGMOD 2014.** NUMA-aware elastic scheduling of ~100K-tuple morsels over pinned workers; >30× on 32 cores. The de facto standard intra-node parallelism model.
20. **Polychroniou, O., Raghavan, A., Ross, K. "Rethinking SIMD Vectorization for In-Memory Databases." SIGMOD 2015.** Systematic vertically-vectorized operators (scans, hash tables, Bloom filters, partitioning) with gather/scatter/compress. Shows full vectorization changes optimal algorithm choice; the SIMD-algorithms companion to X100.
21. **Lang, H. et al. "Data Blocks: Hybrid OLTP and OLAP on Compressed Storage using both Vectorization and Compilation." SIGMOD 2016.** Byte-addressable compression, SMAs/PSMAs, SIMD predicates on compressed data, interpreted vectorized scans feeding compiled pipelines. The reference design for hybrid vectorization+compilation and for scan-time pruning.
22. **Kocberber, O., Falsafi, B., Grot, B. "Asynchronous Memory Access Chaining." PVLDB 9(4), 2016.** Per-lookup state machines to overlap irregular memory accesses. With Chen et al. (TODS 2007), the source on software prefetching for hash operations.
23. **Menon, P., Mowry, T., Pavlo, A. "Relaxed Operator Fusion for In-Memory Databases." PVLDB 11(1), 2017.** Staged compilation with cache-resident buffers reconciles codegen with SIMD and prefetching; up to 2.2×. The "fusion is a dial" result.
24. **Kohn, A., Leis, V., Neumann, T. "Adaptive Execution of Compiled Queries." ICDE 2018.** Bytecode interpretation of LLVM IR + background compilation + morsel-boundary switching. Killed compilation latency as a blanket objection.
25. **Kersten, T., Leis, V., Kemper, A., Neumann, T., Pavlo, A., Boncz, P. "Everything You Always Wanted to Know About Compiled and Vectorized Queries But Were Afraid to Ask." PVLDB 11(13), 2018.** Typer vs Tectorwise under controlled conditions: parity in raw speed, divergence in engineering properties. The debate's settlement treaty; still the paper to cite when choosing an execution model.
26. **Shaikhha, A., Dashti, M., Koch, C. "Push versus Pull-Based Loop Fusion in Query Engines." JFP 28, 2018.** PL-theoretic account of push/pull as dual fusion strategies. Supplies the vocabulary (direction × granularity) for classifying all engines.
27. **Durner, D., Leis, V., Neumann, T. "On the Impact of Memory Allocation on High-Performance Query Processing." DaMoN 2019.** Allocator choice decisively affects DBMS throughput and NUMA scalability. The citation for treating the allocator as a first-order design decision.

**The modern systems**

28. **Raasveldt, M., Mühleisen, H. "DuckDB: An Embeddable Analytical Database." SIGMOD 2019 (demo); + "Data Management for Data Science" CIDR 2020.** In-process vectorized OLAP. Proof the X100 model scaled *down*; the most widely deployed descendant.
29. **Neumann, T., Freitag, M. "Umbra: A Disk-Based System with In-Memory Performance." CIDR 2020; + Kersten, Leis, Neumann. "Tidy Tuples and Flying Start." VLDB Journal 30, 2021.** Variable-size-page buffer manager; German strings; tiered codegen with a custom IR and direct x86 backend. The state of the art of the compiled lineage; defines the latency bar interpreters must meet.
30. **Behm, A. et al. "Photon: A Fast Query Engine for Lakehouse Systems." SIGMOD 2022.** Industrial-scale argument for vectorized-interpreted over codegen: batch-level adaptivity for uncurated data, engineering observability. The most-cited practitioner justification for choosing vectorization.
31. **Pedreira, P. et al. "Velox: Meta's Unified Execution Engine." PVLDB 15(12), 2022.** Vectorized execution as a reusable library; encoding-aware execution; hierarchical memory management. Signals the componentization era.
32. **Kuiper, L., Mühleisen, H. "These Rows Are Made for Sorting and That's Just What We'll Do." ICDE 2023.** Row-format keys/payloads beat columnar comparators for sorting, inside a vectorized engine. Representative of the per-operator-layout refinement wave.
33. **Lamb, A. et al. "Apache Arrow DataFusion: A Fast, Embeddable, Modular Analytic Query Engine." SIGMOD Companion 2024.** Modular Rust engine on Arrow; defends exchange parallelism and async-runtime scheduling. The strongest documented counterpoint to morsel orthodoxy.
34. **Schulze, R. et al. "ClickHouse — Lightning Fast Analytics for Everyone." PVLDB 17(12), 2024.** Production retrospective: processors/ports, multi-version kernels with cpuid dispatch, opportunistic JIT, elastic per-query threading, ClickBench methodology. The best single description of an industrial vectorized server engine.
35. **Raasveldt, M., Holanda, P., Gubner, T., Mühleisen, H. "Fair Benchmarking Considered Difficult." DBTest 2018.** Mock-experiment catalog of benchmarking sins. Mandatory reading before publishing any performance number.

**Reference texts and courses (not papers).** Drepper, "What Every Programmer Should Know About Memory" (2007) — still the standard memory-hierarchy text; Agner Fog's optimization manuals and instruction tables (continuously updated); Intel 64/IA-32 Optimization Reference Manual; CMU 15-721 Advanced Database Systems lecture notes (execution, vectorization, compilation, scheduling) — the standard curriculum stitching these papers together.


---

# 11. Synthesis

## 11.1 The most consistent design philosophies of successful engines

1. **Design from the memory hierarchy outward.** [INFERENCE from the whole record] Every successful engine since 1999 begins with "where do the bytes live and how do they move," not with operator interfaces. Batch sizes, hash-table layouts, string representations, and partitioning fan-outs are all derived from cache, TLB, and bandwidth parameters — and the field's founding methodology (Manegold/Boncz calibration) is to *measure* those parameters, not assume them.

2. **Amortize interpretation; never pay a decision per tuple.** Whether by vectors (X100 lineage), compilation (HyPer lineage), or both, the invariant is that per-tuple control flow — virtual calls, branches, format checks — is hoisted to per-batch, per-morsel, or per-query granularity.

3. **Keep intermediates where bandwidth is free.** The CPU cache "is the only place where bandwidth does not matter" (X100). Too-small batches pay interpretation; too-large batches pay DRAM. Everything from vector size to relaxed operator fusion's staging buffers is this one principle.

4. **Make parallelism a runtime policy, not a plan property.** Morsel-driven scheduling won so thoroughly that even its documented dissenter (DataFusion) frames its exchange design as achieving "similar scalability." Elastic DOP, NUMA-local dispatch, and stealing are the shared toolkit; two-phase build/merge patterns keep shared state out of hot paths.

5. **Adapt at runtime; distrust static models.** Micro-adaptive flavor selection, per-batch specialization (NULL-free/ASCII paths), adaptive compilation tiers, runtime pipeline rewriting for spill, selectivity-ordered conjuncts — successful engines assume data properties are unknowable at plan time and build cheap runtime feedback loops.

6. **Bulk lifetime management.** Arena/region allocation with bulk free, buffer managers as top-level allocators, global memory limits with graceful spill. Per-object lifetime management is treated as a defect.

7. **Measure honestly or drown.** The engines that publish methodology (repetitions, statistics, environment, cold/hot separation, per-release regression tracking) are the ones whose performance claims survived scrutiny. Continuous benchmarking (ClickHouse VersionsBench, DuckDB benchmark runner, Arrow Conbench) is treated as CI, not marketing.

8. **Engineering economics beats microarchitectural purity.** The vectorized-interpreted model dominates new engines not because it is fastest — Kersten et al. showed parity — but because it is debuggable, profileable, portable, and adaptively specializable by ordinary systems programmers. Photon is the clearest primary-source statement of this philosophy.

## 11.2 Where the literature disagrees

1. **Vectorization vs. compilation as the default tier.** Settled as "both are fine, hybrids win" (Kersten 2018; ROF), but the *default* remains contested: Umbra/CedarDB argue tiered codegen is now cheap enough for everything; Photon/DuckDB/Velox argue interpretation + selective specialization is the better engineering point. No neutral study exists of the *engineering* costs (the axis practitioners actually decide on).

2. **Morsel-driven push vs. exchange-based pull on single nodes.** Leis et al. demonstrated morsel superiority in HyPer; DataFusion documents empirical parity with Tokio + repartitioning and abandoned its morsel prototype. Confound: the comparison has never been run with all else equal — engines differ in language, format, and kernels. [INFERENCE] The honest reading: morsel-driven is clearly superior for NUMA multi-socket; on single-socket commodity machines the difference is within engineering noise.

3. **Shared global vs. partitioned parallel aggregation.** Two-phase thread-local pre-aggregation is the industry default (BLU, HyPer, DuckDB); "Global Hash Tables Strike Back!" (2025) argues global concurrent tables win in relevant regimes. Actively contested.

4. **Arrow as internal format.** DataFusion executes directly on Arrow and holds ClickBench Parquet records; DuckDB/Velox/Umbra maintain private formats for selection vectors, out-of-order writes, and string freedoms; Polars forked the implementation. The format's own evolution (Utf8View, ListView, REE) is the spec conceding ground to engine needs. No consensus.

5. **Explicit SIMD vs. auto-vectorization.** Measured end-to-end deltas are small (~4% TPC-H for all of Velox's explicit SIMD; ~10% in Kersten et al.), but per-kernel deltas reach 10×, and AVX-512 compress has no clean auto-vec path. DuckDB and arrow-rs (which deleted its intrinsics) sit on one side; ClickHouse and Velox on the other. The disagreement is really about where the cutoff for "the few kernels that matter" lies.

6. **Statistics for benchmark reporting.** Min-is-best (Chen & Revels) vs. distribution-aware medians with CIs (Kalibera & Jones; DBTest). Both are defensible; mixing them silently is not.

7. **How much OS to trust.** MonetDB's mmap delegation vs. Umbra's explicit buffer management vs. DuckDB's middle path; THP vs. explicit huge pages; pinning vs. letting the scheduler work. The literature agrees only that the defaults are wrong for databases.

## 11.3 Open engineering questions

1. **What is the right execution grain on 2026 hardware?** X100's 1K constant was calibrated on 2005 caches. With 1–2 MB private L2s, 32+ MB L3s, and 128 B lines on some parts, the optimal vector size, and whether it should be adaptive per query/operator, has no current published answer.
2. **How should an engine exploit heterogeneous cores (P/E, big.LITTLE)?** Morsel scheduling assumed homogeneous workers; no surveyed system publishes a considered policy for asymmetric CPUs, now the desktop/laptop default.
3. **Bitmap vs. selection-vector selection representation as a function of ISA.** AVX-512 compress favors bitmaps; NEON favors index vectors; engines pick one globally. Whether per-platform (or per-selectivity) representation switching pays is unresolved in public literature.
4. **Can SVE's vector-length-agnostic model be exploited by a batch engine at all,** given VL regressed from 256 to 128 bits across Graviton generations and layouts must not bake in VL?
5. **What is the principled morsel size?** "~100K tuples" is an empirical 2014 constant entangled with cache sizes, dispatch overhead, and elasticity requirements; no model predicts it from hardware parameters.
6. **Where exactly does software prefetching pay in 2026?** Group prefetch/AMAC results predate current OoO depths (512+ ROB entries hide much more latency); the break-even table sizes need re-measurement per microarchitecture.
7. **How to co-schedule bandwidth-bound and compute-bound pipelines?** Roofline reasoning says scans need few cores and hash builds need many; no surveyed scheduler models per-pipeline bandwidth demand explicitly.
8. **What does honest benchmarking look like on Apple Silicon,** where PMU access is private-API, frequency is opaque, and cores are asymmetric — yet a large fraction of development happens there?
9. **Compressed execution beyond dictionaries:** how far can operators run on RLE/FOR/bit-packed data before decompression, and does the combinatorial kernel explosion stay manageable without codegen? (DuckDB and Velox each solve fragments; no general treatment.)

## 11.4 Research gaps that could justify independent architectural decisions

For a standalone, CPU-first, benchmark-centered execution engine, the following gaps are places where the literature does not dictate an answer, and careful measurement would constitute a genuine contribution:

1. **A calibrated, reproducible re-derivation of the execution-grain constants (vector size, morsel size, aggregation-table partition counts) on current x86 and Apple Silicon**, in the Manegold/Boncz cost-model tradition, published with the measurement harness. The field runs on 2005–2014 constants.
2. **A controlled single-variable comparison of scheduling models** (morsel-push vs exchange-pull vs async-state-machine hybrid) inside *one* engine with shared kernels — the comparison Leis 2014 and DataFusion's docs each claim but neither isolates.
3. **Selection-representation study:** bitmaps vs selection vectors vs hybrid, parameterized by selectivity, ISA (NEON/AVX2/AVX-512), and downstream operator — with the adaptive-switching policy micro-adaptivity suggests but never applied to representations.
4. **Allocator interaction with execution structure:** Durner et al. measured global allocators under one engine; nobody has published arena-vs-pool-vs-pmr comparisons *per operator class* with NUMA and hugepage dimensions controlled.
5. **P/E-core-aware morsel scheduling** — an unclaimed, well-scoped scheduling contribution testable on any current MacBook or Intel desktop.
6. **An honest "SIMD ledger":** per-primitive accounting of explicit-SIMD benefit against auto-vectorized baselines across ISAs, extending the ADMS 2023 Velox study to a from-scratch engine where layout is controlled — quantifying the "~10 kernels that matter" folklore.
7. **Benchmark methodology as a first-class artifact:** engines publish results; few publish falsifiable harnesses with environment manifests, statistics policy, and bias controls (interleaving, layout randomization). A reference implementation of DBTest 2018 + Berger-school practice for execution engines would be citable independent of any performance result.

---

# 12. Bibliography

Primary papers (chronological):

- Graefe, G. "Encapsulation of Parallelism in the Volcano Query Processing System." SIGMOD 1990. https://dl.acm.org/doi/10.1145/93597.98720
- Graefe, G. "Query Evaluation Techniques for Large Databases." ACM Computing Surveys 25(2), 1993. https://dl.acm.org/doi/10.1145/152610.152611
- Graefe, G. "Volcano — An Extensible and Parallel Query Evaluation System." IEEE TKDE 6(1), 1994. https://dl.acm.org/doi/10.1109/69.273032
- Ailamaki, A. et al. "DBMSs on a Modern Processor: Where Does Time Go?" VLDB 1999. https://www.vldb.org/conf/1999/P28.pdf
- Boncz, P., Manegold, S., Kersten, M. "Database Architecture Optimized for the New Bottleneck: Memory Access." VLDB 1999. https://dblp.org/rec/conf/vldb/BonczMK99.html
- Boncz, P., Kersten, M. "MIL Primitives for Querying a Fragmented World." VLDB Journal 8(2), 1999. https://link.springer.com/article/10.1007/s007780050076
- Ross, K. "Conjunctive Selection Conditions in Main Memory." PODS 2002 / ACM TODS 29(1), 2004. https://dl.acm.org/doi/10.1145/543613.543628
- Zhou, J., Ross, K. "Implementing Database Operations Using SIMD Instructions." SIGMOD 2002. http://www.cs.columbia.edu/~kar/pubsk/simd.pdf
- Manegold, S., Boncz, P., Kersten, M. "Generic Database Cost Models for Hierarchical Memory Systems." VLDB 2002. https://vldb.org/conf/2002/S06P03.pdf
- Berger, E. et al. "Reconsidering Custom Memory Allocation." OOPSLA 2002. https://people.cs.umass.edu/~emery/pubs/berger-oopsla2002.pdf
- Stonebraker, M. et al. "C-Store: A Column-Oriented DBMS." VLDB 2005. https://web.stanford.edu/class/cs345d-01/rl/cstore.pdf
- Boncz, P., Zukowski, M., Nes, N. "MonetDB/X100: Hyper-Pipelining Query Execution." CIDR 2005. https://www.cidrdb.org/cidr2005/papers/P19.pdf
- Chen, S., Ailamaki, A., Gibbons, P., Mowry, T. "Improving Hash Join Performance through Prefetching." ACM TODS 2007. http://www.cs.cmu.edu/~natassa/aapubs/conference/hashjoin.pdf
- Drepper, U. "What Every Programmer Should Know About Memory." 2007. https://people.freebsd.org/~lstewart/articles/cpumemory.pdf
- Abadi, D., Madden, S., Hachem, N. "Column-Stores vs. Row-Stores." SIGMOD 2008. http://www.cs.umd.edu/~abadi/papers/abadi-sigmod08.pdf
- Boncz, P., Kersten, M., Manegold, S. "Breaking the Memory Wall in MonetDB." CACM 51(12), 2008. https://cacm.acm.org/research/breaking-the-memory-wall-in-monetdb/
- Mytkowicz, T. et al. "Producing Wrong Data Without Doing Anything Obviously Wrong!" ASPLOS 2009. https://users.cs.northwestern.edu/~robby/courses/322-2013-spring/mytkowicz-wrong-data.pdf
- Zukowski, M. "Balancing Vectorized Query Execution with Bandwidth-Optimized Storage." PhD thesis, U. Amsterdam, 2009. https://ir.cwi.nl/pub/14075
- Neumann, T. "Efficiently Compiling Efficient Query Plans for Modern Hardware." PVLDB 4(9), 2011. https://www.vldb.org/pvldb/vol4/p539-neumann.pdf
- Kemper, A., Neumann, T. "HyPer: A Hybrid OLTP&OLAP Main Memory Database System." ICDE 2011. https://dl.acm.org/doi/10.1109/ICDE.2011.5767867
- Sompolski, J., Zukowski, M., Boncz, P. "Vectorization vs. Compilation in Query Execution." DaMoN 2011. https://dl.acm.org/doi/10.1145/1995441.1995446
- Zukowski, M., Boncz, P. "Vectorwise: Beyond Column Stores." IEEE Data Eng. Bulletin 35(1), 2012. http://sites.computer.org/debull/A12mar/vectorwise.pdf
- Idreos, S. et al. "MonetDB: Two Decades of Research in Column-oriented Database Architectures." IEEE Data Eng. Bulletin 35(1), 2012. http://sites.computer.org/debull/a12mar/monetdb.pdf
- Abadi, D. et al. "The Design and Implementation of Modern Column-Oriented Database Systems." FnT in Databases 5(3), 2013. https://www.cs.umd.edu/~abadi/papers/abadi-column-stores.pdf
- Răducanu, B., Boncz, P., Zukowski, M. "Micro Adaptivity in Vectorwise." SIGMOD 2013. https://dl.acm.org/doi/10.1145/2463676.2465292
- Curtsinger, C., Berger, E. "Stabilizer: Statistically Sound Performance Evaluation." ASPLOS 2013. https://people.cs.umass.edu/~emery/pubs/stabilizer-asplos13.pdf
- Kalibera, T., Jones, R. "Rigorous Benchmarking in Reasonable Time." ISMM 2013. https://dl.acm.org/doi/10.1145/2464157.2464160
- Leis, V., Boncz, P., Kemper, A., Neumann, T. "Morsel-Driven Parallelism." SIGMOD 2014. https://db.in.tum.de/~leis/papers/morsels.pdf
- Polychroniou, O., Raghavan, A., Ross, K. "Rethinking SIMD Vectorization for In-Memory Databases." SIGMOD 2015. https://15721.courses.cs.cmu.edu/spring2016/papers/p1493-polychroniou.pdf
- Hoefler, T., Belli, R. "Scientific Benchmarking of Parallel Computing Systems." SC 2015. https://dl.acm.org/doi/10.1145/2807591.2807644
- Lang, H. et al. "Data Blocks." SIGMOD 2016. https://db.in.tum.de/downloads/publications/datablocks.pdf
- Kocberber, O., Falsafi, B., Grot, B. "Asynchronous Memory Access Chaining." PVLDB 9(4), 2016. http://www.vldb.org/pvldb/vol9/p252-kocberber.pdf
- Chen, J., Revels, J. "Robust Benchmarking in Noisy Environments." 2016. https://arxiv.org/pdf/1608.04295
- Boncz, P., Anadiotis, A., Kläbe, S. "JCC-H: Adding Join Crossing Correlations with Skew to TPC-H." TPCTC 2017. https://ir.cwi.nl/pub/27429
- Menon, P., Mowry, T., Pavlo, A. "Relaxed Operator Fusion for In-Memory Databases." PVLDB 11(1), 2017. https://db.cs.cmu.edu/papers/2017/p1-menon.pdf
- Kohn, A., Leis, V., Neumann, T. "Adaptive Execution of Compiled Queries." ICDE 2018. https://db.in.tum.de/~leis/papers/adaptiveexecution.pdf
- Kersten, T. et al. "Everything You Always Wanted to Know About Compiled and Vectorized Queries But Were Afraid to Ask." PVLDB 11(13), 2018. https://www.vldb.org/pvldb/vol11/p2209-kersten.pdf
- Shaikhha, A., Dashti, M., Koch, C. "Push versus Pull-Based Loop Fusion in Query Engines." JFP 28, 2018. https://arxiv.org/abs/1610.09166
- Raasveldt, M., Holanda, P., Gubner, T., Mühleisen, H. "Fair Benchmarking Considered Difficult." DBTest 2018. https://mytherin.github.io/papers/2018-dbtest.pdf
- van der Kouwe, E. et al. "Benchmarking Crimes: An Emerging Threat in Systems Security." 2018. http://arxiv.org/abs/1801.02381
- Durner, D., Leis, V., Neumann, T. "On the Impact of Memory Allocation..." DaMoN 2019. https://db.in.tum.de/~durner/papers/memory-allocation-impact-damon2019.pdf
- Raasveldt, M., Mühleisen, H. "DuckDB: An Embeddable Analytical Database." SIGMOD 2019 demo. https://duckdb.org/pdf/SIGMOD2019-demo-duckdb.pdf
- Neumann, T., Freitag, M. "Umbra: A Disk-Based System with In-Memory Performance." CIDR 2020. https://db.in.tum.de/~freitag/papers/p29-neumann-cidr20.pdf
- Kersten, T., Leis, V., Neumann, T. "Tidy Tuples and Flying Start." VLDB Journal 30, 2021. https://link.springer.com/article/10.1007/s00778-020-00643-4
- Hunter, A. et al. "Beyond malloc efficiency to fleet efficiency (Temeraire)." OSDI 2021. https://www.usenix.org/system/files/osdi21-hunter.pdf
- Behm, A. et al. "Photon: A Fast Query Engine for Lakehouse Systems." SIGMOD 2022. https://people.eecs.berkeley.edu/~matei/papers/2022/sigmod_photon.pdf
- Pedreira, P. et al. "Velox: Meta's Unified Execution Engine." PVLDB 15(12), 2022. https://www.vldb.org/pvldb/vol15/p3372-pedreira.pdf
- Benson, L., Ebeling, R., Rabl, T. "Evaluating SIMD Compiler-Intrinsics for Database Systems." ADMS 2023. https://ceur-ws.org/Vol-3462/ADMS5.pdf
- Kuiper, L., Mühleisen, H. "These Rows Are Made for Sorting..." ICDE 2023. https://duckdb.org/pdf/ICDE2023-kuiper-muehleisen-sorting.pdf
- Lamb, A. et al. "Apache Arrow DataFusion." SIGMOD Companion 2024. https://dl.acm.org/doi/10.1145/3626246.3653368
- Schulze, R. et al. "ClickHouse — Lightning Fast Analytics for Everyone." PVLDB 17(12), 2024. https://www.vldb.org/pvldb/vol17/p3731-schulze.pdf
- "Data Formats in Analytical DBMSs: Performance Trade-offs and Future Directions." 2024. https://arxiv.org/pdf/2411.14331
- "Global Hash Tables Strike Back!" 2025. https://arxiv.org/html/2505.04153v2
- van Kempen, N., Berger, E. "Reconsidering 'Reconsidering Custom Memory Allocation'." ISMM 2026. https://arxiv.org/abs/2605.17119

Specifications, documentation, engineering sources:

- Apache Arrow Columnar Format. https://arrow.apache.org/docs/format/Columnar.html
- DuckDB internals: vectors, memory management, aggregation, push-based execution. https://duckdb.org/docs/stable/internals/vector · https://duckdb.org/2024/07/09/memory-management.html · https://duckdb.org/2022/03/07/aggregate-hashtable · https://github.com/duckdb/duckdb/issues/1583
- Velox developer docs: vectors, SIMD, memory, arena, task. https://facebookincubator.github.io/velox/develop/vectors.html (and siblings)
- DataFusion docs and configs. https://docs.rs/datafusion/latest/datafusion/ · https://datafusion.apache.org/user-guide/configs.html
- ClickHouse architecture docs; IProcessor.h; PODArray.h; Arena.h; jemalloc PR #2773; CPU dispatch write-up. https://clickhouse.com/docs/en/development/architecture · https://github.com/ClickHouse/ClickHouse (sources) · https://maksimkita.com/blog/cpu-dispatch-in-clickhouse.html
- ClickBench. https://github.com/ClickHouse/ClickBench
- Polars: birds-eye view, string rewrite, streaming engine, benchmarks. https://pola.rs/posts/polars_birds_eye_view · https://pola.rs/posts/polars-string-type · https://github.com/pola-rs/polars/issues/20947 · https://pola.rs/posts/benchmarks/
- CedarDB, "German Strings." https://cedardb.com/blog/german_strings/
- Google Benchmark user guide, reducing variance, random interleaving, perf counters. https://github.com/google/benchmark/blob/main/docs/user_guide.md (and siblings)
- LLVM Benchmarking Tips. https://llvm.org/docs/Benchmarking.html
- LLVM Auto-Vectorization docs. https://llvm.org/docs/Vectorizers.html
- Brendan Gregg: perf, flamegraphs, off-CPU analysis. https://www.brendangregg.com/perf.html · https://www.brendangregg.com/flamegraphs.html
- Joe Mario: perf c2c. https://joemario.github.io/blog/2016/09/01/c2c-blog/
- Agner Fog: optimization manuals and instruction tables. https://www.agner.org/optimize/
- uops.info instruction measurements. https://uops.info/
- Chips and Cheese microarchitecture analyses (Zen 4/5, Golden Cove, Infinity Fabric, Xeon 6, Neoverse V2). https://chipsandcheese.com/
- 7-cpu measured latencies (Apple M1 et al.). https://www.7-cpu.com/cpu/Apple_M1.html
- Travis Downs: performance speed limits; AVX-512 frequency. https://travisdowns.github.io/
- Daniel Lemire: bandwidth, MLP, AVX-512 compress, simdprune. https://lemire.me/blog/ · https://github.com/lemire/simdprune
- dougallj: Apple Firestorm instruction tables. https://dougallj.github.io/applecpu/firestorm.html
- ARM: SVE/SVE2 documentation. https://developer.arm.com/documentation/102340/latest/
- Google Highway; xsimd; std::simd comparison. https://github.com/google/highway · https://github.com/xtensor-stack/xsimd · https://github.com/google/highway/blob/master/g3doc/std_simd_comparison.md
- WG21: P0154R1 (interference sizes), N4468/P0089R1 (Lakos, allocators), P0843 (inplace_vector), P1928 (std::simd). https://www.open-std.org/jtc1/sc22/wg21/
- cppreference: pmr resources. https://en.cppreference.com/w/cpp/memory/memory_resource
- jemalloc (Meta engineering), tcmalloc design, mimalloc TR. https://engineering.fb.com/2011/01/03/core-infra/scalable-memory-allocation-using-jemalloc/ · https://google.github.io/tcmalloc/design.html · https://www.microsoft.com/en-us/research/wp-content/uploads/2019/06/mimalloc-tr-v1.pdf
- Blumofe, R., Leiserson, C. "Scheduling Multithreaded Computations by Work Stealing." JACM 46(5), 1999. https://dl.acm.org/doi/10.1145/324133.324234
- ETH SPCL atomics study. https://spcl.inf.ethz.ch/Research/Parallel_Programming/Atomics/
- Emery Berger: Stabilizer, Coz, "Performance Matters." https://emeryberger.com/research/stabilizer/ · https://github.com/plasma-umass/coz
- Heiser, G. "Systems Benchmarking Crimes." https://www.cse.unsw.edu.au/~gernot/benchmarking-crimes.html
- CMU 15-721 Advanced Database Systems notes. https://15721.courses.cs.cmu.edu/
- Databricks: whole-stage code generation. https://www.databricks.com/blog/2016/05/23/apache-spark-as-a-compiler-joining-a-billion-rows-per-second-on-a-laptop.html
- Meta: Velox and Arrow alignment. https://engineering.fb.com/2024/02/20/developer-tools/velox-apache-arrow-15-composable-data-management/
- Neumann, T., Leis, V. "Database Query Compilation: Our Journey." HYTRADBOI 2025. https://www.hytradboi.com/2025/slides/leis-neumann-compilation.pdf

---

*End of survey. Prepared as a literature review and engineering survey; all architecture decisions are intentionally left to the designer.*
