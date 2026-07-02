# ADR-012 — qhash64 v1 algorithm

- **Identifier:** ADR-012
- **Title:** qhash64 v1 algorithm
- **Status:** Accepted
- **Source:** extracted verbatim from the Engineering PRD, [docs/prd/08-kernel-design.md](../prd/08-kernel-design.md) (REQ-DOC-004). From M0 onward this file is the canonical, living ADR record; status changes happen here and in the [index](README.md), with the PRD text remaining the historical record.

---

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

  *Quality gate:* first-party avalanche suite ([12 §2](../prd/12-testing-architecture.md), REQ-TEST-016): over ≥100k seeded samples per type, every input-bit flip shall flip each output bit with probability within 0.5 ± 0.02, and no output bit may show overall bias > 0.02 — a documented SMHasher-subset, with the limitation stated (full SMHasher run is a dev-time activity recorded in the family doc, not a CI dependency). *Consequences:* test vectors frozen into `tests/golden/qhash64_vectors.txt` at M6; any change to constants/rounds is a major-version event. *Reconsideration:* v1.x string hashing may introduce a companion algorithm; qhash64 itself only changes at v2. *Related:* REQ-K7-001..004, Charter §7.4.
