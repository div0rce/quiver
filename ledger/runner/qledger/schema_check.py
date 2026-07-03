"""First-party structural validation for QLS-1 entries and manifests (REQ-LEDGER-001).

The JSON Schema files under ledger/schema/ are the normative Surface D documents; this
module is the stdlib-only checker the runner and CI use (REQ-INT-004 forbids third-party
validators). It intentionally checks the same constraints the schemas state — required
fields, types, enums — and the test suite carries accept/reject fixtures for both.
"""

from __future__ import annotations

from typing import Any

SCHEMA_VERSION = "QLS-1"
METHODOLOGY_VERSION = "QLM-1"

ENTRY_REQUIRED: dict[str, type | tuple[type, ...]] = {
    "entry_id": str,
    "schema": str,
    "methodology": str,
    "benchmark": str,
    "family": str,
    "api": str,
    "variant": str,
    "element_type": str,
    "axes": dict,
    "machine_id": str,
    "manifest_ref": str,
    "library_version": str,
    "git_commit": str,
    "git_dirty": str,
    "timestamp_utc": str,
    "repetitions": int,
    "results": dict,
    "metrics": list,
    "flags": list,
    "notes": str,
}

VALID_VARIANTS = {"scalar", "autovec", "autovec-avx2", "autovec-avx512", "neon", "avx2", "avx512"}
VALID_FLAGS = {"noisy", "no_pmu", "secondary_platform"}
VALID_METRICS = {"ns_per_batch", "values_per_s", "bytes_per_s", "cycles_per_value"}
RESULT_FIELDS = {"median", "min", "ci95_lo", "ci95_hi", "cv"}

MANIFEST_REQUIRED: dict[str, type | tuple[type, ...]] = {
    "cpu_model": str,
    "machine_id": str,
    "uarch": str,
    "core_used": str,
    "pinning": str,
    "frequency_governor": str,
    "turbo_boost": str,
    "smt": str,
    "aslr": str,
    "os": str,
    "kernel": str,
    "compiler": str,
    "compiler_flags": str,
    "lto": str,
    "google_benchmark_version": str,
    "library_commit": str,
    "timestamp_utc": str,
    "deviations": list,
}


def _type_errors(obj: dict[str, Any], spec: dict[str, type | tuple[type, ...]],
                 what: str) -> list[str]:
    errors = []
    for field, ftype in spec.items():
        if field not in obj:
            errors.append(f"{what}: missing required field '{field}'")
        elif not isinstance(obj[field], ftype):
            errors.append(f"{what}: field '{field}' has type {type(obj[field]).__name__}, "
                          f"expected {ftype}")
    return errors


def validate_entry(entry: dict[str, Any]) -> list[str]:
    """Returns a list of violations; empty means valid (QLS-1)."""
    errors = _type_errors(entry, ENTRY_REQUIRED, "entry")
    if errors:
        return errors
    if entry["schema"] != SCHEMA_VERSION:
        errors.append(f"entry: schema is '{entry['schema']}', expected '{SCHEMA_VERSION}'")
    if entry["methodology"] != METHODOLOGY_VERSION:
        errors.append(f"entry: methodology is '{entry['methodology']}', "
                      f"expected '{METHODOLOGY_VERSION}'")
    if entry["variant"] not in VALID_VARIANTS:
        errors.append(f"entry: variant '{entry['variant']}' not in {sorted(VALID_VARIANTS)}")
    for flag in entry["flags"]:
        if flag not in VALID_FLAGS:
            errors.append(f"entry: unknown flag '{flag}'")
    for metric in entry["metrics"]:
        if metric not in VALID_METRICS:
            errors.append(f"entry: unknown metric '{metric}'")
        elif metric not in entry["results"]:
            errors.append(f"entry: metric '{metric}' listed but absent from results")
    for metric, res in entry["results"].items():
        if metric not in VALID_METRICS:
            errors.append(f"entry: results contain unknown metric '{metric}'")
            continue
        if not isinstance(res, dict):
            errors.append(f"entry: results['{metric}'] is not an object")
            continue
        missing = RESULT_FIELDS - res.keys()
        if missing:
            errors.append(f"entry: results['{metric}'] missing {sorted(missing)}")
        for k in RESULT_FIELDS & res.keys():
            if not isinstance(res[k], (int, float)):
                errors.append(f"entry: results['{metric}']['{k}'] is not a number")
    if entry["repetitions"] < 10:
        errors.append("entry: repetitions < 10 are unpublishable (REQ-LEDGER-004)")
    if entry["git_dirty"] not in ("clean", "dirty"):
        errors.append("entry: git_dirty must be 'clean' or 'dirty'")
    # pmu is nullable; when flags say no_pmu, cycles_per_value must be absent (REQ-LEDGER-008)
    if "no_pmu" in entry["flags"] and "cycles_per_value" in entry["results"]:
        errors.append("entry: no_pmu flag with cycles_per_value present (REQ-LEDGER-008)")
    return errors


def validate_manifest(manifest: dict[str, Any]) -> list[str]:
    errors = _type_errors(manifest, MANIFEST_REQUIRED, "manifest")
    if errors:
        return errors
    for d in manifest["deviations"]:
        if not isinstance(d, str):
            errors.append("manifest: deviations must be strings")
    return errors
