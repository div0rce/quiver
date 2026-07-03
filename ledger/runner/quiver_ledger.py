#!/usr/bin/env python3
"""Quiver ledger runner (MOD-LEDGER; PRD 11; ADR-020/021). Python >= 3.11, stdlib only
(REQ-INT-004).

Commands:
  run       — execute a ledger run on a REGISTERED machine (REQ-LEDGER-007):
              environment checklist -> R fresh-process repetitions per benchmark, order
              shuffled per repetition with a recorded seed (REQ-LEDGER-006) -> percentile-
              bootstrap aggregation (ADR-020) -> append-only results directory
              (REQ-LEDGER-010): entries.json + raw/**.json + manifest.json.
  validate  — structurally validate committed results against QLS-1 (REQ-LEDGER-001);
              exits non-zero on any violation. Used by tests and reviews.

Reproduction (REQ-LEDGER-009): any entry class regenerates via
  python3 ledger/runner/quiver_ledger.py run --machine <id> --filter <pattern>
where <pattern> matches the entry's `benchmark` name.
"""

from __future__ import annotations

import argparse
import datetime
import json
import pathlib
import random
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

from qledger import environment, gbench, schema_check, stats  # noqa: E402

REPO = pathlib.Path(__file__).resolve().parents[2]
DEFAULT_REPS = 10
DEFAULT_MIN_TIME = "0.1s"


def parse_bench_name(name: str) -> dict:
    """BM_<family>/<api>/<variant>/<type>/<k>=<v>/... (+ GB-appended bare arg segments,
    which duplicate the k=v axes and are dropped)."""
    if not name.startswith("BM_"):
        raise ValueError(f"not a REQ-BENCH-002 name: {name}")
    segs = name[3:].split("/")
    if len(segs) < 4:
        raise ValueError(f"name has fewer than 4 segments: {name}")
    family, api, variant, etype = segs[0], segs[1], segs[2], segs[3]
    axes: dict[str, object] = {}
    for seg in segs[4:]:
        if "=" in seg:
            k, v = seg.split("=", 1)
            axes[k] = int(v) if v.lstrip("-").isdigit() else v
    return {"family": family, "api": api, "variant": variant, "element_type": etype,
            "axes": axes}


def entry_slug(name: str) -> str:
    return re.sub(r"[^a-z0-9]+", "-", name.lower()).strip("-")


def library_version() -> str:
    """Version triple from the single source of truth (include/quiver/detail/config.h)."""
    text = (REPO / "include" / "quiver" / "detail" / "config.h").read_text()
    parts = {}
    for part in ("MAJOR", "MINOR", "PATCH"):
        m = re.search(rf"QUIVER_VERSION_{part}\s+(\d+)", text)
        parts[part] = m.group(1) if m else "0"
    return f"{parts['MAJOR']}.{parts['MINOR']}.{parts['PATCH']}"


def cmd_run(args: argparse.Namespace) -> int:
    machine = environment.load_machine(REPO, args.machine)
    deviations = environment.environment_checklist(REPO, machine)
    if deviations and not args.allow_deviations:
        print("environment checklist FAILED (REQ-LEDGER-013); run is non-publishable:",
              file=sys.stderr)
        for d in deviations:
            print(f"  - {d}", file=sys.stderr)
        print("re-run with --allow-deviations for local investigation only", file=sys.stderr)
        return 2

    build_dir = pathlib.Path(args.build_dir)
    bin_dir = build_dir / "bin"
    # quiver_bench_smoke is harness plumbing; quiver_bench_dispatch measures dispatch MODES
    # (dispatched vs direct), not kernel ISA variants — its third name segment is outside the
    # QLS-1 variant vocabulary by design, so it is not ledger material (gate M5 note).
    skip = {"quiver_bench_smoke", "quiver_bench_dispatch"}
    binaries = sorted(p for p in bin_dir.glob("quiver_bench_*") if p.name not in skip)
    if not binaries:
        print(f"no bench binaries under {bin_dir} — build the bench preset first",
              file=sys.stderr)
        return 2

    name_filter = re.compile(args.filter) if args.filter else None
    jobs: list[tuple[pathlib.Path, str | None, str]] = []
    for binary in binaries:
        for env_cap in gbench.VARIANT_ENV.values():
            for name in gbench.list_benchmarks(binary, env_cap):
                if name_filter is None or name_filter.search(name):
                    jobs.append((binary, env_cap, name))
    if not jobs:
        print("filter matched no benchmarks", file=sys.stderr)
        return 2
    print(f"{len(jobs)} benchmark configurations × {args.reps} repetitions")

    sha, _ = environment.git_state(REPO)
    date = datetime.datetime.now(datetime.UTC).strftime("%Y%m%d")
    out_dir = (pathlib.Path(args.out) if args.out
               else REPO / "ledger" / "results" / machine["uarch_dir"] / f"{date}-{sha}")
    raw_dir = out_dir / "raw"
    raw_dir.mkdir(parents=True, exist_ok=True)

    # Repetitions: fresh process per benchmark, shuffled order per repetition (recorded seed).
    samples: dict[tuple[str | None, str], list[gbench.BenchResult]] = {
        (env_cap, name): [] for (_, env_cap, name) in jobs}
    gb_ver = "unknown"
    for rep in range(args.reps):
        order = list(jobs)
        random.Random(args.seed + rep).shuffle(order)
        for binary, env_cap, name in order:
            raw_text, results = gbench.run_one(binary, name, env_cap, args.min_time)
            if len(results) != 1:
                print(f"expected exactly one result for {name}, got {len(results)}",
                      file=sys.stderr)
                return 2
            gb_ver = gbench.gb_version(raw_text)
            samples[(env_cap, name)].append(results[0])
            slug = entry_slug(name) + ("" if env_cap is None else f"--{env_cap}")
            (raw_dir / f"rep{rep}--{slug}.json").write_text(raw_text)
        print(f"  repetition {rep + 1}/{args.reps} complete")

    manifest = environment.build_manifest(REPO, build_dir, machine, gb_ver, args.seed,
                                          deviations)
    (out_dir / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")

    entries, rejected = [], []
    machine_flags = list(machine.get("flags", []))
    lib_version = library_version()
    ts = datetime.datetime.now(datetime.UTC).isoformat(timespec="seconds")
    for (env_cap, name), reps in samples.items():
        meta = parse_bench_name(name)
        metrics: dict[str, list[float]] = {"ns_per_batch": [r.real_time_ns for r in reps]}
        if all(r.items_per_second is not None for r in reps):
            metrics["values_per_s"] = [r.items_per_second for r in reps]  # type: ignore
        if all(r.bytes_per_second is not None for r in reps):
            metrics["bytes_per_s"] = [r.bytes_per_second for r in reps]  # type: ignore
        if all(r.cycles_per_value is not None for r in reps) and "no_pmu" not in machine_flags:
            metrics["cycles_per_value"] = [r.cycles_per_value for r in reps]  # type: ignore
        results_obj = {}
        for metric, values in metrics.items():
            st = stats.metric_stats(values, seed=args.seed)["median"]
            results_obj[metric] = {"median": st.median, "min": st.min, "ci95_lo": st.ci95_lo,
                                   "ci95_hi": st.ci95_hi, "cv": st.cv}
        cv = results_obj["ns_per_batch"]["cv"]
        noise, publishable = stats.noise_flags(cv)
        flags = sorted(set(machine_flags) | set(noise))
        notes = ""
        if noise and publishable:
            notes = (f"cv={cv:.4f} in the 3-5% band: published with noisy flag per "
                     f"REQ-LEDGER-005")
        entry = {
            "entry_id": f"{machine['uarch_dir']}-{date}-{sha}-{entry_slug(name)}",
            "schema": schema_check.SCHEMA_VERSION,
            "methodology": schema_check.METHODOLOGY_VERSION,
            "benchmark": name,
            "family": meta["family"],
            "api": meta["api"],
            "variant": meta["variant"],
            "element_type": meta["element_type"],
            "axes": meta["axes"],
            "machine_id": machine["machine_id"],
            "manifest_ref": "manifest.json",
            "library_version": lib_version,
            "git_commit": sha,
            "git_dirty": "dirty" if "git tree dirty" in manifest["deviations"] else "clean",
            "timestamp_utc": ts,
            "repetitions": args.reps,
            "bootstrap_seed": args.seed,
            "results": results_obj,
            "metrics": sorted(results_obj.keys()),
            "pmu": None if "no_pmu" in machine_flags else {},
            "flags": flags,
            "notes": notes,
        }
        errors = schema_check.validate_entry(entry)
        if errors:
            print(f"INTERNAL: generated entry fails QLS-1: {errors}", file=sys.stderr)
            return 2
        (entries if publishable else rejected).append(entry)

    (out_dir / "entries.json").write_text(json.dumps(entries, indent=2) + "\n")
    if rejected:
        (out_dir / "rejected_noisy.json").write_text(json.dumps(rejected, indent=2) + "\n")
        print(f"{len(rejected)} entries EXCLUDED (cv > 5%, REQ-LEDGER-005) — rerun advised; "
              f"see rejected_noisy.json")
    print(f"{len(entries)} entries -> {out_dir}")
    if deviations:
        print("NON-PUBLISHABLE (deviations recorded in manifest) — investigation only")
    return 0


def cmd_validate(args: argparse.Namespace) -> int:
    results_root = REPO / "ledger" / "results"
    problems = 0
    run_dirs = sorted(p.parent for p in results_root.glob("*/*/entries.json"))
    if not run_dirs and args.require_results:
        print("no committed results found", file=sys.stderr)
        return 1
    for run_dir in run_dirs:
        manifest = json.loads((run_dir / "manifest.json").read_text())
        for err in schema_check.validate_manifest(manifest):
            print(f"{run_dir}/manifest.json: {err}", file=sys.stderr)
            problems += 1
        entries = json.loads((run_dir / "entries.json").read_text())
        ids = set()
        for entry in entries:
            for err in schema_check.validate_entry(entry):
                print(f"{run_dir}/entries.json [{entry.get('entry_id', '?')}]: {err}",
                      file=sys.stderr)
                problems += 1
            if entry["entry_id"] in ids:
                print(f"{run_dir}: duplicate entry_id {entry['entry_id']}", file=sys.stderr)
                problems += 1
            ids.add(entry["entry_id"])
            if manifest["deviations"]:
                print(f"{run_dir}: committed run has manifest deviations "
                      f"(non-publishable, REQ-LEDGER-013)", file=sys.stderr)
                problems += 1
                break
    print(f"validate: {len(run_dirs)} run dir(s), {problems} problem(s)")
    return 1 if problems else 0


def main() -> int:
    ap = argparse.ArgumentParser(prog="quiver_ledger.py", description=__doc__)
    sub = ap.add_subparsers(dest="cmd", required=True)
    run = sub.add_parser("run", help="execute a ledger run on a registered machine")
    run.add_argument("--machine", required=True)
    run.add_argument("--filter", default="", help="regex over REQ-BENCH-002 names")
    run.add_argument("--build-dir", default=str(REPO / "build" / "bench"))
    run.add_argument("--reps", type=int, default=DEFAULT_REPS)
    run.add_argument("--min-time", default=DEFAULT_MIN_TIME)
    run.add_argument("--seed", type=int, default=20260703)
    run.add_argument("--out", default="")
    run.add_argument("--allow-deviations", action="store_true")
    run.set_defaults(fn=cmd_run)
    val = sub.add_parser("validate", help="validate committed results against QLS-1")
    val.add_argument("--require-results", action="store_true")
    val.set_defaults(fn=cmd_validate)
    args = ap.parse_args()
    return args.fn(args)


if __name__ == "__main__":
    raise SystemExit(main())
