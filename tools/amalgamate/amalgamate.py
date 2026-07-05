#!/usr/bin/env python3
"""MOD-AMALG — deterministic single-file amalgamation generator (ADR-018, REQ-BUILD-013).

Emits the drop-in pair ``quiver.h`` + ``quiver.cpp`` by rule-based text transformation only —
no C++ parsing (REQ-INT-005 determinism rests on this staying trivial, which in turn rests on
the source obeying the REQ-STD-006 amalgamation-compatibility rules). ``--check`` lints those
rules so convention drift surfaces as a lint failure, never as a malformed amalgamation.

Determinism (REQ-INT-005): two runs on an identical tree produce byte-identical output. The
only tree-varying token is the ``git describe`` version stamp; every collection is emitted in a
fixed or stable-sorted order and no timestamps/absolute paths are written.

Usage:
  amalgamate.py --out-dir DIR    generate DIR/quiver.h and DIR/quiver.cpp
  amalgamate.py --check          lint REQ-STD-006 rules; exit non-zero on any violation
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# --- ADR-018 fixed orders ---------------------------------------------------------------------

# quiver.h: dependency order — config, core, extern_decls, dispatch, then kernel headers alpha.
PUBLIC_HEADERS = [
    "include/quiver/detail/config.h",
    "include/quiver/core.h",
    "include/quiver/detail/extern_decls.h",
    "include/quiver/dispatch.h",
    "include/quiver/arith.h",
    "include/quiver/compare.h",
    "include/quiver/filter.h",
    "include/quiver/hash.h",
    "include/quiver/mask.h",
    "include/quiver/reduce.h",
    "include/quiver/select.h",
    "include/quiver/take.h",
    "include/quiver/unpack.h",
]

# quiver.cpp source order: cpu, dispatch, kernels/common, then families alphabetically with
# scalar -> avx2 -> neon -> avx512 within each family (ADR-018). Built by rule so a new family
# or backend is picked up without editing this file.
_ISA_ORDER = ["scalar", "avx2", "neon", "avx512"]
_LEADING_CPP = [
    "src/cpu/cpu_features.cpp",
    "src/dispatch/dispatch_tables.cpp",
    "src/dispatch/version.cpp",
    "src/kernels/common/luts.cpp",
]

_PRAGMA_ONCE = re.compile(r"^\s*#\s*pragma\s+once\s*$")
_INCLUDE = re.compile(r'^\s*#\s*include\s+([<"])([^>"]+)[>"]')


def _family_cpp() -> list[str]:
    fam_root = ROOT / "src" / "kernels"
    families = sorted(p.name for p in fam_root.iterdir() if p.is_dir() and p.name != "common")
    out: list[str] = []
    for fam in families:
        for isa in _ISA_ORDER:
            rel = f"src/kernels/{fam}/{fam}_{isa}.cpp"
            if (ROOT / rel).exists():
                out.append(rel)
    return out


def _source_cpp() -> list[str]:
    return _LEADING_CPP + _family_cpp()


def _internal_headers_topo() -> list[str]:
    """All src/**/*.h, topologically ordered by their ``#include "src/..."`` edges so each
    header follows its dependencies; alphabetical tiebreak keeps it deterministic."""
    headers = sorted(p.relative_to(ROOT).as_posix() for p in (ROOT / "src").rglob("*.h"))
    deps: dict[str, set[str]] = {h: set() for h in headers}
    for h in headers:
        for _, target in _includes((ROOT / h).read_text()):
            if target.startswith("src/") and target in deps:
                deps[h].add(target)
    ordered: list[str] = []
    seen: set[str] = set()

    def visit(h: str, stack: tuple[str, ...]) -> None:
        if h in seen:
            return
        if h in stack:
            raise SystemExit(f"amalgamate: cyclic internal include involving {h}")
        for dep in sorted(deps[h]):
            visit(dep, stack + (h,))
        seen.add(h)
        ordered.append(h)

    for h in headers:  # headers already sorted -> deterministic visitation
        visit(h, ())
    return ordered


def _includes(text: str):
    for line in text.splitlines():
        m = _INCLUDE.match(line)
        if m:
            yield m.group(1), m.group(2)


def _is_internal(target: str) -> bool:
    return target.startswith("quiver/") or target.startswith("src/")


def _strip(text: str, *, drop_pragma_once: bool, angle_sink: set[str] | None) -> list[str]:
    """Return `text`'s lines with internal quoted includes removed; #pragma once removed when
    asked; angle (system) includes routed to `angle_sink` (hoisted) or kept in place."""
    out: list[str] = []
    for line in text.splitlines():
        if drop_pragma_once and _PRAGMA_ONCE.match(line):
            continue
        m = _INCLUDE.match(line)
        if m:
            kind, target = m.group(1), m.group(2)
            if kind == '"' and _is_internal(target):
                continue  # inlined elsewhere in the amalgamation
            if kind == "<" and angle_sink is not None:
                angle_sink.add(target)
                continue
        out.append(line)
    return out


def _version() -> str:
    try:
        return subprocess.run(
            ["git", "describe", "--tags", "--always", "--dirty"],
            cwd=ROOT, capture_output=True, text=True, check=True,
        ).stdout.strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return "unknown"


def _banner(kind: str, version: str) -> list[str]:
    return [
        f"// Quiver — single-file amalgamation ({kind}). GENERATED by tools/amalgamate — do not edit.",
        f"// Source version: {version}",
        "// Upstream: https://github.com/div0rce/quiver — License: Apache-2.0 (see upstream LICENSE).",
        "",
    ]


def generate(out_dir: Path) -> None:
    version = _version()

    # --- quiver.h: banner + hoisted system includes + public headers (guards/internal-includes
    # --- stripped), in dependency order. ------------------------------------------------------
    angles: set[str] = set()
    bodies: list[str] = []
    for rel in PUBLIC_HEADERS:
        bodies += _strip((ROOT / rel).read_text(), drop_pragma_once=True, angle_sink=angles)
    header_lines = _banner("public header", version)
    header_lines.append("#pragma once")
    header_lines.append("")
    header_lines += [f"#include <{h}>" for h in sorted(angles)]
    header_lines.append("")
    header_lines += bodies
    (out_dir / "quiver.h").write_text("\n".join(header_lines).rstrip("\n") + "\n")

    # --- quiver.cpp: banner + include "quiver.h" + internal headers (topo) + sources (fixed
    # --- order). System includes stay in place (self-guarded; some live inside #if ISA
    # --- regions), only internal quoted includes and #pragma once are stripped. ---------------
    cpp_lines = _banner("implementation", version)
    cpp_lines.append('#include "quiver.h"')
    cpp_lines.append("")
    for rel in _internal_headers_topo():
        cpp_lines.append(f"// ===== {rel} =====")
        cpp_lines += _strip((ROOT / rel).read_text(), drop_pragma_once=True, angle_sink=None)
        cpp_lines.append("")
    for rel in _source_cpp():
        cpp_lines.append(f"// ===== {rel} =====")
        cpp_lines += _strip((ROOT / rel).read_text(), drop_pragma_once=False, angle_sink=None)
        cpp_lines.append("")
    (out_dir / "quiver.cpp").write_text("\n".join(cpp_lines).rstrip("\n") + "\n")


# --- REQ-STD-006 amalgamation-compatibility lint ----------------------------------------------

_GUARD_MACRO = re.compile(r"^\s*#\s*ifndef\s+\w+_H\b|^\s*#\s*define\s+\w+_H\b")


def check_file(rel: str, text: str, *, is_header: bool) -> list[str]:
    """Return the REQ-STD-006 violations in one file's text (pure; no I/O). `rel` labels the
    file in messages, `is_header` selects the header-only `#pragma once` rule."""
    out: list[str] = []
    lines = text.splitlines()

    # Rule: headers use `#pragma once` exactly, and no include-guard macros anywhere.
    if is_header and not any(_PRAGMA_ONCE.match(ln) for ln in lines):
        out.append(f"{rel}: header missing `#pragma once` (REQ-STD-006)")
    for ln in lines:
        if _GUARD_MACRO.match(ln):
            out.append(f"{rel}: include-guard macro; use `#pragma once` (REQ-STD-006): {ln.strip()}")
            break

    # Rule: internal includes are quoted project-relative; system includes are angle-bracketed.
    for kind, target in _includes(text):
        if kind == '"' and not _is_internal(target):
            out.append(f'{rel}: quoted include "{target}" is not project-relative (REQ-STD-006)')
        if kind == "<" and (target.startswith("quiver/") or target.startswith("src/")):
            out.append(f"{rel}: project header <{target}> must be quoted (REQ-STD-006)")

    # Rule: no #include inside a namespace or function body (must sit at file scope). Tracked by
    # brace depth; any include seen at depth > 0 violates it.
    depth = 0
    for ln in lines:
        if _INCLUDE.match(ln) and depth > 0:
            out.append(f"{rel}: #include inside a namespace/function body (REQ-STD-006): {ln.strip()}")
        depth += ln.count("{") - ln.count("}")
    return out


def check() -> int:
    """Lint the rules the generator relies on. Rules apply from M0 to every shipped header/TU."""
    errors: list[str] = []
    files = sorted(
        [*(ROOT / "include").rglob("*.h"), *(ROOT / "src").rglob("*.h"), *(ROOT / "src").rglob("*.cpp")]
    )
    for f in files:
        errors += check_file(f.relative_to(ROOT).as_posix(), f.read_text(), is_header=f.suffix == ".h")
    if errors:
        print("amalgamate --check: FAIL", file=sys.stderr)
        for e in errors:
            print(f"  - {e}", file=sys.stderr)
        return 1
    print(f"amalgamate --check: OK ({len(files)} files)")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description="Quiver amalgamation generator (ADR-018).")
    ap.add_argument("--out-dir", type=Path, help="directory to write quiver.h + quiver.cpp")
    ap.add_argument("--check", action="store_true", help="lint REQ-STD-006 rules and exit")
    args = ap.parse_args()
    if args.check:
        return check()
    if not args.out_dir:
        ap.error("either --check or --out-dir is required")
    args.out_dir.mkdir(parents=True, exist_ok=True)
    generate(args.out_dir)
    return 0


if __name__ == "__main__":
    sys.exit(main())
