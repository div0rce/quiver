"""Environment checklist and manifest capture (REQ-LEDGER-003/-013).

Publishable runs require: clean git tree, performance governor where the platform has one,
and a registered machine file. Anything else is a recorded deviation; deviations make the
run non-publishable (the runner proceeds only with --allow-deviations, for local
investigation). Stdlib only (REQ-INT-004).
"""

from __future__ import annotations

import dataclasses
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


def git_state(repo: pathlib.Path, out_dir: pathlib.Path | None = None) -> tuple[str, str]:
    """(short commit, 'clean'|'dirty'). The runner's own output is excluded — a run must not flag
    itself dirty by writing its results. `ledger/results/` covers the default destination;
    `out_dir` covers an explicit `--output` (community-run defaults to ./submission, inside the
    repo). Everything else, including untracked source files, still counts: provenance."""
    sha = _run(["git", "-C", str(repo), "rev-parse", "--short=12", "HEAD"]) or "unknown"
    specs = [".", ":(exclude)ledger/results"]
    if out_dir is not None:
        try:
            specs.append(f":(exclude){out_dir.resolve().relative_to(repo.resolve()).as_posix()}")
        except ValueError:
            pass  # output lives outside the repo: nothing to exclude
    status = _run(["git", "-C", str(repo), "status", "--porcelain", "--", *specs])
    return sha, ("dirty" if status else "clean")


def _read_sysfs(path: pathlib.Path) -> str | None:
    """Stripped contents of a sysfs/procfs knob, or None when it is absent or unreadable — the
    four readers below all mean the same thing by both: the value could not be recorded."""
    try:
        return path.read_text().strip()
    except OSError:
        return None


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
    gov = _read_sysfs(pathlib.Path("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"))
    if gov is not None:
        return gov
    if platform.system() == "Darwin":
        return "n/a (macOS: OS-managed DVFS, no user governor)"
    return "unknown"


def smt_state() -> str:
    smt = _read_sysfs(pathlib.Path("/sys/devices/system/cpu/smt/control"))
    if smt is not None:
        return smt
    if platform.system() == "Darwin":
        return "n/a (Apple Silicon has no SMT)"
    return "unknown"


def turbo_state(machine: dict[str, Any]) -> str:
    """Measured turbo/boost state, not the machine file's intended policy. Copying the declared
    policy meant a manifest kept asserting turbo was disabled after a reboot or a power-profile
    daemon silently re-enabled it — the same way power-profiles-daemon reverts the governor."""
    for path, on, off in ((pathlib.Path("/sys/devices/system/cpu/intel_pstate/no_turbo"),
                           "0", "1"),
                          (pathlib.Path("/sys/devices/system/cpu/cpufreq/boost"), "1", "0")):
        value = _read_sysfs(path)
        if value is not None:
            state = {on: "enabled", off: "disabled"}.get(value, f"unknown ({value})")
            return f"{state} (measured: {path.name}={value})"
    return machine.get("turbo_boost", "platform-managed")  # macOS/Arm: no user control to read


def aslr_state() -> str:
    value = _read_sysfs(pathlib.Path("/proc/sys/kernel/randomize_va_space"))
    if value is not None:
        return {"0": "off", "1": "conservative", "2": "full"}.get(value, "unknown")
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


@dataclasses.dataclass(frozen=True)
class RunContext:
    """What a manifest needs to know about the run that produced it. Grouped because these
    always travel together, and a seven-argument call site invites positional mistakes."""
    repo: pathlib.Path
    build_dir: pathlib.Path
    out_dir: pathlib.Path
    machine: dict[str, Any]
    gb_version: str
    repetition_seed: int


def build_manifest(ctx: RunContext, deviations: list[str]) -> dict[str, Any]:
    """Dirtiness is re-checked here, AFTER the run, so a tracked file edited mid-run is still
    caught — a ~50-minute run is long enough for that to happen. `out_dir` is excluded so the
    run's own output cannot mark it dirty."""
    sha, dirty = git_state(ctx.repo, ctx.out_dir)
    comp, flags, lto = compiler_from_cache(ctx.build_dir)
    devs = list(deviations)
    if dirty == "dirty":
        devs = devs if "git tree dirty" in devs else [*devs, "git tree dirty"]
    return {
        "cpu_model": cpu_model(),
        "machine_id": ctx.machine["machine_id"],
        "uarch": ctx.machine["uarch"],
        "core_used": ctx.machine.get("core_policy", "OS-scheduled (no pinning control)"),
        "pinning": ctx.machine.get("pinning", "none"),
        "frequency_governor": frequency_governor(),
        "turbo_boost": turbo_state(ctx.machine),
        "smt": smt_state(),
        "aslr": aslr_state(),
        "os": f"{platform.system()} {platform.release()}",
        "kernel": platform.version(),
        "compiler": comp,
        "compiler_flags": flags,
        "lto": lto,
        "google_benchmark_version": ctx.gb_version,
        "library_commit": sha,
        "timestamp_utc": datetime.datetime.now(datetime.UTC).isoformat(timespec="seconds"),
        "repetition_seed": ctx.repetition_seed,
        "deviations": devs,
    }


def load_machine(repo: pathlib.Path, machine_id: str) -> dict[str, Any]:
    path = repo / "ledger" / "machines" / f"{machine_id}.json"
    if not path.exists():
        raise FileNotFoundError(
            f"machine '{machine_id}' is not registered ({path} missing) — ledger runs execute "
            f"only on registered machines (REQ-LEDGER-007)")
    return json.loads(path.read_text())
