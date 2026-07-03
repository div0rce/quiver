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
    raw: dict[str, Any] = field(repr=False, default_factory=dict)


def _to_ns(value: float, unit: str) -> float:
    scale = {"ns": 1.0, "us": 1e3, "ms": 1e6, "s": 1e9}.get(unit)
    if scale is None:
        raise GBParseError(f"unknown time_unit '{unit}' in GB JSON")
    return value * scale


def parse_gb_json(text: str) -> list[BenchResult]:
    try:
        doc = json.loads(text)
    except json.JSONDecodeError as e:
        raise GBParseError(f"GB output is not valid JSON: {e}") from e
    if "benchmarks" not in doc:
        raise GBParseError("GB JSON missing 'benchmarks' array")
    out: list[BenchResult] = []
    for b in doc["benchmarks"]:
        for req in ("name", "real_time", "time_unit"):
            if req not in b:
                raise GBParseError(f"GB benchmark record missing '{req}'")
        out.append(BenchResult(
            name=b["name"],
            real_time_ns=_to_ns(float(b["real_time"]), b["time_unit"]),
            items_per_second=float(b["items_per_second"]) if "items_per_second" in b else None,
            bytes_per_second=float(b["bytes_per_second"]) if "bytes_per_second" in b else None,
            cycles_per_value=float(b["cycles_per_value"]) if "cycles_per_value" in b else None,
            raw=b,
        ))
    return out


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


def run_one(binary: pathlib.Path, bench_name: str, env_cap: str | None,
            min_time: str) -> tuple[str, list[BenchResult]]:
    """One fresh-process run of exactly one benchmark; returns (raw JSON text, parsed)."""
    env = _env_with_cap(env_cap)
    pattern = "^" + re.escape(bench_name) + "$"
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
