# API reference

One page per operation group, describing exactly what each function promises:
what it computes, what the inputs and outputs mean, what is guaranteed to be identical across CPUs,
and where the measured numbers live. Start here when you are about to call something and want the
precise contract. If you just want a feel for the library, the
[getting-started guide](../guides/getting-started.md) is friendlier.

Pages: [core types](core.md) and [dispatch](dispatch.md), then one per operation, `compare`,
`filter`, `select`, `mask`, `take`, `reduce`, `hash`, `unpack`, `arith`.

## Traceability

Each page follows the reference template in [PRD 14 §5](../prd/14-documentation.md) (contract,
scalar reference, per-CPU notes, measured-verdict block, examples, traceability footer) and is owned
by its module (MOD-CORE, MOD-DISPATCH, MOD-K1 through MOD-K10). Pages ship in the same change as the
code they describe (REQ-DOC-002).
