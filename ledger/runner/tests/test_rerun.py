"""Tests for the REQ-LEDGER-005 rerun pass (`--rerun-noisy`).

The requirement's own wording is "excluded from publication until rerun": a CV > 5% entry is
not publishable, but it earns another full fresh-process repetition set rather than a permanent
hole in the run. These tests drive `rerun_noisy` with a scripted `gbench.run_one` and assert
the three contracts that make the rerun honest:

  * REPLACEMENT, never a merge — the new entry's statistics come only from the new pass;
  * evidence — every attempt's raw files stay side by side (`rerun<P>-rep<K>--<slug>.json`);
  * provenance — the surviving entry's notes carry the full CV chain and the pass number.
"""

import argparse
import contextlib
import io
import json
import pathlib
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

import quiver_ledger  # noqa: E402
from qledger import gbench  # noqa: E402

NAME = "BM_mask/and/avx2/bitmap/n=4096/4096"
MACHINE = {"machine_id": "test-machine", "uarch_dir": "test-uarch", "flags": ["no_pmu"]}


def make_args(rerun_noisy: int, reps: int = 10) -> argparse.Namespace:
    # 10 repetitions: the QLS-1 floor (REQ-LEDGER-004) that build_entry enforces.
    return argparse.Namespace(reps=reps, seed=7, min_time="1x", rerun_noisy=rerun_noisy)


def make_ctx(args: argparse.Namespace, out_dir: pathlib.Path) -> dict:
    # rerun_noisy reads args and the raw directory (out_dir / "raw") from the run context,
    # exactly as cmd_run builds it.
    (out_dir / "raw").mkdir(parents=True, exist_ok=True)
    return {"machine": MACHINE, "args": args, "out_dir": out_dir, "run_id": "submission",
            "sha": "abcdef123456", "lib_version": "0.0.0", "git_dirty": "clean",
            "timestamp": "2026-01-01T00:00:00+00:00"}


def result(ns: float) -> gbench.BenchResult:
    return gbench.BenchResult(name=NAME, real_time_ns=ns, items_per_second=None,
                              bytes_per_second=None, cycles_per_value=None)


def scripted_run_one(per_pass_values: list[list[float]]):
    """A run_one double serving one value per call, pass after pass, and returning the raw
    JSON text the runner writes to disk."""
    queue = [ns for values in per_pass_values for ns in values]

    def fake_run_one(binary, name, env_cap, min_time):
        ns = queue.pop(0)
        raw = json.dumps({"context": {"library_build_type": "release"}, "benchmarks": []})
        return raw, [result(ns)]

    return fake_run_one


NOISY = [100.0] * 9 + [130.0]           # cv ~= 0.092 — rejected by the 5% screen
CLEAN = [100.0, 100.5] * 5              # cv ~= 0.0025 — publishable


def scripted_by_name(values_by_name: dict[str, list[float]]):
    """Like scripted_run_one, but keyed by benchmark name: with several jobs in one pass the
    per-repetition shuffle interleaves calls, so positional scripting cannot address one
    benchmark. Each name consumes its own queue in call order."""
    queues = {name: list(values) for name, values in values_by_name.items()}

    def fake_run_one(binary, name, env_cap, min_time):
        ns = queues[name].pop(0)
        raw = json.dumps({"context": {"library_build_type": "release"}, "benchmarks": []})
        return raw, [gbench.BenchResult(name=name, real_time_ns=ns, items_per_second=None,
                                        bytes_per_second=None, cycles_per_value=None)]

    return fake_run_one


class TestRerunNoisy(unittest.TestCase):
    def _initial_rejected(self, ctx):
        entry, publishable = quiver_ledger.build_entry(NAME, [result(v) for v in NOISY], ctx)
        self.assertFalse(publishable)
        return entry

    def test_recovered_on_first_pass(self):
        args = make_args(rerun_noisy=2)
        jobs = [(pathlib.Path("bin"), None, NAME)]
        with tempfile.TemporaryDirectory() as tmp:
            ctx = make_ctx(args, pathlib.Path(tmp))
            raw_dir = pathlib.Path(tmp) / "raw"
            rejected = [self._initial_rejected(ctx)]
            with mock.patch.object(quiver_ledger.gbench, "run_one",
                                   scripted_run_one([CLEAN])):
                entries, still = quiver_ledger.rerun_noisy(jobs, [], rejected, ctx)
            self.assertEqual(len(entries), 1)
            self.assertEqual(still, [])
            # Statistics come only from the rerun pass — replacement, not a merge.
            self.assertAlmostEqual(entries[0]["results"]["ns_per_batch"]["median"], 100.25)
            # Provenance: pass number and the full CV chain in the notes.
            self.assertIn("rerun pass 1", entries[0]["notes"])
            self.assertIn("->", entries[0]["notes"])
            # Evidence: rerun raw files exist alongside where rep files would live, and the
            # prefix cannot collide with a first-pass `rep*` glob.
            reruns = sorted(p.name for p in raw_dir.glob("rerun1-rep*"))
            self.assertEqual(len(reruns), args.reps)
            self.assertEqual(list(raw_dir.glob("rep*")), [])

    def test_still_noisy_after_all_passes(self):
        args = make_args(rerun_noisy=2)
        jobs = [(pathlib.Path("bin"), None, NAME)]
        with tempfile.TemporaryDirectory() as tmp:
            ctx = make_ctx(args, pathlib.Path(tmp))
            raw_dir = pathlib.Path(tmp) / "raw"
            rejected = [self._initial_rejected(ctx)]
            with mock.patch.object(quiver_ledger.gbench, "run_one",
                                   scripted_run_one([NOISY, NOISY])):
                entries, still = quiver_ledger.rerun_noisy(jobs, [], rejected, ctx)
            self.assertEqual(entries, [])
            self.assertEqual(len(still), 1)
            self.assertIn("rerun pass 2", still[0]["notes"])
            # Three CVs in the chain: initial + two failed passes.
            chain = still[0]["notes"].split("cv ")[1].split(";")[0]
            self.assertEqual(chain.count("->"), 2)
            self.assertEqual(len(list(raw_dir.glob("rerun1-rep*"))), args.reps)
            self.assertEqual(len(list(raw_dir.glob("rerun2-rep*"))), args.reps)

    def test_stops_early_when_nothing_left(self):
        args = make_args(rerun_noisy=5)
        jobs = [(pathlib.Path("bin"), None, NAME)]
        with tempfile.TemporaryDirectory() as tmp:
            ctx = make_ctx(args, pathlib.Path(tmp))
            raw_dir = pathlib.Path(tmp) / "raw"
            rejected = [self._initial_rejected(ctx)]
            with mock.patch.object(quiver_ledger.gbench, "run_one",
                                   scripted_run_one([CLEAN])):
                entries, still = quiver_ledger.rerun_noisy(jobs, [], rejected, ctx)
            self.assertEqual(len(entries), 1)
            # Pass 1 recovered the entry; passes 2..5 must not have run.
            self.assertEqual(list(raw_dir.glob("rerun2-*")), [])

    def test_disabled_is_a_no_op(self):
        args = make_args(rerun_noisy=0)
        with tempfile.TemporaryDirectory() as tmp:
            ctx = make_ctx(args, pathlib.Path(tmp))
            rejected = [self._initial_rejected(ctx)]
            entries, still = quiver_ledger.rerun_noisy([], [], list(rejected), ctx)
        self.assertEqual(entries, [])
        self.assertEqual(still, rejected)

    def test_partial_recovery_keeps_cv_chains_separate(self):
        """Two rejected entries, one recovering per pass: the slow entry's CV chain must grow
        by exactly one value per pass and never absorb the fast entry's history."""
        name2 = "BM_mask/or/avx2/bitmap/n=4096/4096"
        args = make_args(rerun_noisy=2)
        jobs = [(pathlib.Path("bin"), None, NAME), (pathlib.Path("bin"), None, name2)]
        with tempfile.TemporaryDirectory() as tmp:
            ctx = make_ctx(args, pathlib.Path(tmp))
            raw_dir = pathlib.Path(tmp) / "raw"
            e1, _ = quiver_ledger.build_entry(NAME, [result(v) for v in NOISY], ctx)
            e2, _ = quiver_ledger.build_entry(name2, [result(v) for v in NOISY], ctx)
            with mock.patch.object(quiver_ledger.gbench, "run_one", scripted_by_name(
                    {NAME: CLEAN, name2: NOISY + CLEAN})):
                entries, still = quiver_ledger.rerun_noisy(jobs, [], [e1, e2], ctx)
            self.assertEqual(still, [])
            by_name = {e["benchmark"]: e for e in entries}
            self.assertEqual(len(by_name), 2)
            fast, slow = by_name[NAME], by_name[name2]
            self.assertIn("rerun pass 1", fast["notes"])
            self.assertEqual(fast["notes"].split("cv ")[1].split(";")[0].count("->"), 1)
            self.assertIn("rerun pass 2", slow["notes"])
            self.assertEqual(slow["notes"].split("cv ")[1].split(";")[0].count("->"), 2)
            # Pass 1 measured both benchmarks; pass 2 only the still-rejected one.
            self.assertEqual(len(list(raw_dir.glob("rerun1-rep*"))), 2 * args.reps)
            self.assertEqual(len(list(raw_dir.glob("rerun2-rep*"))), args.reps)

    def test_negative_rerun_noisy_is_rejected(self):
        """A negative pass count would silently no-op (range(1, 0) is empty); argparse must
        refuse it instead."""
        with self.assertRaises(SystemExit), \
                mock.patch.object(sys, "argv", ["quiver_ledger.py", "run", "--machine", "m",
                                                "--rerun-noisy", "-3"]), \
                contextlib.redirect_stderr(io.StringIO()):
            quiver_ledger.main()

    def test_reproduction_command_records_the_flag(self):
        """Charter T2: every argument that changes the result is recorded, defaults included."""
        args = argparse.Namespace(machine="m", output="submission", filter="", reps=20,
                                  min_time="0.3s", seed=1, rerun_noisy=2,
                                  build_dir="build/bench", allow_deviations=False)
        self.assertIn("--rerun-noisy 2", quiver_ledger.reproduction_command(args))


if __name__ == "__main__":
    unittest.main()
