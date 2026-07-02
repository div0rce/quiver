# ADR-020 — Statistics implementation

- **Identifier:** ADR-020
- **Title:** Statistics implementation
- **Status:** Accepted
- **Source:** extracted verbatim from the Engineering PRD, [docs/prd/11-performance-ledger.md](../prd/11-performance-ledger.md) (REQ-DOC-004). From M0 onward this file is the canonical, living ADR record; status changes happen here and in the [index](README.md), with the PRD text remaining the historical record.

---

- **Status:** Accepted.
- **Context:** Charter §6.4 fixes the policy surface (≥10 process repetitions, median+min, nonparametric 95% CIs, CV); the PRD owes the estimator mechanics. Survey §7.4 documents the two legitimate schools (min-is-best; distribution-aware) — the charter already committed to reporting both median and min.
- **Alternatives for the CI:** (1) parametric t-intervals — rejected: timing distributions are skewed, non-normal (Survey §7.4); (2) rank-based exact intervals — viable but awkward at R = 10 for the median; (3) **seeded percentile bootstrap** (selected): distribution-free, works for both median and min, trivially reproducible with a recorded seed, B = 10,000 is cheap offline.
- **Decision:** percentile bootstrap, B = 10,000, seed recorded per entry; CV = stddev/mean of repetitions (reported for stability screening only, never as the headline statistic).
- **Consequences:** first-party ~80-line implementation in the runner with golden tests against committed fixtures (REQ [05 §9](../prd/05-internal-architecture.md)); documented caveat: CI width at R = 10 is coarse — R may be raised per entry, recorded in `repetitions`.
- **Reconsideration:** if a methodology reviewer (paper referee, M9) requires Kalibera-Jones multi-level design, bump QLM.
- **Related:** REQ-LEDGER-004/014.
