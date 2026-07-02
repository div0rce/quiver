# 06 — Memory Model

## 1. Purpose

Normative memory semantics for the entire public surface: ownership, bounds, alignment, aliasing, representation layouts, and capacity contracts. This chapter is the single source of truth the kernel specs ([08](08-kernel-design.md)) and API specs ([04](04-public-api.md)) reference. Upstream authority: Charter §7.2 (memory contract), §6.2 (representations), T6 (data-shape honest).

## 2. Requirements

| ID | Requirement |
|---|---|
| REQ-MEM-001 | **Bounds.** No kernel shall read or write any byte outside the documented input ranges and documented output ranges for its arguments. The default contract is exactly `[ptr, ptr + documented_size)`; there are no implicit padding assumptions (Charter §7.2). Access is defined at the architectural observable level: an AVX-512 masked operation whose masked-off lanes fall outside the range does not access those bytes (Intel SDM guarantee) and is within contract (ADR-015). |
| REQ-MEM-002 | **No `_padded` variants in v1.** The `*_padded` naming convention is reserved (Charter §7.2); no such variant shall ship in v1.0. Introducing one requires ledger evidence + PRD amendment ([21-future-work.md](21-future-work.md)). |
| REQ-MEM-003 | **No allocation.** No kernel or Surface B/C function shall allocate. The shipped library shall contain no calls to `new`, `malloc`, or allocating std facilities (verified by `inv_noalloc.cpp` link-seam test + symbol scan in CI). |
| REQ-MEM-004 | **Alignment.** Every kernel shall produce correct results for any element-aligned input/output (natural alignment of `T`; no wider requirement). Byte buffers (bitmaps, packed data) require 1-byte alignment only. Internal wide loads/stores shall use unaligned forms or `memcpy` ([15 §3](15-security-and-ub.md)). The aligned-vs-misaligned performance delta is a ledger axis, never a correctness concern (Charter §7.2). |
| REQ-MEM-005 | **Aliasing.** Output ranges shall not partially overlap any input range. Exact aliasing is permitted only per the ADR-023 matrix (§7). Everything else is a contract violation (debug-asserted where cheaply detectable: pointer-equality cases; full overlap detection is not required). |
| REQ-MEM-006 | **Bitmap representation.** Bit *i* of a bitmap = byte `i/8`, bit `i%8` (LSB-first); 1 = valid/selected. Storage size for *n* bits = `⌈n/8⌉` bytes. Producers shall zero all tail bits in the final byte beyond *n* (ADR-016); consumers shall ignore input tail bits. Arrow-compatible by construction (Charter §6.2; Survey §2.6). |
| REQ-MEM-007 | **Selection-vector representation.** `uint32_t` indices; selection semantics = strictly increasing, in-range (ADR-025); `take` semantics = arbitrary order/duplicates, in-range. |
| REQ-MEM-008 | **Output completeness and write regions.** Every output parameter has a *capacity region* (the §6 "required capacity" column) and a *defined output region*. A kernel shall fully write its defined output region and shall never touch any byte beyond the capacity region. For fixed-size outputs the two regions coincide (bitmap outputs: all `⌈n/8⌉` bytes; fixed-count value outputs: exactly the documented element count). For count-returning compaction outputs (K1 selvec forms, K2 `filter`, K3 `bitmap_to_selvec`), the capacity region is `n` elements, the defined output region is the first *count* elements, and positions `[count, n)` may receive scratch stores with unspecified values — a deliberate allowance for unconditional-store and full-vector compaction (the [08 §5](08-kernel-design.md)/[09 §6](09-simd-architecture.md) algorithms), whose writes provably stay within `[0, n)` because the output cursor never exceeds the processed-input count. Determinism (REQ-API-006) and differential comparison apply to defined output regions only. |
| REQ-MEM-009 | **Lifetime.** No API retains any pointer after return (REQ-API-003). The library holds no pointers to caller memory in any global state. |
| REQ-MEM-010 | **Batch cap.** `kMaxBatchLen = 2³¹−1` bounds every `n`, every `sel.len`, and every `indices.len` (REQ-API-005). |

## 3. Ownership model

```text
Title: Ownership diagram — all arrows are borrows, never transfers
Purpose: REQ-MEM-009 / REQ-API-003 visualization

  Caller heap/stack/arena (caller-owned, caller-aligned, caller-lifetime)
     │ BatchView<T>{data,len}   BitmapView{bits}   SelVec{idx,len}   T*/u8*/u32* out
     ▼                                                        ▲
  quiver kernel  ── reads inputs, writes outputs ─────────────┘
     (no allocation, no retention, no global buffers)
```

Quiver never allocates, frees, resizes, or retains. Buffer sizing, pooling, NUMA placement, and huge-page policy are entirely the caller's domain (Charter §8.2: allocators/buffer managers are permanent non-goals; Survey §5.2).

## 4. Alignment policy (engineering rationale)

Correctness at element alignment is mandatory (REQ-MEM-004) because adopters slice batches at arbitrary offsets (selection-driven sub-ranges). Performance guidance published with the ledger: 64-byte alignment recommended on x86, 128-byte on Apple Silicon (Survey §3.1/§3.3); the ledger's alignment axis quantifies the actual penalty per (kernel, ISA, µarch) instead of asserting folklore (Charter T2).

## 5. Representation details

**Bitmaps (REQ-MEM-006).** Word-level access reads the bitmap as bytes; implementations may assemble 64-bit words via `memcpy` for full words and per-byte tail handling ([09 §5](09-simd-architecture.md)). The tail-zeroing rule makes bitmap outputs byte-comparable (`memcmp`) across ISAs and runs — this is what enables the differential test oracle ([12 §2](12-testing-architecture.md), REQ-TEST-003) and is therefore an invariant, not a nicety.

**Selection vectors (REQ-MEM-007).** Strictly increasing implies unique. Kernels producing selection vectors guarantee sortedness by construction (forward scan); this is invariant-tested (`inv_selvec_sorted.cpp`).

**ADR-016 — Bitmap tail zeroing and output determinism.** *Status:* Accepted. *Context:* differential testing and cross-backend determinism depend on byte-comparable bitmap outputs; the Arrow spec leaves tail bits unspecified, so a choice is required. *Constraints:* single-pass zero-allocation kernels; Arrow compatibility (REQ-MEM-006). *Problem:* leave tail bits unspecified (cheaper by one mask op) or force them to zero. *Alternatives:* (a) unspecified tails — rejected: breaks byte-comparability, leaks nondeterminism across backends, makes `memcmp`-based differential testing impossible; (b) zero tails (selected). *Decision:* producers zero tails; consumers ignore input tails (robustness against foreign bitmaps). *Consequences:* one extra mask per batch tail (negligible); deterministic outputs (REQ-API-006). *Reconsideration:* none foreseen. *Related:* REQ-MEM-006/008, REQ-TEST differential oracle.

## 6. Capacity contracts (canonical table)

Columns per REQ-MEM-008: the **required capacity** is what the caller must provide (the capacity region); the **defined output** is what the kernel guarantees to have written meaningfully. Where they differ, positions between defined output and capacity may hold scratch (compaction rows, marked ★).

| API | Output | Required capacity (capacity region) | Defined output |
|---|---|---|---|
| K1 `compare_bitmap*` | `out_bits` | `⌈n/8⌉` bytes | all `⌈n/8⌉` bytes |
| K1 `compare_*_selvec` ★ | `out_idx` | `n` indices | first *count* indices |
| K2 `filter` (bitmap) ★ | `out` | `n` elements | first *count* = popcount elements |
| K2 `filter` (selvec) | `out` | `sel.len` elements | all `sel.len` elements |
| K3 `bitmap_to_selvec` ★ | `out_idx` | `n` indices | first *count* = popcount indices |
| K3 `selvec_to_bitmap` | `out_bits` | `⌈n/8⌉` bytes | all `⌈n/8⌉` bytes |
| K4 combine/not | `out_bits` | `⌈n/8⌉` bytes | all `⌈n/8⌉` bytes |
| K5 `take` | `out` | `indices.len` elements | all `indices.len` elements |
| K5 `dict_decode` | `out` | `n` elements (`sel.len` fused) | all of it |
| K6 | scalar/struct returns | — | — |
| K7 `hash64*` | `out` | `n` × 8 bytes | all `n` hashes |
| K8 `unpack*` | `out` | `n` elements | all `n` elements |
| K9/K10 | `out` | `n` elements | all `n` elements |
| K10 `arith_checked` | `overflow_bits` (nullable) | `⌈n/8⌉` bytes | all `⌈n/8⌉` bytes |

Callers guarantee the capacity region; kernels fully write the defined output, return counts where data-dependent, and never touch a byte beyond capacity (REQ-MEM-008, REQ-API-011). Guard-page tests place the protected page at the **capacity-region** boundary (REQ-TEST-006).

## 7. ADR-023 — Aliasing contract

- **Status:** Accepted.
- **Context:** engines compact and transform in place; blanket no-aliasing wastes real use cases, blanket allowance forbids `restrict`-quality codegen.
- **Problem:** which overlap is legal, per kernel.
- **Alternatives:** (a) prohibit all aliasing — rejected: forces copies for in-place filter, a top real-world pattern; (b) allow arbitrary overlap — rejected: unimplementable efficiently, unverifiable; (c) **exact-alias allowlist** (selected): partial overlap always prohibited; `out == input` pointer-equality permitted only where a forward-pass safety invariant holds.
- **Decision — the allowlist:**

| Kernel | Permitted exact aliasing | Safety invariant |
|---|---|---|
| K2 `filter` | `out == in.data` | write index ≤ read index in a forward scan |
| K4 combine/not | `out_bits == a.bits` (or `b.bits`) | pure elementwise on words |
| K7 `hash64_combine` | `out == a` or `out == b` | elementwise |
| K9/K10 value outputs | `out == a.data` or `out == b.data`; `out_validity ==` either validity | elementwise |
| all others | none | gather/scatter/conversion patterns lack a safe order |

- **Consequences:** internal implementations may apply `QUIVER_RESTRICT` only where aliasing is prohibited; the allowlist rows are covered by dedicated unit tests (in-place result == out-of-place result).
- **Reconsideration:** adding a row requires demonstrating the invariant and a test, via PRD amendment.
- **Related:** REQ-MEM-005, REQ-API-012.

## 8. Failure modes

Partial overlap, capacity shortfall, out-of-range indices, oversized `n`: contract violations — debug assert where detectable at O(1) (pointer equality, `n` range, null checks), otherwise UB with the [15](15-security-and-ub.md) hardening posture. For contract-satisfying inputs, memory safety is guaranteed (REQ-SEC-001) — this is the sanitizer-clean pledge that makes Quiver vendorable (Charter §7.2).

## 9. Acceptance criteria

ASan/UBSan/MSan-clean across the full differential matrix and fuzz corpus; `inv_noalloc`, `inv_bitmap_tail`, `inv_selvec_sorted`, aliasing-allowlist tests pass; capacity table verified against every API doc page at M10 review.

## 10. Traceability

Charter §7.2/§6.2/T6 → REQ-MEM-001..010 → ADR-016/023 (here), ADR-015/025/026 ([09](09-simd-architecture.md)/[08](08-kernel-design.md)) → tests ([12](12-testing-architecture.md)) → milestones M3+ (enforced from the first kernel onward). Survey authority: §2.3 (padding pattern deliberately rejected), §2.6 (Arrow bitmap), §3.1/§3.3 (alignment guidance).
