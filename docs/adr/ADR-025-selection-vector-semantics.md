# ADR-025 — Selection-vector semantics

- **Identifier:** ADR-025
- **Title:** Selection-vector semantics
- **Status:** Accepted
- **Source:** extracted verbatim from the Engineering PRD, [docs/prd/08-kernel-design.md](../prd/08-kernel-design.md) (REQ-DOC-004). From M0 onward this file is the canonical, living ADR record; status changes happen here and in the [index](README.md), with the PRD text remaining the historical record.

---

*Status:* Accepted. *Context:* Charter §6.2 fixes sorted-unique for selection and arbitrary for `take`; the engineering question is enforcement. *Alternatives:* (1) validate eagerly in release — rejected: O(n) tax on every call contradicts the performance identity of the library; (2) tolerate unsorted input in selection kernels ("works by accident") — rejected: forecloses forward-scan algorithms and in-place filter safety; (3) **contract + debug assertion** (selected): sortedness/in-range checked fully under `QUIVER_ENABLE_ASSERTS` (O(n) scan acceptable in debug), UB in release, fuzz harnesses generate only contract-valid selections while separately fuzzing K8's untrusted-input surface. *Consequences:* documented loudly on every SelVec API; `inv_selvec_sorted` validates producers, assert tests validate consumers. *Related:* REQ-MEM-007, REQ-K5-001/002.
