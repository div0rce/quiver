# 04 — Public API Specification

## 1. Purpose

Defines every externally visible interface as a stable engineering contract: the vocabulary types (Charter Surface B), the dispatch/introspection functions (Surface C), and the ten kernel-family APIs (Surface A). Implementation satisfies this specification; it never extends it. Kernel *semantics and algorithms* are specified in [08-kernel-design.md](08-kernel-design.md); this chapter owns signatures and caller-facing contracts.

## 2. API-wide requirements

| ID | Requirement |
|---|---|
| REQ-API-001 | All public declarations shall live in `namespace quiver { inline namespace v1 { … } }` (ADR-007). |
| REQ-API-002 | Every public function shall be `noexcept`. No public function shall throw, allocate, perform I/O, log, or touch global mutable state, except dispatch resolution state as specified in [07](07-runtime-dispatch.md). |
| REQ-API-003 | All parameters are borrowed: the caller owns every buffer; no API takes or transfers ownership; no pointer is retained beyond the call (lifetime = call duration). |
| REQ-API-004 | Supported element types are exactly: `int8_t, int16_t, int32_t, int64_t, uint8_t, uint16_t, uint32_t, uint64_t, float, double`, constrained by the `Element` concept. Dictionary code types are exactly `uint8_t, uint16_t, uint32_t` (`CodeType` concept). Instantiation with any other type shall fail to compile with a `static_assert`/concept diagnostic. |
| REQ-API-005 | Every kernel accepts batch lengths `0 ≤ n ≤ kMaxBatchLen = 2'147'483'647` (2³¹−1). `n == 0` is valid and produces the documented empty result. `n > kMaxBatchLen` violates the contract (debug-asserted). Rationale: selection indices fit `uint32_t` with headroom; batches are cache-sized by design (Survey §1.4, §11.3 #1). |
| REQ-API-006 | Determinism: identical inputs produce bit-identical outputs for a given (library version, active ISA). Integer and hash kernels are additionally bit-identical **across** ISAs and platforms (Charter §7.4; validated by REQ-TEST differential suites). |
| REQ-API-007 | The public API is the template façade defined in this chapter. Concrete per-type symbols in `quiver::detail` are implementation details with no stability guarantee (ADR-006). |
| REQ-API-008 | Validity/selection bitmap parameters follow one rule set: `BitmapView.bits == nullptr` means "all valid" **only** where the parameter is named `validity`; parameters named `selection` shall be non-null (debug-asserted). |
| REQ-API-009 | No public signature shall change after v1.0 (Charter §7.5). During 0.x, breaking changes require a CHANGELOG entry and a minor-version bump. |
| REQ-API-010 | Every public API shall have: unit tests, property tests, a differential-matrix entry, at least one benchmark, and a reference page under `docs/api/` (cross-refs in §9, [12](12-testing-architecture.md), [10](10-benchmark-architecture.md), [14](14-documentation.md)). |
| REQ-API-011 | Output-parameter capacity and defined-output contracts are as tabulated in [06 §6](06-memory-model.md); kernels fully write their defined output regions, may write scratch only within the capacity regions of compaction outputs (REQ-MEM-008), and never touch any byte beyond a capacity region. |
| REQ-API-012 | Aliasing: outputs shall not partially overlap any input. Exact aliasing (`out == in.data`) is permitted only where the API's spec says so (ADR-023, [06 §7](06-memory-model.md)). |

### Common API contract (inherited by every kernel API below)

Unless a specific API states a delta, every kernel API has: **Ownership/lifetime** per REQ-API-003. **Allocation** none, ever. **Exceptions** `noexcept`, cannot throw. **Thread safety** thread-compatible pure function: concurrent calls are safe when output ranges are disjoint; no synchronization performed or required (first call may trigger dispatch resolution, which is thread-safe per [07](07-runtime-dispatch.md)). **Side effects** none beyond writing the documented output ranges and returning a value. **Invalid input behavior** contract violation = undefined behavior in release builds, `QUIVER_ASSERT` failure (stderr + abort) when asserts are enabled ([16](16-error-handling.md)); for inputs satisfying all preconditions, no execution may exhibit UB or touch memory outside documented ranges (Charter §7.3, REQ-SEC-001). **Complexity** O(n) single pass unless stated. **SIMD** dispatched per [07](07-runtime-dispatch.md); scalar fallback always exists and defines the semantics (Charter T3).

## 3. Surface B — vocabulary types (`quiver/core.h`) — API-CORE-001

```cpp
namespace quiver { inline namespace v1 {

inline constexpr std::int64_t kMaxBatchLen = 2'147'483'647;

template <class T>
concept Element  = /* exactly the ten types of REQ-API-004 */;
template <class T>
concept IntElement = Element<T> && std::integral<T>;
template <class T>
concept CodeType = std::same_as<T, std::uint8_t> || std::same_as<T, std::uint16_t>
                 || std::same_as<T, std::uint32_t>;

enum class CompareOp : std::uint8_t { kEq, kNe, kLt, kLe, kGt, kGe };
enum class MaskOp    : std::uint8_t { kAnd, kOr, kAndNot, kXor };
enum class ArithOp   : std::uint8_t { kAdd, kSub, kMul };
enum class Isa       : std::uint8_t { kScalar = 0, kNeon = 1, kAvx2 = 2, kAvx512 = 3 };

template <Element T> struct BatchView { const T* data; std::int64_t len; };
struct BitmapView { const std::uint8_t* bits; };   // LSB-first, 1 = valid/selected (Charter §6.2)
struct SelVec     { const std::uint32_t* idx; std::int64_t len; };
template <Element T> struct Sma { T min; T max; std::int64_t null_count; };

template <Element T> using SumType =
  /* int64_t for signed ints, uint64_t for unsigned ints, T for float/double */;
}} // namespace
```

Contracts: all four structs are trivially copyable non-owning views (pass by value); `BatchView` requires `data != nullptr` when `len > 0`; `BitmapView` must cover ≥ *n* bits of the operation it accompanies, tail bits beyond *n* ignored on input; `SelVec` for selection semantics requires strictly increasing in-range indices (ADR-025) — `take` relaxes this per API-K5-001. `Sma` field semantics: identity values when no valid element participates (`min = numeric_limits<T>::max()`, `max = lowest()`, documented in [08 §3](08-kernel-design.md)). **Tests:** static-assert suite `test_core.cpp` (triviality, sizes, concept accept/reject). **Benchmarks:** none (no behavior).

## 4. Surface C — dispatch and introspection (`quiver/dispatch.h`)

Full lifecycle semantics in [07-runtime-dispatch.md](07-runtime-dispatch.md); contracts here.

| API ID | Signature | Contract summary |
|---|---|---|
| API-DISP-001 | `Isa active_isa() noexcept;` | Returns the ISA that dispatch would select now (highest supported, capped by any active override). Thread-safe. No allocation. |
| API-DISP-002 | `bool cpu_supports(Isa isa) noexcept;` | True iff the current CPU+OS can execute the given tier (x86: includes XGETBV OS-state checks; ARM: `kNeon` always true, `kAvx*` always false). Pure query; thread-safe. |
| API-DISP-003 | `bool set_isa_override(Isa isa) noexcept;` `void clear_isa_override() noexcept;` | Caps dispatch at `isa`. Returns `false` (no state change) if unsupported. Takes effect for subsequent calls via the policy epoch ([07 §5](07-runtime-dispatch.md)); calls concurrent with an override change may execute under either policy — both are correct. Intended for benchmarking and diagnostics, not steady-state hot paths. |
| API-DISP-004 | `void warmup() noexcept;` | Eagerly resolves every dispatch entry for the current policy. Idempotent; thread-safe. Use before latency-sensitive first calls. |
| API-DISP-005 | `struct Version { int major, minor, patch; };` `Version version() noexcept;` `const char* version_string() noexcept;` | Compile-embedded version; the string has static storage duration and is never freed. |

**Preconditions:** none (all inputs total). **Invalid input:** `set_isa_override` with an out-of-range enum value returns `false`. **Examples:** `examples/03_isa_override.cpp`. **Tests:** `test_dispatch.cpp`, TSan concurrent-resolution test. **Benchmarks:** `bench_dispatch.cpp` (dispatch overhead vs direct call; override-epoch cost).

## 5. Surface A — kernel APIs

Conventions used below: `n` = element count of the primary input batch; “→ count” = the function returns `std::int64_t`, the number of elements/bits written or matched; capacity bounds per [06 §6](06-memory-model.md). All APIs are function templates over `Element T` (façade, ADR-006) unless typed otherwise.

### K1 — compare (`quiver/compare.h`)

| API ID | Signature |
|---|---|
| API-K1-001 | `template<Element T> std::int64_t compare_bitmap(CompareOp op, BatchView<T> in, T comparand, BitmapView validity, std::uint8_t* out_bits) noexcept;` |
| API-K1-002 | `template<Element T> std::int64_t compare_bitmap(CompareOp op, BatchView<T> a, BatchView<T> b, BitmapView a_validity, BitmapView b_validity, std::uint8_t* out_bits) noexcept;` |
| API-K1-003 | `template<Element T> std::int64_t compare_between_bitmap(BatchView<T> in, T lo, T hi, BitmapView validity, std::uint8_t* out_bits) noexcept;` — selects `lo ≤ x && x ≤ hi` (inclusive) |
| API-K1-004/005/006 | `_selvec` counterparts of the three above: final parameter `std::uint32_t* out_idx`; → count of indices written |

**Purpose:** branch-free predicate evaluation producing either selection representation (Charter K1; Survey §3.4). **Preconditions:** batch lengths equal for the two-batch form; capacities per [06 §6](06-memory-model.md); REQ-API-005/008/012. **Postconditions:** bitmap forms set bit *i* iff element *i* is valid **and** satisfies the predicate, zero all other bits including tail bits (ADR-016), and → popcount; selvec forms write strictly increasing indices as the defined output (first *count* entries; capacity region `n` per REQ-MEM-008) and → count. Two-batch form (API-K1-002): element *i* is valid iff `a_valid(i) ∧ b_valid(i)`. Floats: IEEE-754 ordered comparisons — any comparison with NaN is false except `kNe`, which is true (`kNe` with NaN operand selects; [08 §3](08-kernel-design.md)); `-0.0 == +0.0`. **Complexity:** O(n), selectivity-independent (branch-free core). **Tests/benchmarks:** [12 §2, §5](12-testing-architecture.md) matrix rows K1; `bench_compare.cpp` (selectivity × type × ISA axes).

```cpp
// minimal:      select i32 values > 10 into a bitmap
std::int64_t hits = quiver::compare_bitmap(quiver::CompareOp::kGt,
    {vals, n}, 10, {nullptr}, out_bits);
// edge (n==0):  returns 0, writes nothing
quiver::compare_bitmap(quiver::CompareOp::kGt, {vals, 0}, 10, {nullptr}, out_bits);
```

### K2 — filter (`quiver/filter.h`)

| API ID | Signature |
|---|---|
| API-K2-001 | `template<Element T> std::int64_t filter(BatchView<T> in, BitmapView selection, T* out) noexcept;` |
| API-K2-002 | `template<Element T> std::int64_t filter(BatchView<T> in, SelVec sel, T* out) noexcept;` |

**Purpose:** compact selected elements densely, order-preserving (Charter K2). **Preconditions:** `selection.bits != nullptr` (REQ-API-008); `sel` sorted-unique in-range (ADR-025); capacity ≥ selection cardinality. **Postconditions:** defined output = first *count* elements, in input order (bitmap form: capacity region `n`, REQ-MEM-008; selvec form writes exactly `sel.len`); → count (bitmap form: popcount over first *n* bits; selvec form: `sel.len`). **Aliasing delta:** `out == in.data` exact aliasing is permitted (forward compaction invariant, ADR-023). **Complexity:** O(n) bitmap form; O(sel.len) selvec form. **Tests/benchmarks:** matrix rows K2; `bench_filter.cpp` — the AVX-512 `vpcompress` showcase and Zen 4 store-path note land here (Survey §4.1).

```cpp
// typical: predicate then compact, reusing the bitmap
std::int64_t cnt = quiver::compare_bitmap(quiver::CompareOp::kLt, {v, n}, cutoff,
                                          {nullptr}, bits);
quiver::filter({v, n}, quiver::BitmapView{bits}, out);       // writes cnt values
// edge: in-place (out == v) is legal for filter
quiver::filter({v, n}, quiver::BitmapView{bits}, v);
```

### K3 — sel_convert (`quiver/select.h`)

| API ID | Signature |
|---|---|
| API-K3-001 | `std::int64_t bitmap_to_selvec(BitmapView selection, std::int64_t n, std::uint32_t* out_idx) noexcept;` |
| API-K3-002 | `void selvec_to_bitmap(SelVec sel, std::int64_t n, std::uint8_t* out_bits) noexcept;` |

**Purpose:** lossless conversion between the two selection representations (Charter §6.2 dual-representation decision; Survey §11.3 #3). **Preconditions:** `selection.bits != nullptr`; `sel` sorted-unique with all `idx < n`; capacities per [06 §6](06-memory-model.md). **Postconditions:** K3-001's defined output is the first *count* = popcount strictly increasing indices (capacity region `n`, REQ-MEM-008). K3-002 writes a bitmap whose set bits are exactly `sel`, all other bits (including tail) zero. Round-trip `bitmap_to_selvec ∘ selvec_to_bitmap` and vice versa are identities (property-tested). **Not templates** (representation-only, no element type). **Tests/benchmarks:** matrix rows K3; `bench_select.cpp` — primary instrument of the M9 representation study.

```cpp
// typical: switch representation to feed take(); edge: empty bitmap -> count 0
std::int64_t cnt = quiver::bitmap_to_selvec(quiver::BitmapView{bits}, n, idx);
quiver::take({vals, n}, quiver::SelVec{idx, cnt}, out);
```

### K4 — mask_algebra (`quiver/mask.h`)

| API ID | Signature |
|---|---|
| API-K4-001 | `void mask_combine(MaskOp op, BitmapView a, BitmapView b, std::int64_t n, std::uint8_t* out_bits) noexcept;` |
| API-K4-002 | `void mask_not(BitmapView a, std::int64_t n, std::uint8_t* out_bits) noexcept;` |
| API-K4-003 | `std::int64_t mask_popcount(BitmapView a, std::int64_t n) noexcept;` |
| API-K4-004 | `bool mask_all(BitmapView a, std::int64_t n) noexcept;` `bool mask_any(...) noexcept;` `bool mask_none(...) noexcept;` |

**Purpose:** bitmap boolean algebra and cardinality; the validity-combination primitive for null propagation (Charter K4). **Preconditions:** all `bits != nullptr` here (these APIs *are* the mask operations; the "null = all valid" shorthand does not apply); `n ≥ 0`. **Postconditions:** combine/not write `⌈n/8⌉` bytes with tail bits zero (ADR-016); `mask_all/any/none` are true for `n == 0` as: all=true, any=false, none=true (vacuous truth, documented). **Aliasing delta:** `out_bits == a.bits` or `== b.bits` exact aliasing permitted. **Tests/benchmarks:** matrix rows K4; `bench_mask.cpp` — a designated T7 candidate where auto-vectorization is expected to match explicit SIMD (Survey §4.4); the verdict is published either way.

```cpp
// typical: null-propagating AND of two validities; edge: n==0 -> all()==true, any()==false
quiver::mask_combine(quiver::MaskOp::kAnd, {a_valid}, {b_valid}, n, out_valid);
std::int64_t nulls = n - quiver::mask_popcount(quiver::BitmapView{out_valid}, n);
```

### K5 — take / dict_decode (`quiver/take.h`)

| API ID | Signature |
|---|---|
| API-K5-001 | `template<Element T> void take(BatchView<T> values, SelVec indices, T* out) noexcept;` |
| API-K5-002 | `template<Element T, CodeType C> void dict_decode(BatchView<T> dict, const C* codes, std::int64_t n, T* out) noexcept;` |
| API-K5-003 | `template<Element T, CodeType C> void dict_decode(BatchView<T> dict, const C* codes, std::int64_t n, SelVec sel, T* out) noexcept;` |

**Purpose:** gather by index; dictionary decode as its special case; selection-fused decode touches only surviving positions (Charter K5; Survey §2.4 lazy-decode lineage). **Precondition deltas:** `indices` (K5-001) may be **unsorted and contain duplicates** but every index must satisfy `idx[i] < values.len` (ADR-025); all `codes[i] < dict.len` (K5-002) — for K5-003 only at selected positions `codes[sel.idx[j]]`; `sel` sorted-unique with `idx < n`. **Postconditions:** K5-001 writes `indices.len` elements, `out[i] = values.data[indices.idx[i]]`; K5-002 writes `n` elements; K5-003 writes `sel.len` **packed** elements, `out[j] = dict.data[codes[sel.idx[j]]]`. **Complexity:** O(output length); random-access bound — the performance contract is MLP-oriented, not bandwidth-oriented (Survey §3.9). **Evidence-gated variant:** AVX2/AVX-512 gather vs scalar-load implementations both exist; shipped default chosen by ledger at the owning milestone gate (REQ-KERNEL-007, Survey §4.2). **Tests/benchmarks:** matrix rows K5; `bench_take.cpp` (dict size sweep across cache levels).

```cpp
// typical: decode only rows that survived a filter
quiver::dict_decode(quiver::BatchView<int64_t>{dict, dlen}, codes, n,
                    quiver::SelVec{sel, cnt}, out);   // writes cnt packed values
```

### K6 — reduce / SMA (`quiver/reduce.h`)

| API ID | Signature |
|---|---|
| API-K6-001 | `template<Element T> T reduce_min(BatchView<T> in, BitmapView validity) noexcept;` + overload `(…, SelVec sel)`; same shape for `reduce_max` |
| API-K6-002 | `template<Element T> SumType<T> reduce_sum_wrap(BatchView<T> in, BitmapView validity) noexcept;` + `(…, SelVec sel)` overload |
| API-K6-003 | `template<IntElement T> bool reduce_sum_checked(BatchView<T> in, BitmapView validity, SumType<T>* out_sum) noexcept;` + `SelVec` overload — → true iff the mathematical sum is unrepresentable in `SumType<T>` (`*out_sum` then holds the wrapped value) |
| API-K6-004 | `template<Element T> Sma<T> compute_sma(BatchView<T> in, BitmapView validity) noexcept;` + overload `(…, SelVec sel)` (participation = selected ∧ valid; `null_count` counts selected-but-invalid positions) |
| API-K6-005 | `std::int64_t reduce_count_valid(BitmapView validity, std::int64_t n) noexcept;` — defined as `validity.bits ? mask_popcount(validity, n) : n` (public-API delegation to K4, [02 §6](02-repository-architecture.md)) — + overload `(BitmapView validity, std::int64_t n, SelVec sel)` counting selected ∧ valid (`sel.len` when validity is null) |

**Purpose:** single-pass reductions with optional validity and selection; SMA = min+max+null_count in one pass (Charter K6). **Semantics:** an element participates iff selected ∧ valid. Empty participation → identities: `min = numeric_limits<T>::max()`, `max = lowest()`, sums = 0, `Sma = {max_ident, lowest_ident, null_count}`. Integer sums accumulate in `SumType<T>` (64-bit): narrow types cannot overflow within `kMaxBatchLen` (proof: 2³¹ × 2³¹ < 2⁶³), so `reduce_sum_checked` for them always returns false; for 64-bit inputs the check is exact (128-bit accumulation, scalar-class speed permitted and ledgered — [08 §K6](08-kernel-design.md)). Floats: accumulate in `T` under the fixed blocked-accumulator policy of ADR-013 — bit-identical per (version, ISA), documented divergence from strict left-fold, strict-order recourse = the scalar reference (Charter §7.4); min/max propagate NaN (any participating NaN → NaN result); `-0.0`/`+0.0` ordering may return either zero. **Tests/benchmarks:** matrix rows K6 incl. NaN/±0 cases and identity cases; `bench_reduce.cpp` (null-density × selectivity axes; ADR-013 accumulator-count evidence).

```cpp
// minimal: sum with nulls          // edge: empty selection -> identities
auto s   = quiver::reduce_sum_wrap(quiver::BatchView<int32_t>{v, n}, {valid});
auto sma = quiver::compute_sma(quiver::BatchView<double>{d, n}, {valid},
                               quiver::SelVec{idx, 0});   // sma.min == +max, sma.max == lowest
```

### K7 — hash (`quiver/hash.h`)

| API ID | Signature |
|---|---|
| API-K7-001 | `template<Element T> void hash64(BatchView<T> in, std::uint64_t seed, std::uint64_t* out) noexcept;` |
| API-K7-002 | `void hash64_combine(const std::uint64_t* a, const std::uint64_t* b, std::int64_t n, std::uint64_t* out) noexcept;` |

**Purpose:** batch hashing of fixed-width keys; multi-column keys via combine (Charter K7). **Semantics:** `out[i] = qhash64(in.data[i], seed)`; algorithm, constants, float canonicalization (`-0.0 → +0.0` before hashing; NaN payloads hash as their bit patterns), and combine formula are frozen in ADR-012 ([08 §K7](08-kernel-design.md)). **Guarantees:** output bit-identical across ISAs and platforms; stable within a major version; **non-cryptographic** (Charter §7.4). No validity parameter by design: null handling is engine policy; callers combine validity externally (rationale in [08 §K7](08-kernel-design.md)). **Aliasing delta (K7-002):** `out == a` or `out == b` permitted. **Tests:** frozen test vectors (committed golden file), cross-ISA equality, first-party avalanche/bias suite ([12 §2](12-testing-architecture.md), REQ-TEST-016). **Benchmarks:** `bench_hash.cpp`; NEON GPR-vs-vector evidence-gated variant (REQ-KERNEL-007; Survey §3.9, §4.1).

```cpp
// typical: two-column key; edge: null handling is the caller's policy (see family doc)
quiver::hash64(quiver::BatchView<int64_t>{k1, n}, seed, h1);
quiver::hash64(quiver::BatchView<int32_t>{k2, n}, seed, h2);
quiver::hash64_combine(h1, h2, n, h1);   // out == a aliasing permitted
```

### K8 — unpack (`quiver/unpack.h`)

| API ID | Signature |
|---|---|
| API-K8-001 | `template<class Out> void unpack(const std::uint8_t* packed, std::int64_t n, int bit_width, Out* out) noexcept;` — `Out ∈ {uint8_t, uint16_t, uint32_t, uint64_t}` |
| API-K8-002 | `template<class Out> void unpack_for(const std::uint8_t* packed, std::int64_t n, int bit_width, Out base, Out* out) noexcept;` — `out[i] = base + value_i` (wrapping) |

**Purpose:** bit-unpack packed integers; frame-of-reference fusion (Charter K8; Survey §1.4 PFOR lineage). **Layout (ADR-026):** value *i* occupies bits `[i·w, (i+1)·w)` of the stream; bit *j* = byte `j/8`, bit `j%8` (LSB-first little-endian; Parquet-compatible). **Preconditions:** `0 ≤ bit_width ≤ 8·sizeof(Out)`; `packed` readable for exactly `⌈n·bit_width/8⌉` bytes — the kernel shall not read beyond that bound (hardened + fuzz-prioritized: REQ-SEC-004); `bit_width == 0` means all values equal 0 (or `base` for `unpack_for`) and `packed` may be null. **Postconditions:** writes exactly `n` elements. **Tests/benchmarks:** width sweep 0..8·sizeof(Out) exhaustive in the differential matrix; `bench_unpack.cpp` (width × Out × ISA).

```cpp
// typical: 12-bit FOR-encoded column; edge: width 0 -> constant base, packed may be null
quiver::unpack_for<std::uint32_t>(packed, n, 12, base, out);
quiver::unpack_for<std::uint32_t>(nullptr, n, 0, base, out);   // out[i] == base
```

### K9 — arith (`quiver/arith.h`)

| API ID | Signature |
|---|---|
| API-K9-001 | `template<Element T> void arith(ArithOp op, BatchView<T> a, BatchView<T> b, T* out) noexcept;` + scalar-rhs overload `(ArithOp, BatchView<T> a, T b, T* out)` |
| API-K9-002 | `template<Element T> void arith(ArithOp op, BatchView<T> a, BatchView<T> b, BitmapView a_validity, BitmapView b_validity, T* out, std::uint8_t* out_validity) noexcept;` |

**Purpose:** elementwise add/sub/mul; wrapping for integers (explicit in the family contract and docs), IEEE-754 for floats (Charter K9). **Semantics:** K9-002 ≡ K9-001 on values plus `mask_combine(kAnd)` on validities (documented composition; values are computed at all lanes including invalid ones — safe for wrapping integers; float invalid lanes may hold NaN/Inf garbage, which is defined-behavior garbage the validity bit masks). **Preconditions:** `a.len == b.len`; validities non-null in K9-002. **Aliasing delta:** `out == a.data` or `out == b.data` permitted; `out_validity == a_validity.bits` or `b_validity.bits` permitted. **Tests/benchmarks:** matrix rows K9; `bench_arith.cpp`.

```cpp
// typical: nullable add, in place; edge: wrapping by name — INT32_MAX + 1 == INT32_MIN
quiver::arith(quiver::ArithOp::kAdd, {a, n}, {b, n}, {a_valid}, {b_valid}, a, out_valid);
```

### K10 — arith_guarded (`quiver/arith.h`)

| API ID | Signature |
|---|---|
| API-K10-001 | `template<IntElement T> std::int64_t arith_checked(ArithOp op, BatchView<T> a, BatchView<T> b, T* out, std::uint8_t* overflow_bits) noexcept;` + scalar-rhs overload — → overflow count; `overflow_bits` nullable |
| API-K10-002 | `template<IntElement T> void arith_saturating(ArithOp op, BatchView<T> a, BatchView<T> b, T* out) noexcept;` + scalar-rhs overload |

**Purpose:** overflow-guarded integer arithmetic; never silent UB (Charter K10, §7.4). **Semantics (ADR-014):** `arith_checked` writes wrapped (two's-complement / modular) results at every lane, sets bit *i* of `overflow_bits` (when non-null, tail-zeroed) iff lane *i* overflowed, and returns the overflow count — 0 means clean; positions compose with K2/K3 for extraction. `arith_saturating` clamps to `numeric_limits<T>::min()/max()`. **Preconditions:** as K9; `overflow_bits` capacity `⌈n/8⌉` when non-null. **Aliasing delta:** as K9. **Tests/benchmarks:** boundary-value matrix (±max, ±1, 0, mixed-sign mul) in [12 §2](12-testing-architecture.md); `bench_arith_guarded.cpp` — documents the checked-vs-wrap tax and the 64-bit-mul scalar fallback honestly (T7).

```cpp
// typical: checked sum column with overflow positions
std::int64_t bad = quiver::arith_checked(quiver::ArithOp::kMul, {a, n}, {b, n},
                                         out, ovf_bits);
if (bad != 0) { /* extract positions: bitmap_to_selvec(ovf_bits, n, idx) */ }
```

## 6. API dependency graph

`reduce_count_valid` (K6) → `mask_popcount` (K4); K9-002 → semantics of `mask_combine` (K4). No other public API depends on another. Callers layer freely; no ordering constraints exist between families beyond data flow.

## 7. Compatibility, versioning, deprecation

- **Source compatibility:** guaranteed within a major version from v1.0 (REQ-API-009). **Binary compatibility:** not promised in v1 (static lib + inline namespace; ADR-007 reserves the mechanism). **Semantic compatibility:** kernel results are stable within a major version; qhash64 output changes are major-version events (Charter §7.4).
- Introduction versions: Tier A families + Surfaces B/C at v0.1 (scalar); Tier B families at v0.4 (full table in [19 §4](19-release-plan.md)).
- **Deprecation policy:** nothing may be deprecated before v1.0 (surfaces are still 0.x-fluid); post-1.0, deprecations require `[[deprecated]]` + one full minor version of coexistence + migration notes in release docs.

## 8. ADR-006 — Public API style

- **Status:** Accepted.
- **Context:** Charter Surface B promises non-owning view vocabulary types; Charter §7.6 anticipates a future C ABI; kernels are compiled per-ISA in TUs (ADR-003), so public templates cannot carry implementations.
- **Problem:** how callers name types and how templates meet compiled code.
- **Alternatives:** (1) `std::span`-based signatures — rejected: `size_t` vs `int64` friction, no place to hang bitmap/selvec semantics, harder future C shim; (2) rich owning/RAII types — rejected: violates T6/REQ-API-003; (3) pure C-style pointer+length everywhere — rejected: charter names view structs; (4) **first-party trivially-copyable view structs + thin constrained-template façade over concrete per-type `detail::` symbols** (selected).
- **Decision:** Alternative 4. The façade is `inline` `if constexpr` type switches (or tag-dispatched calls) to `quiver::detail::<kernel>_<type>` functions declared in `detail/extern_decls.h` and defined in kernel TUs. One concrete symbol per admissible template-parameter combination (10 element types; ×3 code types for K5 `dict_decode`; fewer where a concept narrows the set).
- **Consequences:** + per-ISA compilation works; public headers stay light; C shim later is mechanical. − a fixed extern-symbol inventory must be maintained (bounded: closed type list × closed API list).
- **Reconsideration:** C ABI work (future) may promote `detail` symbols to a stable `qv_` C surface.
- **Related:** REQ-API-004/007, ADR-003, Charter §7.1/§7.6.

## 9. ADR-007 — Inline-namespace ABI versioning

- **Status:** Accepted. **Context/Problem:** static-library symbol collisions across future major versions; charter promises SemVer.
- **Alternatives:** no namespace versioning (rejected: forecloses v2 coexistence), preprocessor-renamed symbols (rejected: ugly, non-idiomatic).
- **Decision:** `namespace quiver { inline namespace v1 { … } }`; the inline namespace increments only on ABI-epoch changes (major versions with incompatible types).
- **Consequences:** user code spells `quiver::` unchanged; mangled names carry `v1`. **Reconsideration:** C ABI milestone. **Related:** REQ-API-001/009.

## 10. Acceptance criteria

Every API above has: implemented façade + concrete symbols; unit/property/differential/fuzz coverage per [12](12-testing-architecture.md); a benchmark per [10](10-benchmark-architecture.md); a `docs/api/` page per [14](14-documentation.md); examples compiled in CI. The public-surface freeze review at M10 confirms signature-for-signature equality between this chapter and shipped headers.

## 11. Traceability

Charter §7.1–7.6 (surfaces, contracts), §6.1–6.3 (catalog, types, ISA), Appendix A (canonical semantics) → REQ-API-001..012, API-CORE-001, API-DISP-001..005, API-K1..K10 → ADR-006/007 (this chapter), ADR-012/013/014/016/023/025/026 (semantics, in [06](06-memory-model.md)/[08](08-kernel-design.md)) → tests ([12](12-testing-architecture.md)), benchmarks ([10](10-benchmark-architecture.md)), milestones M1/M3–M7 ([18](18-milestones.md)).
