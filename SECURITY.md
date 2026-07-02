# Security Policy

## Reporting a vulnerability

Please report suspected vulnerabilities privately via **GitHub private security advisories** (Security → "Report a vulnerability" on this repository). Do not open public issues for suspected vulnerabilities.

Response expectation (solo-maintainer, stated honestly per REQ-SEC-007): acknowledgment within **7 days**; assessment and remediation plan communicated in the advisory thread.

## Scope

Quiver computes over caller-provided memory inside the caller's process. It parses no files, opens no sockets, reads one environment variable, and spawns nothing. The security-relevant surface is documented in the threat model: [docs/prd/15-security-and-ub.md](docs/prd/15-security-and-ub.md). Notes:

- For inputs satisfying documented preconditions, memory safety is guaranteed and sanitizer-verified (REQ-SEC-001). Contract-violating inputs are undefined behavior by contract; hardening rules bound the blast radius.
- `qhash64` is **non-cryptographic**; no security property is claimed for it, including hash-flooding resistance.
- Supply chain: the shipped library has zero runtime dependencies; development dependencies and CI actions are hash-/SHA-pinned; release artifacts ship with `SHA256SUMS` and artifact attestations (REQ-SEC-006).

## Supported versions

Pre-1.0: only the latest tagged release receives fixes.
