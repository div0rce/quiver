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
import hashlib
import json
import pathlib
import random
import re
import shlex
import subprocess
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


def summarize(values: list[float], seed: int) -> dict[str, float]:
    """The ADR-020 median estimator with its bootstrap CI and CV, as stored in an entry."""
    st = stats.metric_stats(values, seed=seed)["median"]
    return {"median": st.median, "min": st.min, "ci95_lo": st.ci95_lo,
            "ci95_hi": st.ci95_hi, "cv": st.cv}


def build_pmu(reps: list, machine_flags: list[str], seed: int) -> dict | None:
    """The entry's `pmu` object (REQ-BENCH-005, Charter §6.4).

    Three outcomes, deliberately distinct:
      * `None`          — the platform cannot expose counters at all (machine flagged `no_pmu`,
                          e.g. Apple Silicon withholds them from user processes);
      * `unavailable`   — a capable platform where perf_event_open did not succeed this run
                          (kernel.perf_event_paranoid too high, no CAP_PERFMON);
      * `available`     — counters measured, summarized like any other metric.

    `cycles_per_value` is not here: it is a per-value rate screened by the CV policy, so it lives
    in `results`. `ipc` and `branch_miss_pct` are microarchitectural context.
    """
    if "no_pmu" in machine_flags:
        return None
    # Summarize over the repetitions that DID read counters. Requiring all ten threw away every
    # measurement when a single perf_event_open failed transiently under load; the count is
    # recorded so a partial sample is visible rather than passed off as a full one.
    counters = {"ipc": [r.ipc for r in reps if r.ipc is not None],
                "branch_miss_pct": [r.branch_miss_pct for r in reps
                                    if r.branch_miss_pct is not None]}
    if not all(counters.values()):
        return {"status": "unavailable"}
    out = {"status": "available", "repetitions_measured": min(map(len, counters.values()))}
    if out["repetitions_measured"] < len(reps):
        out["status"] = "partial"  # some repetitions had no PMU access
    return {**out, **{name: summarize(values, seed) for name, values in counters.items()}}


class RunAborted(Exception):
    """A run cannot proceed. Carries the operator-facing reason; cmd_run turns it into exit 2."""


def discover_jobs(build_dir: pathlib.Path, name_filter) -> list[tuple[pathlib.Path, str | None, str]]:
    """(binary, ISA cap, benchmark name) triples to measure.

    quiver_bench_smoke is harness plumbing; quiver_bench_dispatch measures dispatch MODES rather
    than kernel ISA variants, so its third name segment is outside the QLS-1 variant vocabulary
    and it is not ledger material (gate M5 note).

    A benchmark name already encodes its variant (REQ-BENCH-002), so the same name under two caps
    is the same measurement twice. On x86 the autovec-* baselines register in both the uncapped
    and QUIVER_ISA=scalar processes — separate compiled TUs (ADR-011) that never route through
    dispatch — which produced 75 duplicate benchmarks with colliding entry_ids in the first x86
    run. Keep the first cap offering a name; the raw filename still records which cap ran it.
    """
    bin_dir = build_dir / "bin"
    skip = {"quiver_bench_smoke", "quiver_bench_dispatch"}
    binaries = sorted(p for p in bin_dir.glob("quiver_bench_*") if p.name not in skip)
    if not binaries:
        raise RunAborted(f"no bench binaries under {bin_dir} — build the bench preset first")

    jobs = [job for binary in binaries for job in _binary_jobs(binary, name_filter)]
    if not jobs:
        raise RunAborted("filter matched no benchmarks")
    return jobs


def _selected(name: str, name_filter) -> bool:
    return name_filter is None or bool(name_filter.search(name))


def _binary_jobs(binary: pathlib.Path, name_filter):
    """Every distinct benchmark name this binary offers, paired with the first cap exposing it."""
    seen: set[str] = set()
    for env_cap in gbench.VARIANT_ENV.values():
        for name in gbench.list_benchmarks(binary, env_cap):
            if name not in seen and _selected(name, name_filter):
                seen.add(name)
                yield binary, env_cap, name


def collect_samples(jobs: list, args: argparse.Namespace, raw_dir: pathlib.Path
                    ) -> tuple[dict, str]:
    """Run every job `args.reps` times, one fresh process each, order shuffled per repetition
    from a recorded seed (REQ-LEDGER-006). Returns (samples keyed by (cap, name), GB version)."""
    samples: dict[tuple[str | None, str], list[gbench.BenchResult]] = {
        (env_cap, name): [] for (_, env_cap, name) in jobs}
    gb_ver = "unknown"
    for rep in range(args.reps):
        order = list(jobs)
        random.Random(args.seed + rep).shuffle(order)
        for binary, env_cap, name in order:
            raw_text, results = gbench.run_one(binary, name, env_cap, args.min_time)
            if len(results) != 1:
                raise RunAborted(f"expected exactly one result for {name}, got {len(results)}")
            gb_ver = gbench.gb_version(raw_text)
            samples[(env_cap, name)].append(results[0])
            slug = entry_slug(name) + ("" if env_cap is None else f"--{env_cap}")
            (raw_dir / f"rep{rep}--{slug}.json").write_text(raw_text)
        print(f"  repetition {rep + 1}/{args.reps} complete")
    return samples, gb_ver


def _all_or_none(reps: list, attr: str) -> list[float] | None:
    """A throughput metric, or None when any repetition lacks it.

    These stay all-or-nothing: a missing items/bytes rate means the benchmark does not report
    one at all, not that a counter dropped out.
    """
    values = [getattr(r, attr) for r in reps]
    return values if all(v is not None for v in values) else None


def _measured_only(reps: list, attr: str) -> list[float] | None:
    """A PMU-derived metric over the repetitions that measured it, or None if none did.

    PMU metrics share attach_pmu's failure mode, so they get the same tolerance build_pmu has:
    summarize the repetitions that did measure rather than discarding all ten because one
    perf_event_open failed transiently.
    """
    values = [v for v in (getattr(r, attr) for r in reps) if v is not None]
    return values or None


def collect_metrics(reps: list, machine_flags: list[str]) -> dict[str, list[float]]:
    """Per-metric sample vectors. Throughput is all-or-nothing, PMU metrics tolerate gaps."""
    metrics = {"ns_per_batch": [r.real_time_ns for r in reps]}
    optional = {"values_per_s": _all_or_none(reps, "items_per_second"),
                "bytes_per_s": _all_or_none(reps, "bytes_per_second")}
    if "no_pmu" not in machine_flags:
        optional["cycles_per_value"] = _measured_only(reps, "cycles_per_value")
    metrics.update({k: v for k, v in optional.items() if v is not None})
    return metrics


def build_entry(name: str, reps: list, ctx: dict) -> tuple[dict, bool]:
    """One QLS-1 entry plus whether the CV policy lets it be published (REQ-LEDGER-005)."""
    meta = parse_bench_name(name)
    machine, args = ctx["machine"], ctx["args"]
    machine_flags = list(machine.get("flags", []))
    results_obj = {metric: summarize(values, args.seed)
                   for metric, values in collect_metrics(reps, machine_flags).items()}
    pmu_obj = build_pmu(reps, machine_flags, args.seed)
    cv = results_obj["ns_per_batch"]["cv"]
    noise, publishable = stats.noise_flags(cv)
    # A machine without a static no_pmu flag can still fail perf_event_open transiently or by
    # configuration; flag the measured outcome so flag-based filtering finds it instead of it
    # being visible only inside the pmu object. A `partial` sample keeps its counters and is
    # recorded there — `no_pmu` would misstate it, and the schema fixes the flag vocabulary.
    measured = {"no_pmu"} if (pmu_obj or {}).get("status") == "unavailable" else set()
    notes = (f"cv={cv:.4f} in the 3-5% band: published with noisy flag per REQ-LEDGER-005"
             if noise and publishable else "")
    entry = {
        # Keyed by the run DIRECTORY name so supplementary runs at the same commit
        # (e.g. <date>-<sha>-b) can never collide with the main run's ids.
        "entry_id": f"{machine['uarch_dir']}-{ctx['run_id']}-{entry_slug(name)}",
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
        "library_version": ctx["lib_version"],
        "git_commit": ctx["sha"],
        "git_dirty": ctx["git_dirty"],
        "timestamp_utc": ctx["timestamp"],
        "repetitions": args.reps,
        "bootstrap_seed": args.seed,
        "results": results_obj,
        "metrics": sorted(results_obj.keys()),
        "pmu": pmu_obj,
        "flags": sorted(set(machine_flags) | set(noise) | measured),
        "notes": notes,
    }
    errors = schema_check.validate_entry(entry)
    if errors:
        raise RunAborted(f"INTERNAL: generated entry fails QLS-1: {errors}")
    return entry, publishable


def run_identity(sha: str) -> str:
    """The canonical name of a run: `<date>-<commit>`.

    entry_id is built from THIS, never from the output directory name. community-run writes to
    the literal `submission`, so deriving identity from the directory made every bundle from one
    machine mint identical ids — and because the ids are baked into entries.json, renaming the
    directory at append time cannot repair them. The identity is reproducible (same commit, same
    day, same id) and independent of `--output`.
    """
    return f"{datetime.datetime.now(datetime.UTC).strftime('%Y%m%d')}-{sha}"


def resolve_out_dir(args: argparse.Namespace, machine: dict, run_id: str) -> pathlib.Path:
    """Where results are written. The canonical destination is named for the run identity; an
    explicit --output only moves the files, it does not change what the run IS."""
    if args.out:
        return pathlib.Path(args.out)
    return REPO / "ledger" / "results" / machine["uarch_dir"] / run_id


def write_results(out_dir: pathlib.Path, entries: list, rejected: list,
                  deviations: list[str]) -> None:
    (out_dir / "entries.json").write_text(json.dumps(entries, indent=2) + "\n")
    # Always emit the rejected-noisy record, empty when nothing was excluded. CONTRIBUTING.md
    # lists it as part of a Lane A bundle, and an explicit [] states "nothing was rejected"
    # where a missing file cannot be told apart from a lost artifact.
    (out_dir / "rejected_noisy.json").write_text(json.dumps(rejected, indent=2) + "\n")
    if rejected:
        print(f"{len(rejected)} entries EXCLUDED (cv > 5%, REQ-LEDGER-005) — rerun advised; "
              f"see rejected_noisy.json")
    print(f"{len(entries)} entries -> {out_dir}")
    if deviations:
        print("NON-PUBLISHABLE (deviations recorded in manifest) — investigation only")


def report_deviations(deviations: list[str]) -> None:
    print("environment checklist FAILED (REQ-LEDGER-013); run is non-publishable:",
          file=sys.stderr)
    for d in deviations:
        print(f"  - {d}", file=sys.stderr)
    print("re-run with --allow-deviations for local investigation only", file=sys.stderr)


def partition_entries(samples: dict, ctx: dict) -> tuple[list, list]:
    """Split measured samples into publishable entries and CV-rejected ones."""
    entries, rejected = [], []
    for (_, name), reps in samples.items():
        entry, publishable = build_entry(name, reps, ctx)
        (entries if publishable else rejected).append(entry)
    return entries, rejected


def cmd_run(args: argparse.Namespace) -> int:
    machine = environment.load_machine(REPO, args.machine)
    deviations = environment.environment_checklist(REPO, machine)
    if deviations and not args.allow_deviations:
        report_deviations(deviations)
        return 2

    build_dir = pathlib.Path(args.build_dir)
    try:
        jobs = discover_jobs(build_dir, re.compile(args.filter) if args.filter else None)
        print(f"{len(jobs)} benchmark configurations × {args.reps} repetitions")

        sha, _ = environment.git_state(REPO)
        run_id = run_identity(sha)
        out_dir = resolve_out_dir(args, machine, run_id)
        (out_dir / "raw").mkdir(parents=True, exist_ok=True)
        samples, gb_ver = collect_samples(jobs, args, out_dir / "raw")

        manifest = environment.build_manifest(
            environment.RunContext(repo=REPO, build_dir=build_dir, out_dir=out_dir,
                                   run_id=run_id, machine=machine, gb_version=gb_ver,
                                   repetition_seed=args.seed),
            deviations)
        (out_dir / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")

        # The manifest re-derives commit and deviations AFTER the run so a mid-run edit is
        # caught; entries must quote those authoritative values, not the pre-run snapshot used
        # only to name the output directory.
        # The manifest's commit is authoritative (post-run), so the identity follows it.
        run_id = run_identity(manifest["library_commit"])
        ctx = {"machine": machine, "args": args, "out_dir": out_dir, "run_id": run_id,
               "sha": manifest["library_commit"],
               "lib_version": library_version(),
               "git_dirty": "dirty" if "git tree dirty" in manifest["deviations"] else "clean",
               "timestamp": datetime.datetime.now(datetime.UTC).isoformat(timespec="seconds")}
        entries, rejected = partition_entries(samples, ctx)
    except RunAborted as exc:
        print(str(exc), file=sys.stderr)
        return 2

    write_results(out_dir, entries, rejected, manifest["deviations"])
    return 0


def _load_json(path: pathlib.Path, expect: type) -> tuple[object | None, list[str]]:
    """(document, problems). A malformed artifact is a VALIDATION FAILURE, never a traceback:
    `validate` is the gate a reviewer runs on someone else's bundle, so it must survive a
    truncated, unreadable or wrong-shaped file and say which one it was."""
    label = f"{path.parent.name}/{path.name}"
    if not path.exists():
        return None, [f"{label}: missing"]
    if not path.is_file():
        return None, [f"{label}: not a regular file"]
    try:
        doc = json.loads(path.read_text())
    except OSError as exc:
        return None, [f"{label}: unreadable ({exc.strerror})"]
    except json.JSONDecodeError as exc:
        return None, [f"{label}: invalid JSON at line {exc.lineno} column {exc.colno}"]
    if not isinstance(doc, expect):
        return None, [f"{label}: top-level JSON is {type(doc).__name__}, expected "
                      f"{expect.__name__}"]
    return doc, []


def _manifest_problems(run_dir: pathlib.Path) -> list[str]:
    manifest, problems = _load_json(run_dir / "manifest.json", dict)
    if manifest is None:
        return problems
    problems += [f"{run_dir}/manifest.json: {e}" for e in schema_check.validate_manifest(manifest)]
    if manifest.get("deviations"):
        problems.append(f"{run_dir}: committed run has manifest deviations "
                        f"(non-publishable, REQ-LEDGER-013)")
    return problems


def _entry_problems(run_dir: pathlib.Path, index: int, entry: object,
                    seen: set[str]) -> list[str]:
    if not isinstance(entry, dict):
        return [f"{run_dir}/entries.json [{index}]: entry is {type(entry).__name__}, "
                f"expected object"]
    entry_id = entry.get("entry_id", "?")
    problems = [f"{run_dir}/entries.json [{entry_id}]: {e}"
                for e in schema_check.validate_entry(entry)]
    if entry_id in seen:
        problems.append(f"{run_dir}: duplicate entry_id {entry_id}")
    seen.add(entry_id)
    return problems


def validate_run_dir(run_dir: pathlib.Path) -> list[str]:
    """Every QLS-1 problem in one run directory (REQ-LEDGER-013/-015)."""
    problems = _manifest_problems(run_dir)
    entries, entry_problems = _load_json(run_dir / "entries.json", list)
    problems += entry_problems
    seen: set[str] = set()
    for index, entry in enumerate(entries or []):
        problems += _entry_problems(run_dir, index, entry, seen)
    return problems


def cross_run_id_collisions(run_dirs: list[pathlib.Path]) -> list[str]:
    """entry_id must be unique across the whole ledger, not just within one run.

    build_entry derives it from the OUTPUT DIRECTORY NAME, which is unique for a default run
    (`<date>-<sha>`) but is the literal `submission` for every community-run bundle. Two
    submissions from the same machine therefore mint identical ids; appending the second makes
    every `qle:<entry_id>` doc reference ambiguous (REQ-LEDGER-015). validate_run_dir only sees
    one directory, so the collision is invisible to it.
    """
    owner: dict[str, pathlib.Path] = {}
    problems: list[str] = []
    for run_dir in run_dirs:
        entries, _ = _load_json(run_dir / "entries.json", list)  # malformed: reported elsewhere
        for entry in entries or []:
            if not isinstance(entry, dict):
                continue
            entry_id = entry.get("entry_id", "?")
            if entry_id in owner:
                problems.append(f"{run_dir}: entry_id {entry_id} already published by "
                                f"{owner[entry_id]} — ids must be unique across the ledger "
                                f"(REQ-LEDGER-015); rename the run directory before appending")
            else:
                owner[entry_id] = run_dir
    return problems


def cmd_validate(args: argparse.Namespace) -> int:
    run_dirs = sorted(p.parent for p in (REPO / "ledger" / "results").glob("*/*/entries.json"))
    # A Lane A bundle in flight lives at the repo root (CONTRIBUTING.md) and is reviewed before
    # it is appended to ledger/results/, so it must face the same QLS-1 checks, not fewer.
    if (REPO / "submission" / "entries.json").exists():
        run_dirs.append(REPO / "submission")
    if not run_dirs and args.require_results:
        print("no committed results found", file=sys.stderr)
        return 1
    problems = [p for run_dir in run_dirs for p in validate_run_dir(run_dir)]
    problems += cross_run_id_collisions(run_dirs)
    for problem in problems:
        print(problem, file=sys.stderr)
    print(f"validate: {len(run_dirs)} run dir(s), {len(problems)} problem(s)")
    return 1 if problems else 0


def _git_or_die(*argv: str) -> str:
    """Provenance must fail loudly: a silently empty git-status.txt reads as a clean tree."""
    proc = subprocess.run(["git", "-C", str(REPO), *argv],
                          capture_output=True, text=True, timeout=30)
    if proc.returncode != 0:
        raise RunAborted(f"git {' '.join(argv)} failed ({proc.returncode}): "
                         f"{proc.stderr.strip()[:200]}")
    return proc.stdout


def reproduction_command(args: argparse.Namespace) -> str:
    """Charter T2: "can a stranger with the named hardware reproduce the number from the repo
    alone?" — so record EVERY argument that changes the result, defaults included. --seed drives
    the bootstrap CIs (ADR-020), so omitting it silently changed the reported interval;
    --allow-deviations marks the bundle investigation-only."""
    parts = ["python3", "ledger/runner/quiver_ledger.py", "community-run",
             "--machine", str(args.machine), "--output", str(args.output)]
    if args.filter:
        parts += ["--filter", str(args.filter)]
    parts += ["--reps", str(args.reps), "--min-time", str(args.min_time),
              "--seed", str(args.seed), "--build-dir", str(args.build_dir)]
    if args.allow_deviations:
        parts.append("--allow-deviations")
    # shlex.join quotes only what needs it, and escapes embedded quotes — a filter regex or a
    # path containing a space must survive copy-paste for T2 reproduction to mean anything.
    return shlex.join(parts) + "\n"


def write_provenance(out_dir: pathlib.Path, args: argparse.Namespace) -> int:
    """The sidecars a reviewer needs: exact command, git state, checksums over every file."""
    (out_dir / "commands.txt").write_text(reproduction_command(args))
    (out_dir / "git-status.txt").write_text(
        f"commit: {_git_or_die('rev-parse', 'HEAD').strip()}\n"
        f"{_git_or_die('status', '--porcelain=v1', '-b')}")
    sums = [f"{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.relative_to(out_dir)}"
            for p in sorted(out_dir.rglob("*")) if p.is_file() and p.name != "checksums.txt"]
    (out_dir / "checksums.txt").write_text("\n".join(sums) + "\n")
    return len(sums)


def cmd_community_run(args: argparse.Namespace) -> int:
    """One-command community submission (the REQ-LEDGER-012 funnel): a normal `run` into a
    self-contained submission directory, plus provenance sidecars. The contributor opens a PR
    containing only this directory (CONTRIBUTING.md, Lane A)."""
    out_dir = pathlib.Path(args.output)
    args.out = str(out_dir)
    rc = cmd_run(args)
    if rc != 0:
        return rc
    try:
        count = write_provenance(out_dir, args)
    except RunAborted as exc:
        print(str(exc), file=sys.stderr)
        return 2
    print(f"submission bundle ready: {out_dir} ({count} files checksummed)")
    print("next: open a PR containing only this directory (CONTRIBUTING.md, Lane A)")
    return 0


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
    community = sub.add_parser(
        "community-run",
        help="a ledger run packaged as a self-contained submission directory "
             "(entries + manifest + raw + commands.txt + git-status.txt + checksums.txt)")
    community.add_argument("--machine", required=True)
    community.add_argument("--filter", default="", help="regex over REQ-BENCH-002 names")
    community.add_argument("--build-dir", default=str(REPO / "build" / "bench"))
    community.add_argument("--reps", type=int, default=DEFAULT_REPS)
    community.add_argument("--min-time", default=DEFAULT_MIN_TIME)
    community.add_argument("--seed", type=int, default=20260703)
    community.add_argument("--output", default="submission",
                           help="submission directory (default: submission/)")
    community.add_argument("--allow-deviations", action="store_true")
    community.set_defaults(fn=cmd_community_run)
    val = sub.add_parser("validate", help="validate committed results against QLS-1")
    val.add_argument("--require-results", action="store_true")
    val.set_defaults(fn=cmd_validate)
    args = ap.parse_args()
    return args.fn(args)


if __name__ == "__main__":
    raise SystemExit(main())
