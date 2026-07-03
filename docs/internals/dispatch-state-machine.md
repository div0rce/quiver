# Dispatch state machine and memory-ordering argument

The normative protocol of PRD [07 §5–§6](../prd/07-runtime-dispatch.md) / REQ-DISP-008, as implemented in `src/dispatch/dispatch_internal.h` and `dispatch_tables.cpp`, with the full soundness argument reviewers should check against (this protocol is TSan-validated by `Dispatch.ConcurrentResolutionIsRaceBenign`).

## States

Per `DispatchEntry`: `Unresolved (fn_epoch == 0)` → `Resolved(epoch E)` → *(policy epoch bump)* → effectively `Stale` → re-`Resolved(epoch E′)`. Policy epochs start at 1 and only increase (`set_isa_override`/`clear_isa_override` bump). Global: `FeatureMask ∈ {Undetected, Detected(mask)}` (monotone, bit 7 = detected flag — an all-false-features CPU is representable), `Override ∈ {None, Cap(isa)}`, `EnvCap` (written once, together with detection).

## The protocol

**Hot path** (`dispatch_get`):

1. `now = policy_epoch.load(relaxed)`
2. `e = entry.fn_epoch.load(acquire)`; if `e == now` →
3. `fn = entry.fn.load(relaxed)`; call `fn`.

**Slow path** (`resolve`): compute policy cap (forces detection + env read on first use), select the highest non-null backend ≤ cap (scalar row is never null), then publish:

4. `entry.fn.store(fn, relaxed)`
5. `entry.fn_epoch.store(now, release)`

## Why step 3 may be relaxed

The acquire at step 2 synchronizes-with the release at step 5. Any read that observes epoch `E` at step 2 therefore also observes every store sequenced before step 5 in the resolver that published `E` — including step 4's `fn` store. A matched epoch can never yield a stale or null `fn`, on ARM's weak memory model as well as x86-TSO.

## Interleaved resolvers (why last-write-wins is safe)

Two resolvers R1 (observed epoch 1) and R2 (observed epoch 2) may interleave stores as `fn₁, fn₂, epoch₂, epoch₁`, leaving `{fn = fn₂, fn_epoch = 1}`. A reader holding `now = 2` mismatches and re-resolves — fine. A reader holding a stale `now = 1` (it loaded the policy epoch before the bump) matches and calls `fn₂`, a backend selected under the *newer* policy. That is a correct execution: calls concurrent with an epoch bump may run under either policy (REQ-DISP-009), and every published `fn` is a valid backend from the row. No interleaving can produce a null or out-of-row pointer after an epoch match, because every `fn_epoch` store is preceded (release-ordered) by a store of a non-null row backend.

## Stale policy-epoch reads

Step 1 is relaxed, so `now` may lag an in-flight bump. Consequence: the entry resolves/matches against the older epoch and executes the older (or newer, per above) policy's backend for a few calls — explicitly permitted (REQ-DISP-009). Overrides are benchmarking/diagnostic controls, not synchronization points.

## Environment read-once

`QUIVER_ISA` is read inside the same once-block as feature detection: the env-cap slot is stored before the detected flag is released, so any thread observing the flag (acquire) also observes the cached env value. Duplicate concurrent detection is benign — the function is pure and every publisher writes identical values (REQ-INT-001).
