# 15 — Security and Undefined Behavior

## 1. Purpose

The UB-avoidance catalog, the untrusted-input posture, and supply-chain discipline. The sanitizer-clean, no-UB-in-contract pledge is a *product* property (Charter §7.2/§7.3: adopters run sanitizer CI; a library that trips it is unvendorable), so this chapter is normative for every line of shipped code.

## 2. Threat model

Quiver computes over caller-provided memory inside the caller's process. It parses no files, opens no sockets, reads one environment variable (REQ-DISP-005), and spawns nothing. The security surface is therefore: (a) memory safety of kernels on contract-satisfying inputs — guaranteed; (b) behavior on contract-violating inputs — UB by contract, bounded by the hardening rules below; (c) K8 `unpack`, whose packed-bytes input is realistically **untrusted** (file-format-adjacent) and is held to a stricter robustness standard; (d) supply chain of the repository itself. qhash64 is documented non-cryptographic (Charter §7.4) — no security property is claimed for it, including hash-flooding resistance (engines choose seeds; documented).

## 3. Requirements

| ID | Requirement |
|---|---|
| REQ-SEC-001 | For inputs satisfying documented preconditions, no execution path shall exhibit undefined behavior or access memory outside documented ranges — verified by the full differential matrix, guard-page suite, and fuzz corpus under ASan+UBSan+MSan (REQ-TEST-003/006/007/009). |
| REQ-SEC-002 | **UB catalog (prohibited constructs / required patterns):** signed-overflow arithmetic (use unsigned internals or overflow builtins); type punning via unions or pointer casts (use `std::bit_cast`/`memcpy`); misaligned dereference (use `memcpy` or unaligned intrinsics — REQ-SIMD-007); shifts ≥ bit-width (mask or branch shift counts; K8's variable shifts shall clamp by construction); out-of-bounds pointer *formation* (compute end pointers only as `ptr + valid_size`); uninitialized reads (REQ-MEM-008 output completeness); `reinterpret_cast` between unrelated types (allowlist: byte-pointer views). clang-tidy + UBSan enforce; the catalog lives in `docs/internals/ub-catalog.md` with per-rule rationale. |
| REQ-SEC-003 | Debug assertions never substitute for the REQ-SEC-001 guarantee: asserts check *contracts*; in-contract safety holds with asserts off (release). |
| REQ-SEC-004 | K8 shall treat `packed` bytes as untrusted: any byte content is memory-safe given a correct byte-count precondition; reads bounded by `⌈n·w/8⌉` exactly; K8 receives the largest fuzz budget (REQ-TEST-007) and raw-bytes (not just structured) fuzzing. |
| REQ-SEC-005 | The shipped library shall contain no calls that could fail at runtime for environmental reasons (no allocation REQ-MEM-003, no I/O, no clock reads); the only OS interaction is CPU detection + one `getenv` at first dispatch resolution ([07](07-runtime-dispatch.md)). |
| REQ-SEC-006 | Supply chain: zero runtime dependencies (Charter T4); dev dependencies pinned by version + SHA-256 (REQ-BUILD-007); GitHub Actions pinned by commit SHA (REQ-CI-001); release artifacts published with `SHA256SUMS` and GitHub artifact attestations (REQ-CI-009); Dependabot enabled for Actions pins only. |
| REQ-SEC-007 | A `SECURITY.md` (M0) shall define the vulnerability-report channel (GitHub private advisories) and the response expectation (solo-maintainer honest SLA: acknowledgment ≤ 7 days). |
| REQ-SEC-008 | No secrets exist in the repository or CI beyond GitHub-provided tokens; workflows use minimal permissions blocks (`contents: read` default; release job elevated explicitly). |

## 4. Enforcement map

| Rule class | Static (clang-tidy/-W) | Dynamic (san/fuzz) | Structural |
|---|---|---|---|
| Signed overflow | `-Wconversion`, tidy checks | UBSan | unsigned-internal idiom ([17 §5](17-coding-standards.md)) |
| Punning/alignment | tidy `reinterpret_cast` audit | UBSan alignment, ASan | REQ-SIMD-007, `bit_cast` idiom |
| Bounds | — | ASan + guard pages (REQ-TEST-006) | no-over-read tail policy (ADR-015) |
| Uninitialized | — | MSan (nightly) | REQ-MEM-008 |
| Untrusted K8 | — | raw-byte fuzzing (REQ-SEC-004) | exact byte-bound reads |

## 5. Failure modes / acceptance / traceability

A sanitizer finding anywhere in shipped code is a release blocker regardless of "it works in practice." **Acceptance:** REQ-SEC-001 evidence chain green at every gate; UB catalog published; `SECURITY.md` live; attestations verified on the v0.3 release dry run. **Traceability:** Charter §7.2/§7.3/§7.4/T4 → REQ-SEC-001..008 → [06](06-memory-model.md)/[12](12-testing-architecture.md)/[13](13-ci-architecture.md)/[17](17-coding-standards.md) → all release gates. Survey authority: §7.5 (correctness-before-numbers), §4.4 (compiler-behavior realism).
