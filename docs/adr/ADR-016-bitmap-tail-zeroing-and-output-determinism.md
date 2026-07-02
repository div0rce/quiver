# ADR-016 — Bitmap tail zeroing and output determinism

- **Identifier:** ADR-016
- **Title:** Bitmap tail zeroing and output determinism
- **Status:** Accepted
- **Source:** extracted verbatim from the Engineering PRD, [docs/prd/06-memory-model.md](../prd/06-memory-model.md) (REQ-DOC-004). From M0 onward this file is the canonical, living ADR record; status changes happen here and in the [index](README.md), with the PRD text remaining the historical record.

---

**ADR-016 — Bitmap tail zeroing and output determinism.** *Status:* Accepted. *Context:* differential testing and cross-backend determinism depend on byte-comparable bitmap outputs; the Arrow spec leaves tail bits unspecified, so a choice is required. *Constraints:* single-pass zero-allocation kernels; Arrow compatibility (REQ-MEM-006). *Problem:* leave tail bits unspecified (cheaper by one mask op) or force them to zero. *Alternatives:* (a) unspecified tails — rejected: breaks byte-comparability, leaks nondeterminism across backends, makes `memcmp`-based differential testing impossible; (b) zero tails (selected). *Decision:* producers zero tails; consumers ignore input tails (robustness against foreign bitmaps). *Consequences:* one extra mask per batch tail (negligible); deterministic outputs (REQ-API-006). *Reconsideration:* none foreseen. *Related:* REQ-MEM-006/008, REQ-TEST differential oracle.
