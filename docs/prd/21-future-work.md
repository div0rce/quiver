# 21 — Future Work

## 1. Purpose

Deferred items with revisit conditions. Everything here is **out of scope for v1** and shall not be implemented, partially implemented, or "prepared for" beyond what a listed reservation states. Items marked **[charter]** originate in Charter §8.1 and their scope status can only change by charter amendment; items marked **[prd]** are engineering deferrals this PRD introduces and can be activated by PRD amendment. This chapter exists so absence is legible as decision, not omission (master prompt Part 3).

| # | Item | Origin | Revisit condition | Reserved hooks in v1 |
|---|---|---|---|---|
| F-01 | Compressed-predicate kernels (predicates over FOR/RLE/dictionary data) | [charter] §8.1, OA C9 | FastLanes/format stabilization; v2 planning | K8's fused-predicate variant is the named doorstep (Charter §8.1); none in code |
| F-02 | String hashing (ptr,len views) in K7 | [charter] §8.1 | post-v1.0, demand-driven | ADR-012 names it a companion algorithm, not a qhash64 change |
| F-03 | Fused unpack+predicate (K8 v1.x) | [charter] §6.1 | post-v1.0 | API naming space reserved in [04 K8](04-public-api.md) family |
| F-04 | `*_padded` kernel variants | [charter] §7.2 / REQ-MEM-002 | ledger evidence that scalar tails dominate a hot family; PRD amendment | naming convention reserved; ADR-015 reconsideration clause |
| F-05 | SVE2 backends | [charter] §6.3 | server-ARM VL economics stabilize (Survey §4.1) | REQ-SIMD-010 keeps it out of `main`; exploratory branch permitted |
| F-06 | C ABI shim (`qv_` surface) | [charter] §7.6 | post-v1.0, demand-driven | ADR-006's concrete-symbol core makes it mechanical; ADR-017 notes error-code latitude at that boundary |
| F-07 | Rust/Python bindings | [charter] §7.6 | community-driven, never core-maintained | none |
| F-08 | GPU anything | [charter] §8.1 | v2+ at most | none |
| F-09 | Windows/MSVC tier-1 promotion | [charter] §8.1 | demonstrated demand + green tier-2 history | tier-2 CI leg maintained |
| F-10 | Shared-library builds + symbol-visibility macros | [prd] REQ-BUILD-002 | adopter demand; ABI policy decision | `QUIVER_API` macro reserved empty |
| F-11 | Custom assert handler hook | [prd] REQ-ERR-005 | embedder demand | none (fixed handler in v1) |
| F-12 | f64-accumulating f32 sum variant | [prd] [08 K6](08-kernel-design.md) | numerical-accuracy demand from adopters | none |
| F-13 | macOS kperf PMU integration | [prd] ADR-022 | private-API risk accepted knowingly (OA C12 red team) | ledger schema already tolerates per-platform metric absence |
| F-14 | OSS-Fuzz onboarding | [prd] [12](12-testing-architecture.md) | post-launch (needs public repo + stability) | libFuzzer targets are OSS-Fuzz-shaped already |
| F-15 | PGO/BOLT exploration for kernel TUs | [prd] | ledger evidence of front-end stalls; methodology implications reviewed first (QLM) | none |
| F-16 | git-lfs for ledger raw data | [prd] ADR-021 | repo > ~200 MB | none |
| F-17 | vcpkg/Conan registry submissions | [prd] REQ-BUILD-015 | v0.6 shipped; charter §9.1 18-month item | packaging skeletons from M8 |
| F-18 | Data-dependent adaptive backend choice (micro-adaptivity) | [prd] | v2 research direction (Survey §2.10, §9 #12); would need charter-level scope review | REQ-KERNEL-007 keeps v1 selection static + evidence-gated |

## 2. Traceability

Charter §8.1/§8.2 (deferred/permanent split is charter-owned; nothing from §8.2's permanent-never table appears here by construction) → this list → activation paths as marked. The Engine Test (Charter T1) applies to every future item.
