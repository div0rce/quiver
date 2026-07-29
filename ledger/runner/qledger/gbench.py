"""Google Benchmark subprocess driver and JSON parsing (REQ-LEDGER-006, PRD 11 §7).

Each repetition of each benchmark is a FRESH PROCESS; the run order across benchmarks is
shuffled per repetition with a recorded seed (interleaving defense). GB JSON drift is a
versioned error naming the offending field — no silent tolerance beyond documented
ignore-unknown-fields. Stdlib only (REQ-INT-004).
"""

from __future__ import annotations

import json
import pathlib
import re
import subprocess
from dataclasses import dataclass, field
from typing import Any

# Variant -> QUIVER_ISA env cap. The bench binary registers names for the ACTIVE tier, so
# capping the process selects both the code path and the registered variant name
# (REQ-BENCH-002/-010: on ARM the scalar cap registers as `autovec`).
VARIANT_ENV = {
    "default": None,       # the machine's best tier (neon / avx2 / avx512)
    "baseline": "scalar",  # registers as `scalar` on x86, `autovec` on ARM
}


class GBParseError(RuntimeError):
    """QLS-aware GB JSON drift error (PRD 11 §7): names the offending field."""


@dataclass
class BenchResult:
    name: str
    real_time_ns: float
    items_per_second: float | None
    bytes_per_second: float | None
    cycles_per_value: float | None
    # REQ-BENCH-005 reports three PMU-derived counters when perf_event_open succeeds; all three
    # are None on a machine or kernel without PMU access, and the entry records that as
    # `pmu: unavailable` rather than silently omitting the columns.
    ipc: float | None = None
    branch_miss_pct: float | None = None
    raw: dict[str, Any] = field(repr=False, default_factory=dict)


def _to_ns(value: float, unit: str) -> float:
    scale = {"ns": 1.0, "us": 1e3, "ms": 1e6, "s": 1e9}.get(unit)
    if scale is None:
        raise GBParseError(f"unknown time_unit '{unit}' in GB JSON")
    return value * scale


def _optional_float(record: dict[str, Any], key: str) -> float | None:
    """None means the counter was absent, which must stay distinct from a measured 0.0."""
    return float(record[key]) if key in record else None


def _one_result(record: dict[str, Any]) -> BenchResult:
    for required in ("name", "real_time", "time_unit"):
        if required not in record:
            raise GBParseError(f"GB benchmark record missing '{required}'")
    return BenchResult(
        name=record["name"],
        real_time_ns=_to_ns(float(record["real_time"]), record["time_unit"]),
        items_per_second=_optional_float(record, "items_per_second"),
        bytes_per_second=_optional_float(record, "bytes_per_second"),
        cycles_per_value=_optional_float(record, "cycles_per_value"),
        ipc=_optional_float(record, "ipc"),
        branch_miss_pct=_optional_float(record, "branch_miss_pct"),
        raw=record,
    )


def parse_gb_json(text: str) -> list[BenchResult]:
    try:
        doc = json.loads(text)
    except json.JSONDecodeError as e:
        raise GBParseError(f"GB output is not valid JSON: {e}") from e
    if "benchmarks" not in doc:
        raise GBParseError("GB JSON missing 'benchmarks' array")
    return [_one_result(record) for record in doc["benchmarks"]]


def gb_version(doc_text: str) -> str:
    try:
        ctx = json.loads(doc_text).get("context", {})
        return str(ctx.get("library_version", ctx.get("library_build_type", "unknown")))
    except json.JSONDecodeError:
        return "unknown"


def list_benchmarks(binary: pathlib.Path, env_cap: str | None) -> list[str]:
    env = _env_with_cap(env_cap)
    proc = subprocess.run([str(binary), "--benchmark_list_tests"], capture_output=True,
                          text=True, env=env, check=False)
    if proc.returncode != 0:
        raise RuntimeError(f"{binary.name} --benchmark_list_tests failed: {proc.stderr[-400:]}")
    return [ln.strip() for ln in proc.stdout.splitlines() if ln.strip().startswith("BM_")]


# Google Benchmark compiles --benchmark_filter as a POSIX ERE, where a backslash before a
# non-special character is undefined. Python's re.escape escapes '-' as '\-', which GB rejects with
# "Could not compile benchmark re: Invalid escape in regular expression" — it then exits 0 having
# printed nothing, so the failure surfaces as a JSON parse error rather than a filter error. Only
# ERE metacharacters may be escaped; '-' is literal outside a bracket expression. This is reachable
# on any variant whose name contains a hyphen (`autovec-avx2`, `autovec-avx512`), i.e. every x86
# baseline (REQ-BENCH-002 / ADR-011).
# `]` and `}` are omitted deliberately: they are metacharacters only *inside* a bracket
# expression or interval, which escaping a literal never opens, and Google Benchmark rejects
# `\]` / `\}` as invalid escapes exactly like `\-` — the failure this function exists to avoid.
_ERE_METACHARS = frozenset(".^$*+?()[{|\\")


def ere_escape(text: str) -> str:
    """Escape `text` for a POSIX ERE, touching only characters that are actually metacharacters."""
    return "".join("\\" + ch if ch in _ERE_METACHARS else ch for ch in text)


def run_one(binary: pathlib.Path, bench_name: str, env_cap: str | None,
            min_time: str) -> tuple[str, list[BenchResult]]:
    """One fresh-process run of exactly one benchmark; returns (raw JSON text, parsed)."""
    env = _env_with_cap(env_cap)
    pattern = "^" + ere_escape(bench_name) + "$"
    proc = subprocess.run(
        [str(binary), f"--benchmark_filter={pattern}", "--benchmark_format=json",
         f"--benchmark_min_time={min_time}"],
        capture_output=True, text=True, env=env, check=False)
    if proc.returncode != 0:
        # Bench validation aborts stop the whole run: no partial entry (PRD 11 §7).
        raise RuntimeError(
            f"benchmark process failed ({binary.name} / {bench_name}): {proc.stderr[-400:]}")
    return proc.stdout, parse_gb_json(proc.stdout)


def _env_with_cap(cap: str | None) -> dict[str, str]:
    import os
    env = dict(os.environ)
    env.pop("QUIVER_ISA", None)
    if cap is not None:
        env["QUIVER_ISA"] = cap
    return env
