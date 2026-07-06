# Disputing a ledger number (REQ-DOC-008, Charter §12)

Every published Quiver performance number is reproducible from artifacts in this repository.
If a number looks wrong on comparable hardware, this page is the protocol, disputes are
answerable from artifacts alone, and losses get the same prominence as wins (Charter T7).

## 1. Reproduce the entry

Each ledger entry records its `benchmark` name, `axes`, `machine_id`, and `manifest_ref`.
The reproduction command for any entry class is:

```sh
cmake --preset bench && cmake --build --preset bench -j
python3 ledger/runner/quiver_ledger.py run \
    --machine <your-machine-id> \
    --filter '<the entry's benchmark name, regex-escaped>'
```

You will need a machine file under `ledger/machines/` describing your hardware (copy an
existing one, the runner refuses unregistered machines by design, REQ-LEDGER-007). The
runner enforces the REQ-LEDGER-013 environment checklist (clean tree, performance governor
where the platform has one) and writes `entries.json` + `manifest.json` + per-repetition
raw output you can compare directly against the committed run.

## 2. What counts as a discrepancy

Compare **medians for the same (benchmark, axes, variant)** on comparable hardware. A
dispute is actionable when the medians differ beyond both entries' 95% CIs and the
manifests do not explain it (different compiler, flags, governor, SMT, or memory
configuration are explanations, not bugs). Single runs are not comparable, the ledger
never publishes them (REQ-LEDGER-004), and neither should a dispute.

## 3. File it

Open an issue with the **benchmark-dispute template**
(`.github/ISSUE_TEMPLATE/benchmark-dispute.yml`). It requires:

- your full `manifest.json` (the environment facts are usually the answer),
- the runner command you executed and its `entries.json` output,
- the `entry_id`(s) you are disputing.

## 4. What happens next

Confirmed discrepancies produce a **superseding entry** with a `notes` explanation.
Published results are append-only and history is never rewritten (REQ-LEDGER-010). If the
explanation is a code regression, it becomes a release blocker per the regression policy
([methodology §6](../benchmarks/methodology.md)).

---
*Traceability: REQ-LEDGER-004/-007/-009/-010/-013, REQ-DOC-008; Charter §12, T2/T7.*
