"""Tests for the Google Benchmark filter escaping in qledger.gbench (PRD 11 §7).

`run_one` pins a benchmark by compiling its exact name into a `--benchmark_filter` anchored regex.
Google Benchmark compiles that as a POSIX ERE, where a backslash before a non-metacharacter is
undefined: it reports "Could not compile benchmark re: Invalid escape in regular expression", then
exits 0 having printed nothing, so the failure surfaces downstream as a JSON parse error rather than
a filter error.

Python's `re.escape` escapes '-', which makes every hyphenated variant name unrunnable. Those are
exactly the x86 auto-vectorized baselines (`autovec-avx2`, `autovec-avx512`, ADR-011 / REQ-BENCH-002)
— unreachable while Apple M2 (variants `neon`, `autovec`) was the only registered machine.
"""

import argparse
import json
import pathlib
import re
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

import quiver_ledger  # noqa: E402
from qledger import environment, gbench  # noqa: E402

# Real REQ-BENCH-002 names, including the hyphenated x86 baselines that regressed.
NAMES = [
    "BM_mask/and/avx2/bitmap/n=4096/4096",
    "BM_mask/and/autovec-avx2/bitmap/n=4096/4096",
    "BM_mask/and/autovec-avx512/bitmap/n=65536/65536",
    "BM_reduce/sum_wrap/avx2/i64/n=4096/nulls=0/4096/0",
    "BM_compare/bitmap_gt/neon/i8/n=4096/sel=10/4096/10",
]


class TestEreEscape(unittest.TestCase):
    def test_hyphen_is_not_escaped(self):
        """The regression: '-' is literal outside a bracket expression, and GB rejects '\\-'."""
        self.assertNotIn("\\-", gbench.ere_escape("autovec-avx2"))
        self.assertEqual(gbench.ere_escape("autovec-avx2"), "autovec-avx2")

    def test_slash_and_equals_are_not_escaped(self):
        """Both appear in every benchmark name and are literal in an ERE."""
        escaped = gbench.ere_escape("BM_mask/and/x/n=4096")
        self.assertNotIn("\\/", escaped)
        self.assertNotIn("\\=", escaped)
        self.assertEqual(escaped, "BM_mask/and/x/n=4096")

    def test_metacharacters_are_escaped(self):
        # ']' and '}' are excluded on purpose — see TestEreClosingDelimiters.
        for ch in ".^$*+?()[{|\\":
            with self.subTest(ch=ch):
                self.assertEqual(gbench.ere_escape(ch), "\\" + ch)

    def test_anchored_pattern_matches_only_itself(self):
        """Escaping must stay exact: a name's pattern matches that name and no sibling."""
        for name in NAMES:
            with self.subTest(name=name):
                rx = re.compile("^" + gbench.ere_escape(name) + "$")
                self.assertTrue(rx.match(name))
                for other in NAMES:
                    if other != name:
                        self.assertIsNone(rx.match(other))

    def test_metacharacters_cannot_widen_a_match(self):
        """An unescaped '.' would let one name select another; confirm it is neutralised."""
        rx = re.compile("^" + gbench.ere_escape("BM_a.c") + "$")
        self.assertTrue(rx.match("BM_a.c"))
        self.assertIsNone(rx.match("BM_abc"))


class TestPmuParsing(unittest.TestCase):
    """REQ-BENCH-005: the harness emits cycles_per_value, ipc and branch_miss_pct when
    perf_event_open succeeds. All three must survive into BenchResult — ipc and branch_miss_pct
    were previously parsed into `raw` only and dropped before reaching the entry."""

    @staticmethod
    def _doc(**counters):
        bench = {"name": "BM_x/a/avx2/i64/n=4/4", "real_time": 1.0, "time_unit": "ns"}
        bench.update(counters)
        return json.dumps({"benchmarks": [bench]})

    def test_counters_are_parsed(self):
        r = gbench.parse_gb_json(self._doc(
            cycles_per_value=0.5, ipc=3.4, branch_miss_pct=0.011))[0]
        self.assertAlmostEqual(r.cycles_per_value, 0.5)
        self.assertAlmostEqual(r.ipc, 3.4)
        self.assertAlmostEqual(r.branch_miss_pct, 0.011)

    def test_absent_counters_are_none_not_zero(self):
        """No PMU access must be distinguishable from a measured zero."""
        r = gbench.parse_gb_json(self._doc())[0]
        self.assertIsNone(r.cycles_per_value)
        self.assertIsNone(r.ipc)
        self.assertIsNone(r.branch_miss_pct)

    def test_partial_counters(self):
        r = gbench.parse_gb_json(self._doc(ipc=2.0))[0]
        self.assertAlmostEqual(r.ipc, 2.0)
        self.assertIsNone(r.branch_miss_pct)


class TestBuildPmu(unittest.TestCase):
    """`pmu` must distinguish three outcomes: the platform cannot measure (None), it could but
    did not this run ("unavailable"), or it did ("available"). The field was previously hardcoded
    to `{}` for every PMU-capable machine, collapsing the last two."""

    @staticmethod
    def _reps(ipc, bmiss, n=10):
        return [gbench.BenchResult(name="x", real_time_ns=1.0, items_per_second=None,
                                   bytes_per_second=None, cycles_per_value=None,
                                   ipc=ipc, branch_miss_pct=bmiss) for _ in range(n)]

    def test_no_pmu_platform_is_null(self):
        """Apple Silicon withholds counters entirely (Charter §6.4) — not the same as 'unavailable'."""
        self.assertIsNone(quiver_ledger.build_pmu(self._reps(3.0, 0.01), ["no_pmu"], seed=1))

    def test_missing_counters_report_unavailable(self):
        got = quiver_ledger.build_pmu(self._reps(None, None), [], seed=1)
        self.assertEqual(got, {"status": "unavailable"})

    def test_partial_counters_report_unavailable(self):
        """Half a measurement is not a measurement."""
        got = quiver_ledger.build_pmu(self._reps(3.0, None), [], seed=1)
        self.assertEqual(got, {"status": "unavailable"})

    def test_available_carries_summarized_counters(self):
        got = quiver_ledger.build_pmu(self._reps(3.5, 0.002), [], seed=1)
        self.assertEqual(got["status"], "available")
        for key in ("ipc", "branch_miss_pct"):
            self.assertEqual(sorted(got[key]), ["ci95_hi", "ci95_lo", "cv", "median", "min"])
        self.assertAlmostEqual(got["ipc"]["median"], 3.5)
        self.assertAlmostEqual(got["branch_miss_pct"]["median"], 0.002)

    def test_zero_is_measured_not_missing(self):
        """A genuine 0.0 branch-miss rate must not be mistaken for an absent counter."""
        got = quiver_ledger.build_pmu(self._reps(3.5, 0.0), [], seed=1)
        self.assertEqual(got["status"], "available")
        self.assertAlmostEqual(got["branch_miss_pct"]["median"], 0.0)


class TestManifestDirtiness(unittest.TestCase):
    """A run must not flag itself dirty by writing its own output, but must still catch a real
    edit made mid-run. So dirtiness is re-checked after the run with out_dir excluded, rather
    than trusting a pre-run snapshot (which would miss mid-run edits entirely)."""

    def setUp(self):
        self.repo = pathlib.Path(tempfile.mkdtemp(prefix="qledger-")) / "r"
        self.repo.mkdir(parents=True)
        for cmd in (["init", "-q", "-b", "main"], ["config", "user.email", "t@e.invalid"],
                    ["config", "user.name", "t"]):
            subprocess.run(["git", "-C", str(self.repo), *cmd], check=True)
        (self.repo / "tracked.txt").write_text("v1\n")
        subprocess.run(["git", "-C", str(self.repo), "add", "-A"], check=True)
        subprocess.run(["git", "-C", str(self.repo), "commit", "-qm", "base"], check=True)
        self.out = self.repo / "submission"
        self.out.mkdir()
        (self.out / "entries.json").write_text("[]")

    def test_own_output_does_not_mark_dirty(self):
        """The regression: community-run writes ./submission inside the repo."""
        self.assertEqual(environment.git_state(self.repo, self.out)[1], "clean")
        self.assertEqual(environment.git_state(self.repo)[1], "dirty")  # unexcluded => dirty

    def test_real_edit_is_still_caught(self):
        """Excluding out_dir must not blind the check to an actual mid-run change."""
        (self.repo / "tracked.txt").write_text("edited mid-run\n")
        self.assertEqual(environment.git_state(self.repo, self.out)[1], "dirty")

    def test_unrelated_untracked_file_is_still_caught(self):
        (self.repo / "stray.txt").write_text("x\n")
        self.assertEqual(environment.git_state(self.repo, self.out)[1], "dirty")

    def test_out_dir_outside_the_repo_is_tolerated(self):
        outside = pathlib.Path(tempfile.mkdtemp(prefix="qledger-out-"))
        self.assertEqual(environment.git_state(self.repo, outside)[1], "dirty")  # submission/ shows



class TestPmuPartialRepetitions(unittest.TestCase):
    """One transient perf_event_open failure in ten repetitions must not discard the other nine."""

    @staticmethod
    def _rep(ipc, bmiss):
        return gbench.BenchResult(name="x", real_time_ns=1.0, items_per_second=None,
                                  bytes_per_second=None, cycles_per_value=None,
                                  ipc=ipc, branch_miss_pct=bmiss)

    def test_partial_sample_is_kept_and_labelled(self):
        reps = [self._rep(3.5, 0.002) for _ in range(9)] + [self._rep(None, None)]
        got = quiver_ledger.build_pmu(reps, [], seed=1)
        self.assertEqual(got["status"], "partial")
        self.assertEqual(got["repetitions_measured"], 9)
        self.assertAlmostEqual(got["ipc"]["median"], 3.5)

    def test_full_sample_is_available_not_partial(self):
        got = quiver_ledger.build_pmu([self._rep(3.5, 0.002) for _ in range(10)], [], seed=1)
        self.assertEqual(got["status"], "available")
        self.assertEqual(got["repetitions_measured"], 10)

    def test_no_counters_at_all_is_unavailable(self):
        got = quiver_ledger.build_pmu([self._rep(None, None) for _ in range(10)], [], seed=1)
        self.assertEqual(got, {"status": "unavailable"})


class TestEreClosingDelimiters(unittest.TestCase):
    """Google Benchmark rejects \\] and \\} as invalid escapes, exactly like \\-. They are
    metacharacters only inside a bracket expression or interval, which a literal never opens."""

    def test_closing_delimiters_are_not_escaped(self):
        self.assertEqual(gbench.ere_escape("a]b}c"), "a]b}c")

    def test_opening_delimiters_are_still_escaped(self):
        self.assertEqual(gbench.ere_escape("a[b{c"), "a\\[b\\{c")


class TestRunIdentity(unittest.TestCase):
    """entry_id must key off a canonical run identity, never the output directory name.

    community-run writes to the literal `submission`, so directory-derived ids made every bundle
    from one machine collide — and because ids are baked into entries.json, renaming the directory
    at append time cannot repair them. Identity is `<date>-<commit>`: reproducible for a given
    commit and day, and independent of --output.
    """

    def test_identity_is_date_and_commit(self):
        rid = quiver_ledger.run_identity("abc123def456")
        self.assertTrue(rid.endswith("-abc123def456"))
        self.assertRegex(rid, r"^\d{8}-abc123def456$")

    def test_identity_ignores_output_directory(self):
        """Two submissions from the same machine at the SAME commit and day are the same run."""
        self.assertEqual(quiver_ledger.run_identity("deadbeef"),
                         quiver_ledger.run_identity("deadbeef"))

    def test_different_commits_are_different_runs(self):
        """Distinct commits must never share an identity, whatever the directory is called."""
        self.assertNotEqual(quiver_ledger.run_identity("aaaaaaaaaaaa"),
                            quiver_ledger.run_identity("bbbbbbbbbbbb"))

    def test_canonical_destination_is_named_for_the_identity(self):
        args = argparse.Namespace(out=None)
        machine = {"uarch_dir": "intel-coffee-lake"}
        out = quiver_ledger.resolve_out_dir(args, machine, "20260730-deadbeef")
        self.assertEqual(out.name, "20260730-deadbeef")
        self.assertEqual(out.parent.name, "intel-coffee-lake")

    def test_explicit_output_moves_files_but_not_identity(self):
        args = argparse.Namespace(out="/tmp/submission")
        out = quiver_ledger.resolve_out_dir(args, {"uarch_dir": "x"}, "20260730-deadbeef")
        self.assertEqual(out, pathlib.Path("/tmp/submission"))  # only the location changed


class TestCrossRunCollisions(unittest.TestCase):
    """Uniqueness is a property of the whole ledger; validate_run_dir only ever sees one dir."""

    @staticmethod
    def _run_dir(root: pathlib.Path, name: str, ids: list[str]) -> pathlib.Path:
        d = root / name
        d.mkdir(parents=True)
        (d / "entries.json").write_text(json.dumps([{"entry_id": i} for i in ids]))
        return d

    def test_distinct_identities_do_not_collide(self):
        root = pathlib.Path(tempfile.mkdtemp())
        a = self._run_dir(root, "a", ["uarch-20260730-aaaa-bm-x"])
        b = self._run_dir(root, "b", ["uarch-20260731-bbbb-bm-x"])
        self.assertEqual(quiver_ledger.cross_run_id_collisions([a, b]), [])

    def test_genuine_collision_is_reported(self):
        root = pathlib.Path(tempfile.mkdtemp())
        a = self._run_dir(root, "a", ["uarch-20260730-aaaa-bm-x"])
        b = self._run_dir(root, "b", ["uarch-20260730-aaaa-bm-x"])
        problems = quiver_ledger.cross_run_id_collisions([a, b])
        self.assertEqual(len(problems), 1)
        self.assertIn("already published by", problems[0])

    def test_malformed_entries_do_not_crash_the_sweep(self):
        root = pathlib.Path(tempfile.mkdtemp())
        good = self._run_dir(root, "good", ["uarch-20260730-aaaa-bm-x"])
        bad = root / "bad"
        bad.mkdir()
        (bad / "entries.json").write_text("{ truncated")
        self.assertEqual(quiver_ledger.cross_run_id_collisions([good, bad]), [])


class TestValidateRunDirIsTotal(unittest.TestCase):
    """`validate` is the gate a reviewer runs on someone else's bundle: a malformed artifact must
    be a reported problem, never an uncaught traceback."""

    def _dir(self, **files) -> pathlib.Path:
        d = pathlib.Path(tempfile.mkdtemp()) / "run"
        d.mkdir()
        for name, body in files.items():
            (d / name.replace("_", ".", 1)).write_text(body)
        return d

    def test_missing_artifacts(self):
        problems = quiver_ledger.validate_run_dir(self._dir())
        self.assertTrue(any("missing" in p for p in problems))

    def test_invalid_json(self):
        problems = quiver_ledger.validate_run_dir(self._dir(manifest_json="{ truncated"))
        self.assertTrue(any("invalid JSON" in p for p in problems))

    def test_wrong_top_level_type(self):
        problems = quiver_ledger.validate_run_dir(self._dir(manifest_json='["list"]'))
        self.assertTrue(any("expected dict" in p for p in problems))

    def test_artifact_is_a_directory(self):
        d = self._dir()
        (d / "manifest.json").mkdir()
        self.assertTrue(any("not a regular file" in p
                            for p in quiver_ledger.validate_run_dir(d)))

    def test_entries_items_not_objects(self):
        d = self._dir(manifest_json=json.dumps({"deviations": []}),
                      entries_json='[1, "two", null]')
        problems = quiver_ledger.validate_run_dir(d)
        self.assertEqual(sum("expected object" in p for p in problems), 3)


if __name__ == "__main__":
    unittest.main()
