# ADR-026 — Bit-packing layout

- **Identifier:** ADR-026
- **Title:** Bit-packing layout
- **Status:** Accepted
- **Source:** extracted verbatim from the Engineering PRD, [docs/prd/08-kernel-design.md](../prd/08-kernel-design.md) (REQ-DOC-004). From M0 onward this file is the canonical, living ADR record; status changes happen here and in the [index](README.md), with the PRD text remaining the historical record.

---

- **ADR-026 — Bit-packing layout.** *Status:* Accepted. *Problem:* fix the packed layout. *Alternatives:* MSB-first (rejected: no ecosystem pull), 32-value lane-interleaved FastLanes layout (rejected for v1: ties Quiver to an evolving format — Charter §8.1 defers compressed-format coupling), **LSB-first little-endian contiguous** (selected): value *i* occupies bits `[i·w, (i+1)·w)`; bit *j* lives at byte `j/8`, bit `j%8`. Parquet-RLE-bit-packing-compatible, trivially specifiable, byte-order-independent definition. *Reconsideration:* v2 compressed-kernel expansion may add interleaved layouts as new APIs. *Related:* REQ-K8-001.
