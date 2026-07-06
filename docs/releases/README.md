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
  OK -->|no| HOLD["Hold: no artifacts published<br/>(this is where v0.6.0 sits today,<br/>one nightly job is broken)"]
  PUB --> MAN["A maintainer reviews<br/>and publishes by hand"]
```

Release notes follow a fixed template; gate records under `gates/` are the auditable trail of every
release checklist. Start of the trail: [gates/M0.md](gates/M0.md). Owner: maintainer.

## Traceability

Release-note template: REQ-DOC-010. Gate records: REQ-MS-002. The tag-driven flow above is the
`release.yml` workflow (REQ-CI-009); the "hold" branch is why v0.6.0 has no attached files yet
(risk R-19).
