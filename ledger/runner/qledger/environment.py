"""Environment checklist and manifest capture (REQ-LEDGER-003/-013).

Publishable runs require: clean git tree, performance governor where the platform has one,
and a registered machine file. Anything else is a recorded deviation; deviations make the
run non-publishable (the runner proceeds only with --allow-deviations, for local
investigation). Stdlib only (REQ-INT-004).
"""

from __future__ import annotations

import datetime
import json
import pathlib
import platform
import subprocess
from typing import Any


def _run(cmd: list[str]) -> str:
    try:
        return subprocess.run(cmd, capture_output=True, text=True, check=False).stdout.strip()
    except OSError:
        return ""


def git_state(repo: pathlib.Path) -> tuple[str, str]:
    """(short commit, 'clean'|'dirty'). The runner's own output under ledger/results/ is
    excluded — a run must not flag itself dirty by writing its results (everything else,
    including untracked source files, still counts: provenance)."""
    sha = _run(["git", "-C", str(repo), "rev-parse", "--short=12", "HEAD"]) or "unknown"
    status = _run(["git", "-C", str(repo), "status", "--porcelain", "--",
                   ".", ":(exclude)ledger/results"])
    return sha, ("dirty" if status else "clean")


def cpu_model() -> str:
    if platform.system() == "Darwin":
        return _run(["sysctl", "-n", "machdep.cpu.brand_string"]) or platform.processor()
    try:
        for line in pathlib.Path("/proc/cpuinfo").read_text().splitlines():
            if line.lower().startswith("model name"):
                return line.split(":", 1)[1].strip()
    except OSError:
        pass
    return platform.processor() or platform.machine()


def frequency_governor() -> str:
    """Linux: the cpufreq governor (must be 'performance' for publishable runs).
    macOS exposes no governor control; that is the platform's normal state, recorded as such
    (not a deviation) — Apple entries are secondary-platform anyway (REQ-LEDGER-008)."""
    gov_path = pathlib.Path("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor")
    if gov_path.exists():
        try:
            return gov_path.read_text().strip()
        except OSError:
            return "unreadable"
    if platform.system() == "Darwin":
        return "n/a (macOS: OS-managed DVFS, no user governor)"
    return "unknown"


def smt_state() -> str:
    smt = pathlib.Path("/sys/devices/system/cpu/smt/control")
    if smt.exists():
        try:
            return smt.read_text().strip()
        except OSError:
            return "unreadable"
    if platform.system() == "Darwin":
        return "n/a (Apple Silicon has no SMT)"
    return "unknown"


def aslr_state() -> str:
    p = pathlib.Path("/proc/sys/kernel/randomize_va_space")
    if p.exists():
        try:
            return {"0": "off", "1": "conservative", "2": "full"}.get(
                p.read_text().strip(), "unknown")
        except OSError:
            return "unreadable"
    if platform.system() == "Darwin":
        return "on (macOS default, not user-controllable per-process here)"
    return "unknown"


def compiler_from_cache(build_dir: pathlib.Path) -> tuple[str, str, str]:
    """(compiler id+version, CXX flags for the active config, LTO state) from CMakeCache."""
    cache = build_dir / "CMakeCache.txt"
    values: dict[str, str] = {}
    if cache.exists():
        for line in cache.read_text().splitlines():
            if "=" in line and ":" in line.split("=", 1)[0]:
                key = line.split(":", 1)[0]
                values[key] = line.split("=", 1)[1]
    cxx = values.get("CMAKE_CXX_COMPILER", "unknown")
    version = _run([cxx, "--version"]).splitlines()[0] if cxx != "unknown" else "unknown"
    build_type = values.get("CMAKE_BUILD_TYPE", "unknown")
    flags = (values.get("CMAKE_CXX_FLAGS", "") + " "
             + values.get(f"CMAKE_CXX_FLAGS_{build_type.upper()}", "")).strip() or "(defaults)"
    lto = values.get("CMAKE_INTERPROCEDURAL_OPTIMIZATION", "OFF") or "OFF"
    return version, f"{flags} [{build_type}]", lto


def environment_checklist(repo: pathlib.Path, machine: dict[str, Any]) -> list[str]:
    """Returns deviations (empty == publishable), per REQ-LEDGER-013."""
    deviations: list[str] = []
    _, dirty = git_state(repo)
    if dirty == "dirty":
        deviations.append("git tree dirty")
    gov = frequency_governor()
    if not (gov == "performance" or gov.startswith("n/a")):
        deviations.append(f"frequency governor is '{gov}', not 'performance'")
    if machine.get("machine_id", "") == "":
        deviations.append("machine file missing machine_id")
    return deviations


def build_manifest(repo: pathlib.Path, build_dir: pathlib.Path, machine: dict[str, Any],
                   gb_version: str, repetition_seed: int,
                   deviations: list[str]) -> dict[str, Any]:
    sha, dirty = git_state(repo)
    comp, flags, lto = compiler_from_cache(build_dir)
    devs = list(deviations)
    if dirty == "dirty":
        devs = devs if "git tree dirty" in devs else [*devs, "git tree dirty"]
    return {
        "cpu_model": cpu_model(),
        "machine_id": machine["machine_id"],
        "uarch": machine["uarch"],
        "core_used": machine.get("core_policy", "OS-scheduled (no pinning control)"),
        "pinning": machine.get("pinning", "none"),
        "frequency_governor": frequency_governor(),
        "turbo_boost": machine.get("turbo_boost", "platform-managed"),
        "smt": smt_state(),
        "aslr": aslr_state(),
        "os": f"{platform.system()} {platform.release()}",
        "kernel": platform.version(),
        "compiler": comp,
        "compiler_flags": flags,
        "lto": lto,
        "google_benchmark_version": gb_version,
        "library_commit": sha,
        "timestamp_utc": datetime.datetime.now(datetime.UTC).isoformat(timespec="seconds"),
        "repetition_seed": repetition_seed,
        "deviations": devs,
    }


def load_machine(repo: pathlib.Path, machine_id: str) -> dict[str, Any]:
    path = repo / "ledger" / "machines" / f"{machine_id}.json"
    if not path.exists():
        raise FileNotFoundError(
            f"machine '{machine_id}' is not registered ({path} missing) — ledger runs execute "
            f"only on registered machines (REQ-LEDGER-007)")
    return json.loads(path.read_text())
