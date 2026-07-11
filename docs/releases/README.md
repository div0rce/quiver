# Releases and gates

This is the paper trail. Each release has notes, and each step of the project has
a "gate" record: a short, honest report of what was done, what passed, and what was deliberately
left for later. If you want to know exactly how far along Quiver is and why, the
[status report](final-implementation-report.md) is the best single page; the gate records are the
detail behind it.

What happens when a version is tagged:

```mermaid
flowchart TD
  T["Tag vX.Y.Z"] --> CI["Run the full test gate<br/>+ the nightly suite"]
  CI --> OK{"All green?"}
  OK -->|yes| PUB["Build the single-file drop-in<br/>+ checksums + provenance,<br/>open a draft release"]
  OK -->|no| HOLD["Hold: no artifacts published<br/>(where v0.6.0 sits permanently:<br/>its tag predates the R-19 fix)"]
  PUB --> MAN["A maintainer reviews<br/>and publishes by hand"]
```

The publish path is live-verified as of v0.7.0: its tag ran the nightly green end to end (the MSan
leg fixed under R-19) and `release.yml` attached the drop-in pair, checksums, and provenance.

Release notes follow a fixed template; gate records under `gates/` are the auditable trail of every
release checklist. Start of the trail: [gates/M0.md](gates/M0.md). Owner: maintainer.

## Traceability

Release-note template: REQ-DOC-010. Gate records: REQ-MS-002. The tag-driven flow above is the
`release.yml` workflow (REQ-CI-009); the "hold" branch is why v0.6.0 has no attached files
(risk R-19, resolved at v0.7.0; the v0.6.0 hold itself is permanent, its tag predates the fix).
