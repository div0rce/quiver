"""Golden tests for the ADR-020 statistics implementation (PRD 05 §9, PRD 11 §10).

The bootstrap is SEEDED: for a fixed (values, seed, B) the CIs are exact constants. These
goldens pin the committed implementation — any change to the estimator mechanics (resampling
scheme, percentile method, RNG) fails here and is a QLM version event (REQ-LEDGER-014).
"""

import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from qledger import stats  # noqa: E402

GOLDEN_VALUES = [105.0, 98.0, 101.5, 99.2, 100.1, 97.8, 102.3, 100.9, 99.5, 103.4]
GOLDEN_SEED = 42


class TestPercentile(unittest.TestCase):
    def test_linear_interpolation(self):
        s = sorted(GOLDEN_VALUES)
        self.assertAlmostEqual(stats.percentile(s, 2.5), 97.845, places=10)
        self.assertAlmostEqual(stats.percentile(s, 97.5), 104.64, places=10)

    def test_single_value(self):
        self.assertEqual(stats.percentile([7.0], 50.0), 7.0)

    def test_empty_raises(self):
        with self.assertRaises(ValueError):
            stats.percentile([], 50.0)


class TestBootstrapGolden(unittest.TestCase):
    def test_median_estimator_golden(self):
        ms = stats.metric_stats(GOLDEN_VALUES, seed=GOLDEN_SEED)["median"]
        self.assertEqual(ms.median, 100.5)
        self.assertEqual(ms.min, 97.8)
        self.assertAlmostEqual(ms.ci95_lo, 99.05, places=10)
        self.assertAlmostEqual(ms.ci95_hi, 102.45, places=10)
        self.assertAlmostEqual(ms.cv, 0.02308902444447875, places=15)

    def test_min_estimator_golden(self):
        ms = stats.metric_stats(GOLDEN_VALUES, seed=GOLDEN_SEED)["min"]
        self.assertAlmostEqual(ms.ci95_lo, 97.8, places=10)
        self.assertAlmostEqual(ms.ci95_hi, 99.5, places=10)

    def test_seed_determinism(self):
        # Same seed => identical CIs (the reproducibility contract). Different seeds may
        # legitimately coincide (median CIs quantize at R = 10), so no inequality is asserted.
        a = stats.bootstrap_ci(GOLDEN_VALUES, "median", seed=7)
        b = stats.bootstrap_ci(GOLDEN_VALUES, "median", seed=7)
        self.assertEqual(a, b)

    def test_unknown_estimator_rejected(self):
        with self.assertRaises(ValueError):
            stats.bootstrap_ci(GOLDEN_VALUES, "mean", seed=1)


class TestNoisePolicy(unittest.TestCase):
    def test_bands(self):
        self.assertEqual(stats.noise_flags(0.01), ([], True))
        self.assertEqual(stats.noise_flags(0.04), (["noisy"], True))
        self.assertEqual(stats.noise_flags(0.08), (["noisy"], False))


if __name__ == "__main__":
    unittest.main()
