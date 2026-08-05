# Representation study — bitmap vs selection vector

**Status: OPEN — pre-registered; two of ≥3 ISAs collected.** The full study needs
entry-referenced ledger data across ≥3 ISAs and ≥5 µarchs (Charter §6.2 / Survey §11.3 #3);
registered so far: `apple-m2-mba` (NEON, indicative slice) and `intel-i9-9900k` (AVX2, committed
`compare` entries, run 20260805-f7b016f85d08). The cross-µarch analysis and conclusion remain
**PENDING hardware** (R-06 / REQ-LEDGER-012). No numbers here are invented (Charter T2).

This directory is the M9 charter deliverable — the bitmap-vs-selvec representation question, its
pre-registered method, the data collected so far, and the analysis/conclusion (open):

| File | Contents |
|------|----------|
| [question.md](question.md) | The research question and hypotheses |
| [method.md](method.md) | Pre-registered protocol + decision rule (run before looking at cross-µarch data) |
| [entries.md](entries.md) | Data: the apple-m2 (NEON) indicative slice and the committed intel-coffee-lake (AVX2) entries (run 20260805-f7b016f85d08); remaining µarchs toward ≥3 ISAs / ≥5 µarchs pending |
| [analysis.md](analysis.md) | Analysis of the slice; what the full study still requires |
| [conclusion.md](conclusion.md) | OPEN — the ≥3-ISA/≥5-µarch bar is unmet (2 ISAs / 2 µarchs registered) |

Gate: [releases/gates/M9.md](../../../releases/gates/M9.md).
