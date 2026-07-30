"""ADR-020 statistics: median, min, seeded percentile-bootstrap 95% CIs, CV.

First-party, Python stdlib only (REQ-INT-004). The bootstrap is the SEEDED percentile
bootstrap (B = 10,000 by default): resample-with-replacement the repetition vector, compute
the estimator per resample, report the 2.5/97.5 percentiles of the resample distribution.
Deterministic for a given (values, seed) — the seed is recorded per entry (REQ-LEDGER-004).
Golden tests pin exact outputs against committed fixtures (PRD 05 §9).
"""

from __future__ import annotations

import random
import statistics
from dataclasses import dataclass

BOOTSTRAP_B = 10_000
CI_LO_PCT = 2.5
CI_HI_PCT = 97.5


@dataclass(frozen=True)
class MetricStats:
    """Aggregates for one metric across process-level repetitions (REQ-LEDGER-002)."""

    median: float
    min: float
    ci95_lo: float
    ci95_hi: float
    cv: float


def percentile(sorted_values: list[float], pct: float) -> float:
    """Linear-interpolation percentile on a pre-sorted list (the classic 'exclusive' method
    is deliberately avoided: this matches statistics.quantiles(n=..., method='inclusive')
    behavior and is pinned by golden tests)."""
    if not sorted_values:
        raise ValueError("percentile of empty list")
    if len(sorted_values) == 1:
        return sorted_values[0]
    rank = (pct / 100.0) * (len(sorted_values) - 1)
    lo = int(rank)
    hi = min(lo + 1, len(sorted_values) - 1)
    frac = rank - lo
    return sorted_values[lo] * (1.0 - frac) + sorted_values[hi] * frac


_ESTIMATORS = {"median": statistics.median, "min": min}


def bootstrap_ci(
    values: list[float],
    estimator: str,
    seed: int,
    b: int = BOOTSTRAP_B,
) -> tuple[float, float]:
    """Seeded percentile bootstrap for 'median' or 'min' (ADR-020).

    The estimator is resolved once through a table rather than branched per resample: the RNG
    draw order is what makes the interval reproducible, and it is unchanged by the lookup.
    """
    stat = _ESTIMATORS.get(estimator)
    if stat is None:
        raise ValueError(f"unknown estimator: {estimator}")
    if not values:
        raise ValueError("bootstrap of empty list")
    rng = random.Random(seed)
    n = len(values)
    resample_stats = sorted(
        stat([values[rng.randrange(n)] for _ in range(n)]) for _ in range(b))
    return (
        percentile(resample_stats, CI_LO_PCT),
        percentile(resample_stats, CI_HI_PCT),
    )


def coefficient_of_variation(values: list[float]) -> float:
    """CV = stddev/mean of the repetitions — stability screening only, never the headline
    statistic (ADR-020)."""
    if len(values) < 2:
        return 0.0
    mean = statistics.fmean(values)
    if mean == 0.0:
        return 0.0
    return statistics.stdev(values) / mean


def metric_stats(values: list[float], seed: int, b: int = BOOTSTRAP_B) -> dict[str, MetricStats]:
    """Both ADR-020 estimators for one metric's repetition vector, keyed 'median'/'min'.

    Both use the same recorded seed; the two bootstraps draw from independent Random
    instances so adding one never perturbs the other.
    """
    med_lo, med_hi = bootstrap_ci(values, "median", seed, b)
    min_lo, min_hi = bootstrap_ci(values, "min", seed + 1, b)
    cv = coefficient_of_variation(values)
    return {
        "median": MetricStats(
            median=statistics.median(values),
            min=min(values),
            ci95_lo=med_lo,
            ci95_hi=med_hi,
            cv=cv,
        ),
        "min": MetricStats(
            median=statistics.median(values),
            min=min(values),
            ci95_lo=min_lo,
            ci95_hi=min_hi,
            cv=cv,
        ),
    }


def noise_flags(cv: float) -> tuple[list[str], bool]:
    """REQ-LEDGER-005 noise policy: (flags, publishable)."""
    if cv > 0.05:
        return (["noisy"], False)  # excluded from publication until rerun
    if cv > 0.03:
        return (["noisy"], True)  # published flagged, with a notes explanation
    return ([], True)
