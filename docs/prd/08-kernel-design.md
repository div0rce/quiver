# 08 — Kernel Design

## 1. Purpose

Complete specifications for kernel families K1–K10 (MOD-K1…MOD-K10): normative semantics, per-ISA algorithm selection, family invariants, and family test/benchmark obligations. Signatures and caller contracts live in [04 §5](04-public-api.md); memory rules in [06](06-memory-model.md); ISA mechanics in [09](09-simd-architecture.md).

## 2. Common kernel module contract

Every kernel family module (MOD-K1…K10) inherits this contract; family sections state only deltas and specifics. This inheritance is itself the specification (fields not repeated below are defined here, once, normatively).

- **Non-responsibilities (all families):** no allocation, no I/O, no logging, no dispatch logic (registration only), no cross-family internals (REQ-REPO-009), no data-dependent backend switching (REQ-KERNEL-007 is resolution-time only), no threading.
- **Dependencies:** MOD-CORE, MOD-KCOMMON, MOD-DISPATCH (registration). **Dependents:** dispatch tables, tests, benches, examples.
- **Public interfaces:** the family's [04 §5](04-public-api.md) APIs; internal structure: the five-file pattern of [02 §3](02-repository-architecture.md) — `_scalar_impl.h` (the specification, Charter T3), `_scalar.cpp`, `_avx2.cpp`, `_neon.cpp`, `_avx512.cpp`.
- **Lifecycle / state model:** stateless pure functions; no init/shutdown; immutable `constinit` LUT data only (via MOD-KCOMMON).
- **Invariants (all families):** output determinism per REQ-API-006 (over defined output regions); bounds per REQ-MEM-001; output completeness and write regions per REQ-MEM-008 — compaction kernels (K1 selvec, K2, K3→selvec) may scatter scratch only within their `n`-element capacity regions, never beyond; bitmap tails zeroed per ADR-016; produced selection vectors strictly increasing within the defined output (ADR-025).
- **Failure modes (all families):** contract violations per [16](16-error-handling.md) (debug assert / release UB); no other failure modes exist — kernels are total on their contract domain.
- **Performance contract (template):** single pass, O(n) (K5: O(output)); no allocation; core loops branch-free where selection-dependent (REQ-KERNEL-003); roofline class per family table §4 governs benchmark interpretation (Survey §3.2, §3.8, §3.9).
- **Threading contract:** thread-compatible pure functions (common API contract, [04 §2](04-public-api.md)).
- **Memory contract:** [06](06-memory-model.md) in full; aliasing per ADR-023 matrix.
- **Test matrix (template, instantiated per family in [12 §2, §5](12-testing-architecture.md)):** unit (hand-computed cases + `reference.h` second oracle), property, differential (every backend vs `_scalar_impl.h`, byte-exact per §3 float rules), fuzz (differential libFuzzer target), invariant, regression.
- **Benchmark matrix (template, instantiated per family in [10 §7](10-benchmark-architecture.md)):** variants {scalar, autovec-*, per-ISA} × ledger axes (Charter §6.4).
- **Documentation:** one `docs/api/<family>.md` page per the [14 §5](14-documentation.md) template, including the ledger excerpt with the explicit-vs-autovec verdict (win **or loss**, Charter T7).
- **Acceptance (template):** all matrix tests green on tier-1 platforms + SDE; sanitizer-clean; benchmarks run and validate; doc page complete; REQs below verified.

### Common requirements

| ID | Requirement |
|---|---|
| REQ-KERNEL-001 | Each family ships backends per the milestone plan; `_scalar_impl.h` defines semantics and shall contain no intrinsics or target regions (REQ-REPO-004). |
| REQ-KERNEL-002 | Integer-kernel backends shall produce bit-identical outputs to the scalar reference; float rules per §3/ADR-013. Verified by the differential suites. |
| REQ-KERNEL-003 | K1, K2, K3 core loops shall be branch-free with respect to data values (selection-independent cost; Survey §3.4). Benchmark evidence: selectivity sweep published per family; no gate threshold (evidence, not target — master prompt Part 8). |
| REQ-KERNEL-004 | Every kernel is a single forward pass over its inputs; no kernel may require a second pass (SMA, popcount-with-convert etc. are one-pass by construction). |
| REQ-KERNEL-005 | Concrete symbols follow ADR-006: `quiver::detail::<api>_<type>[_<repr>]`, generated from one X-macro inventory shared with the dispatch table ([07 §4](07-runtime-dispatch.md)). |
| REQ-KERNEL-006 | Every backend TU compiles in every build regardless of runtime selection (dead-backend rot prevention); `QUIVER_DISABLE_AVX512` is the only exclusion mechanism. |
| REQ-KERNEL-007 | **Evidence-gated variant selection:** where this chapter marks a technique choice as evidence-gated (K5 gather; K7 NEON), both implementations shall exist in the backend TU behind an internal compile-time constant; the shipped default is selected by ledger comparison at the owning milestone gate; the decision, data, and date are recorded in the family doc page and the losing variant remains compiled (test-covered) for re-evaluation. |
| REQ-KERNEL-008 | Every family doc page shall carry the family's roofline class (§4) and per-ISA implementation notes citing their microarchitectural rationale (Survey references). |

## 3. Numeric semantics (normative, all families)

1. **Integer arithmetic** is two's-complement wrapping unless the API says otherwise (K10 checked/saturating). Implementations shall avoid signed-overflow UB internally via unsigned arithmetic or overflow builtins ([15 §3](15-security-and-ub.md)).
2. **Float comparisons (K1):** IEEE-754 ordered semantics; any comparison involving NaN yields false, except `kNe` yields true. `-0.0 == +0.0`. `between` = two ordered comparisons ANDed.
3. **Float min/max (K6):** if any participating lane is NaN, the result is the canonical quiet NaN of `T` (payload-normalized so results stay bit-identical across ISAs); otherwise IEEE ordering with `-0.0`/`+0.0` treated equal (either zero may be returned, fixed per (version, ISA)).
4. **Float sums (K6):** blocked multi-accumulator policy per ADR-013; bit-identical per (version, ISA); cross-ISA divergence expected and documented; strict-order recourse = scalar reference via `set_isa_override(kScalar)` (Charter §7.4).
5. **Empty/all-invalid reductions:** `min → numeric_limits<T>::max()`, `max → numeric_limits<T>::lowest()`, sums → 0, `Sma{max(), lowest(), null_count = participating-null count}`.
6. **Hash float canonicalization (K7):** `-0.0` hashes as `+0.0`; NaNs hash by bit pattern (documented; engines wanting NaN-normalized hashing canonicalize upstream).

**ADR-013 — Float reduction reassociation policy.** *Status:* Accepted. *Context:* single-accumulator FP reduction is latency-bound, wasting 6–12× throughput (Survey §3.9); Charter §7.4 requires an explicit reassociation policy, prohibits `-ffast-math`, and demands per-(version, ISA) determinism. *Problem:* fix an accumulation order that is fast, deterministic, and specifiable. *Alternatives:* (1) strict left-fold — rejected: forfeits ILP (Survey §3.9); (2) compiler-chosen (`-ffast-math`/pragmas) — rejected: nondeterministic across compilers, charter-prohibited; (3) pairwise/tree over whole batch — rejected: needs O(log n) staging or recursion, complicates masking; (4) **fixed blocked accumulation** (selected): A accumulator registers of the backend's lane width; main loop adds element blocks in order; accumulators combine pairwise `(0+2),(1+3),then +`; horizontal fold low→high lane; tail (`n mod block`) folded sequentially into the scalar result afterward, in index order. Accumulator counts (frozen): **scalar A=1 — strict sequential left-fold**, because the Charter (§7.4) designates the scalar reference as the strict-order recourse; the scalar backend therefore knowingly sacrifices float-sum ILP throughput (Survey §3.9), and this is stated in the K6 doc page and ledger interpretation. NEON/AVX2/AVX-512: A=4 vectors (f32 lanes: 16/32/64; f64: 8/16/32). Note the corollary for baselines: since compilers cannot reassociate FP without fast-math (charter-prohibited), the autovec float-sum baselines are also strict-order — the verdict block for K6 float sums compares explicit-SIMD reassociated variants against a strict baseline and shall say so explicitly. *Consequences:* explicit backends get better worst-case error than left-fold; the testkit implements the policy generically (parameterized by the per-ISA accumulator layout, with A=1 reproducing the scalar backend exactly) so differential tests have an exact float oracle ([12 §2](12-testing-architecture.md), REQ-TEST-004); changing A or the fold order is a semantic change requiring a version bump. *Reconsideration:* ledger evidence that A=4 vectors starves a µarch (then bump A via minor version with CHANGELOG semantic note). *Related:* REQ-K6-003, REQ-API-006.

## 4. Family roofline classes

| Class | Families | Benchmark interpretation guidance (Survey §3.2/§3.8/§3.9) |
|---|---|---|
| Bandwidth-bound | K2, K4, K8, K9 (large n) | compare against STREAM-class ceilings; scaling claims suspect beyond DRAM saturation |
| Latency/MLP-bound | K5 (out-of-cache dict) | interpret via outstanding-miss counts, not IPC; dict-size sweep across cache levels mandatory |
| Compute-bound | K7, K10-checked, K6-float | ILP/port pressure dominate; explicit SIMD most likely to pay here |
| Mixed / selectivity-shaped | K1, K3, K6-int | branch-free claim (REQ-KERNEL-003) shown by flat selectivity curves |

## 5. Family specifications

### K1 — MOD-K1-COMPARE

- **Purpose:** branch-free predicate evaluation into either selection representation (Charter K1; the dual-representation instrument of Survey §11.3 #3).
- **APIs:** API-K1-001..006. **Semantics:** [04 §5 K1](04-public-api.md) + §3.2 above. Validity handling: output bit set iff `valid(i) ∧ pred(i)`, where for the two-batch form `valid(i) = a_valid(i) ∧ b_valid(i)`; a null `validity.bits` means all-valid and shall take a distinct, validity-mask-free code path (no fake all-ones mask materialization).
- **Algorithms:** *scalar:* per-8-element byte assembly — compute predicate bits branchlessly (`unsigned(cond) << k`), OR into a byte, store per byte; selvec form: `out[c] = i; c += cond`. *AVX2:* `vpcmp*` per lane → `vpmovmskb`-derived bytes written to the bitmap; selvec via bitmap fragment + per-byte LUT expansion of indices (MOD-KCOMMON tables). Unsigned integer compares synthesized via bias-XOR (no unsigned compare pre-AVX-512; documented technique). *NEON:* `cmXX` → narrowing `shrn` movemask idiom (Survey §4.1) → bytes. *AVX-512:* native mask registers; `vpcmpu*` covers unsigned directly; masks stored byte-wise; selvec via `vpcompressd` on an index vector (Survey §4.1).
- **Family invariants:** bitmap and selvec forms agree exactly (property-tested equivalence through K3).
- **Specific requirements:** REQ-K1-001 (output = valid ∧ predicate; popcount return), REQ-K1-002 (IEEE float rules §3.2), REQ-K1-003 (both representations produced by every backend tier).
- **Tests/benches (beyond template):** selectivity-flatness bench rows; float-special unit block (±0, NaN, ±inf, denormals); two-batch form length-mismatch assert test.
- **Acceptance:** template + K1-specific rows green; ledger rows for all six APIs at owning milestones.

### K2 — MOD-K2-FILTER

- **Purpose:** dense order-preserving compaction; the AVX-512 compress showcase (Charter K2; Survey §4.1).
- **APIs:** API-K2-001/002. **Semantics:** [04 §5 K2](04-public-api.md).
- **Algorithms:** *scalar:* forward `out[c] = in[i]; c += bit(i)` (branchless). *AVX2:* 32/64-bit lanes: bitmap byte → 256-entry `vpermd`/`vpermq`-index LUT → permute + store + advance by popcount (Survey §4.1 emulated compress); 8/16-bit lanes: nibble-pair `pshufb` LUT compaction at 128-bit granularity (MOD-KCOMMON tables). *NEON:* nibble `TBL` LUT compaction, 64-bit halves (Lemire simdprune lineage — Survey §4.1). *AVX-512:* `vpcompressd/q`; 8/16-bit via `vpcompressb/w` when VBMI2 (resolution-time variant, REQ-DISP-011) else 32-bit widening path; **compress to register then store** (Zen 4 microcode hazard, Survey §4.1) — mandatory implementation note.
- **Family invariants:** order preservation; in-place safety (`out == in.data`, ADR-023).
- **Specific requirements:** REQ-K2-001 (order-preserving, exact count return), REQ-K2-002 (in-place equivalence test row), REQ-K2-003 (selvec-driven form is O(sel.len), never O(n)).
- **Tests/benches:** in-place vs out-of-place equality; selectivity × type × ISA grid is this family's headline ledger table.

### K3 — MOD-K3-SELECT

- **Purpose:** lossless conversion between representations; primary instrument of the M9 representation study (Charter §6.2; Survey §11.3 #3).
- **APIs:** API-K3-001/002. **Semantics:** [04 §5 K3](04-public-api.md).
- **Algorithms:** *bitmap→selvec:* K2's compaction machinery applied to an iota index stream (shared LUTs; scalar: `out[c]=i; c+=bit`). *selvec→bitmap:* zero the output words, then set bits in a forward pass (`out[idx>>3] |= 1<<(idx&7)`); sorted input makes writes near-sequential; word-buffered variant (accumulate bits of the current word before store) is the scalar optimization; explicit SIMD expected marginal — a designated T7 honest-loss candidate.
- **Specific requirements:** REQ-K3-001 (round-trip identity both directions), REQ-K3-002 (K3-001 returns popcount and matches `mask_popcount` exactly).
- **Tests/benches:** round-trip property; agreement with K4 popcount; density-sweep benches feeding M9.

### K4 — MOD-K4-MASK

- **Purpose:** bitmap boolean algebra + cardinality; the null-propagation primitive (Charter K4).
- **APIs:** API-K4-001..004. **Semantics:** [04 §5 K4](04-public-api.md); vacuous-truth rules for n=0.
- **Algorithms:** all ops are 64-bit-word loops (`memcpy` word access, [15 §3](15-security-and-ub.md)) with `std::popcount` accumulation; tail per ADR-016. Auto-vectorizer expected to handle word loops well (Survey §4.4) — explicit AVX2/NEON/AVX-512 backends still implemented per REQ-KERNEL-006, with the autovec-vs-explicit verdict published either way (T7); `mask_all/any/none` short-circuit at word granularity (data-dependent early exit is permitted here — these are queries, not selection-dependent transforms; REQ-KERNEL-003 does not apply).
- **Specific requirements:** REQ-K4-001 (word-loop implementation shape; bitwise results exact), REQ-K4-002 (vacuous-truth semantics), REQ-K4-003 (in-place aliasing rows of ADR-023 tested).
- **Tests/benches:** exhaustive small-n (0..129) truth-table tests vs `reference.h`; popcount agreement with K3.

### K5 — MOD-K5-TAKE

- **Purpose:** gather/dictionary decode; the MLP-bound family (Charter K5; Survey §3.9, §4.2).
- **APIs:** API-K5-001..003. **Semantics:** [04 §5 K5](04-public-api.md); fused form output is **packed** (`out[j] = dict[codes[sel.idx[j]]]`).
- **Algorithms:** *scalar:* unrolled independent loads (≥8 in flight) to expose MLP (Survey §3.9). *AVX2/AVX-512:* **evidence-gated** (REQ-KERNEL-007): (a) hardware `vpgatherdd/q` path vs (b) scalar-load/insert path — Survey §4.2 predicts (b) out-of-cache and mixed results in-cache; the M4/M7 ledger decides per (ISA, element width). *NEON:* no gather exists; unrolled scalar loads with NEON stores (Survey §4.1).
- **Specific requirements:** REQ-K5-001 (arbitrary/duplicate indices legal for `take`), REQ-K5-002 (bounds precondition debug-asserted: full scan under asserts, no cost in release), REQ-K5-003 (fused decode touches only selected code positions — no full-batch decode; validated by guard-page test in [12 §2](12-testing-architecture.md)), REQ-K5-004 (evidence-gated gather decision recorded per REQ-KERNEL-007).
- **Tests/benches:** duplicate/reverse index tests; dict-size sweep {4 KiB, 32 KiB, 256 KiB, 8 MiB, 64 MiB} crossing cache levels (roofline class mandate, §4).

### K6 — MOD-K6-REDUCE

- **Purpose:** single-pass reductions and SMA (Charter K6).
- **APIs:** API-K6-001..005. **Semantics:** [04 §5 K6](04-public-api.md) + §3 above; participation = selected ∧ valid.
- **Algorithms:** *integers:* multi-accumulator wrapping sums (widen at load: 8/16→64 via staged widening adds — pairwise widening to avoid per-element widening cost); min/max via SIMD min/max with identity-filled masked lanes (validity byte → lane mask via KCOMMON expansion LUT / `vpmovm2*` on AVX-512). *floats:* ADR-013 policy; NaN canonicalization post-pass for min/max (§3.3). *checked i64/u64 sum:* 128-bit accumulation (compiler `__int128` on GCC/Clang; MSVC `_addcarry_u64` pair) — scalar-class implementation permitted, ledgered honestly (T7). *SMA:* fused min+max+null-count single pass.
- **Specific requirements:** REQ-K6-001 (participation rule), REQ-K6-002 (identity results for empty participation), REQ-K6-003 (float policy conformance — testkit policy oracle), REQ-K6-004 (checked-sum exactness: property vs big-integer reference in testkit), REQ-K6-005 (SMA single-pass: equals composition of min/max/count oracles).
- **Tests/benches:** NaN/±0/denormal blocks; null-density × selectivity bench axes; accumulator-policy conformance test.

### K7 — MOD-K7-HASH

- **Purpose:** batch hashing with cross-platform stable output (Charter K7/§7.4).
- **APIs:** API-K7-001/002.
- **ADR-012 — qhash64 v1 algorithm.** *Status:* Accepted. *Context:* Charter defers the algorithm to the PRD inside a stability contract: cross-ISA/platform identical, stable per major version, non-cryptographic, engine-partitioning quality. *Problem:* pick a fixed-width-key hash that vectorizes acceptably on ISAs lacking 64-bit vector multiply (AVX2/NEON) while retaining finalizer-grade avalanche. *Alternatives:* (1) xxHash3/wyhash-style 64×64→128 folding — rejected: 128-bit multiply is scalar-only on all target ISAs, forcing either scalar hashing or a different vector algorithm (which would violate cross-ISA identity); (2) CRC32-based — rejected: weak distribution, hardware-dependent availability; (3) AES-round-based — rejected: not baseline on x86-64-v1, cross-ISA identity fragile; (4) **Murmur3-style 64-bit finalizer chain with seed pre-mix** (selected): only 64-bit multiplies by constants + xorshifts — vectorizable via native `vpmullq` (AVX-512) and 32-bit-multiply decomposition (AVX2/NEON), identical results everywhere. *Definition (frozen):*

  ```text
  GOLDEN = 0x9E3779B97F4A7C15   C1 = 0xFF51AFD7ED558CCD   C2 = 0xC4CEB9FE1A85EC53
  fmix64(x) : x ^= x>>33; x *= C1; x ^= x>>33; x *= C2; x ^= x>>33; return x
  key64(v)  : integers → zero-extended bit pattern (i8/i16/i32 sign bits NOT extended:
              the two's-complement bit pattern is zero-extended, documented);
              f32/f64 → bit pattern, -0.0 canonicalized to +0.0 first
  qhash64(v, seed)      = fmix64(key64(v) ^ (seed + GOLDEN))
  hash64_combine(a, b)  = fmix64(a ^ (b + GOLDEN + (a << 6) + (a >> 2)))
  ```

  *Quality gate:* first-party avalanche suite ([12 §2](12-testing-architecture.md), REQ-TEST-016): over ≥100k seeded samples per type, every input-bit flip shall flip each output bit with probability within 0.5 ± 0.02, and no output bit may show overall bias > 0.02 — a documented SMHasher-subset, with the limitation stated (full SMHasher run is a dev-time activity recorded in the family doc, not a CI dependency). *Consequences:* test vectors frozen into `tests/golden/qhash64_vectors.txt` at M6; any change to constants/rounds is a major-version event. *Reconsideration:* v1.x string hashing may introduce a companion algorithm; qhash64 itself only changes at v2. *Related:* REQ-K7-001..004, Charter §7.4.
- **Algorithms:** *scalar:* fmix64 chain, 4×-unrolled. *AVX-512:* `vpmullq`-based vertical fmix64. *AVX2:* 64-bit multiply decomposed into three `vpmuludq` + shifts (documented standard technique). *NEON:* **evidence-gated** (REQ-KERNEL-007): (a) vector decomposition via `umull`/`mla` vs (b) unrolled GPR scalar (Apple's 2 GPR mul pipes may win — Survey §3.9/§4.1); M6 ledger decides.
- **Specific requirements:** REQ-K7-001 (frozen algorithm/constants), REQ-K7-002 (cross-ISA/platform bit-identity — differential + committed vectors), REQ-K7-003 (avalanche gate), REQ-K7-004 (no validity parameter; doc page explains the composition idiom for nulls).
- **Tests/benches:** golden vectors; cross-compile vector equality (x86 CI vs ARM CI compare committed hashes); throughput per type per ISA.

### K8 — MOD-K8-UNPACK

- **Purpose:** bit-unpacking with FOR fusion (Charter K8; Survey §1.4 PFOR lineage).
- **APIs:** API-K8-001/002.
- **ADR-026 — Bit-packing layout.** *Status:* Accepted. *Problem:* fix the packed layout. *Alternatives:* MSB-first (rejected: no ecosystem pull), 32-value lane-interleaved FastLanes layout (rejected for v1: ties Quiver to an evolving format — Charter §8.1 defers compressed-format coupling), **LSB-first little-endian contiguous** (selected): value *i* occupies bits `[i·w, (i+1)·w)`; bit *j* lives at byte `j/8`, bit `j%8`. Parquet-RLE-bit-packing-compatible, trivially specifiable, byte-order-independent definition. *Reconsideration:* v2 compressed-kernel expansion may add interleaved layouts as new APIs. *Related:* REQ-K8-001.
- **Algorithms:** *scalar:* 64-bit shift-buffer loop (refill from `memcpy` word loads, exact tail bytes per REQ-MEM-001). *AVX2/AVX-512:* per-width specialized kernels for w ∈ {1,2,4,8,16,32} (aligned sub-word shuffles + variable shifts + masks), generic shift-buffer path for all other widths; width dispatch is a resolution-free `switch` at kernel entry (one predictable branch, then straight-line — not data-dependent, REQ-KERNEL-003 preserved). *NEON:* same structure with `TBL`/`ushl` variable shifts. FOR fusion = one wrapping add in the store path.
- **Specific requirements:** REQ-K8-001 (ADR-026 layout), REQ-K8-002 (reads exactly `⌈n·w/8⌉` bytes — page-guard tested, fuzz-prioritized: REQ-SEC-004), REQ-K8-003 (w=0 → all zeros/`base`, `packed` may be null), REQ-K8-004 (full width sweep 0..8·sizeof(Out) in the differential matrix).
- **Tests/benches:** width-exhaustive differential; guard-page boundary tests; width × Out throughput grid.

### K9 — MOD-K9-ARITH

- **Purpose:** elementwise wrapping/IEEE arithmetic with validity composition (Charter K9).
- **APIs:** API-K9-001/002. **Semantics:** [04 §5 K9](04-public-api.md); K9-002 ≡ values-at-all-lanes + `mask_combine(kAnd)` — implemented by calling K4's public API (the documented cross-family exception, [02 §6](02-repository-architecture.md)).
- **Algorithms:** direct SIMD add/sub/mul per lane width; 64-bit multiply on AVX2/NEON via the same decomposition as K7; floats native. Auto-vectorizer competitive here — expected T7 verdict territory.
- **Specific requirements:** REQ-K9-001 (wrapping semantics, no UB path — unsigned internal arithmetic), REQ-K9-002 (validity overload equals documented composition — property test).
- **Tests/benches:** boundary values; composition-equality property; bandwidth-bound interpretation note per §4.

### K10 — MOD-K10-ARITH-GUARDED

- **Purpose:** overflow-guarded arithmetic; never silent UB (Charter K10/§7.4).
- **APIs:** API-K10-001/002.
- **ADR-014 — Overflow reporting design.** *Status:* Accepted. *Context:* Charter Appendix A defers granularity (flag vs first-index vs mask) inside the no-silent-UB contract. *Alternatives:* (1) boolean flag only — rejected: callers needing positions would rerun scalar; (2) first overflow index — rejected: forces early-exit data-dependent branch into the hot loop (violates REQ-KERNEL-003 spirit) and loses total count; (3) **count return + optional position bitmap** (selected): count accumulates branchlessly; the nullable bitmap writes positions only when the caller wants them; positions compose with K2/K3 for extraction — reuses the library's own vocabulary. *Consequences:* two variants of the inner loop (bitmap/no-bitmap) per backend — bounded cost; `overflow_bits` tail-zeroed per ADR-016. *Reconsideration:* none before v2. *Related:* REQ-K10-001/002, API-K10-001.
- **Algorithms:** *checked add/sub:* sign-trick overflow detection (`((a^r)&(b^r)) < 0` signed; carry compare unsigned) — fully vectorizable all ISAs; *checked mul:* 8/16/32-bit via widening multiply + range compare (vectorized); 64-bit via scalar `__builtin_mul_overflow` loop (documented, ledgered — T7). *saturating:* NEON native `sqadd/uqadd/sqsub/uqsub`; x86 native for 8/16-bit, compare+blend clamp for 32/64-bit; saturating mul via widening/clamp (8/16/32) and scalar clamp path (64).
- **Specific requirements:** REQ-K10-001 (wrapped results at all lanes + exact count + exact position bitmap), REQ-K10-002 (saturation clamps exactly at type limits; `INT_MIN` negation/mul edge cases enumerated in unit tests), REQ-K10-003 (64-bit checked/saturating mul may be scalar; the family doc states it and the ledger shows it).
- **Tests/benches:** boundary matrix (`{min, min+1, -1, 0, 1, max-1, max}` cross-products, mixed-sign mul); checked-vs-wrap overhead benches per §4 compute-bound framing.

## 6. ADR-025 — Selection-vector semantics

*Status:* Accepted. *Context:* Charter §6.2 fixes sorted-unique for selection and arbitrary for `take`; the engineering question is enforcement. *Alternatives:* (1) validate eagerly in release — rejected: O(n) tax on every call contradicts the performance identity of the library; (2) tolerate unsorted input in selection kernels ("works by accident") — rejected: forecloses forward-scan algorithms and in-place filter safety; (3) **contract + debug assertion** (selected): sortedness/in-range checked fully under `QUIVER_ENABLE_ASSERTS` (O(n) scan acceptable in debug), UB in release, fuzz harnesses generate only contract-valid selections while separately fuzzing K8's untrusted-input surface. *Consequences:* documented loudly on every SelVec API; `inv_selvec_sorted` validates producers, assert tests validate consumers. *Related:* REQ-MEM-007, REQ-K5-001/002.

## 7. Acceptance criteria (chapter)

Every family meets the template acceptance + its specific REQs; the two evidence-gated decisions (K5, K7) are recorded with ledger data at their gates; numeric-semantics conformance suites (float specials, boundary integers) pass on every backend including under SDE; the family doc pages carry roofline class, per-ISA notes, and honest verdicts.

## 8. Traceability

Charter §6.1 (catalog), §6.2 (representations), §7.4 (determinism/hash/float/overflow), Appendix A (canonical semantics) → REQ-KERNEL-001..008, REQ-K1..K10 blocks → ADR-012/013/014/025/026 (here) + ADR-006/016/023 (elsewhere) → APIs ([04](04-public-api.md)) → tests ([12](12-testing-architecture.md)) → benches ([10](10-benchmark-architecture.md)) → milestones M3–M7 ([18](18-milestones.md)). Survey authority cited inline throughout (§1.4, §2.3, §3.2–3.9, §4.1–4.5, §11.3–11.4).
