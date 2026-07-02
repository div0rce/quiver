# ADR-014 — Overflow reporting design

- **Identifier:** ADR-014
- **Title:** Overflow reporting design
- **Status:** Accepted
- **Source:** extracted verbatim from the Engineering PRD, [docs/prd/08-kernel-design.md](../prd/08-kernel-design.md) (REQ-DOC-004). From M0 onward this file is the canonical, living ADR record; status changes happen here and in the [index](README.md), with the PRD text remaining the historical record.

---

- **ADR-014 — Overflow reporting design.** *Status:* Accepted. *Context:* Charter Appendix A defers granularity (flag vs first-index vs mask) inside the no-silent-UB contract. *Alternatives:* (1) boolean flag only — rejected: callers needing positions would rerun scalar; (2) first overflow index — rejected: forces early-exit data-dependent branch into the hot loop (violates REQ-KERNEL-003 spirit) and loses total count; (3) **count return + optional position bitmap** (selected): count accumulates branchlessly; the nullable bitmap writes positions only when the caller wants them; positions compose with K2/K3 for extraction — reuses the library's own vocabulary. *Consequences:* two variants of the inner loop (bitmap/no-bitmap) per backend — bounded cost; `overflow_bits` tail-zeroed per ADR-016. *Reconsideration:* none before v2. *Related:* REQ-K10-001/002, API-K10-001.
