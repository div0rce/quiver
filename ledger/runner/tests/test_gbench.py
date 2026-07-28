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

import json
import pathlib
import re
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from qledger import gbench  # noqa: E402

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
        for ch in ".^$*+?()[]{}|\\":
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


if __name__ == "__main__":
    unittest.main()
