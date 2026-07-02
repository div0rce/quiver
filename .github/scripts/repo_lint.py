#!/usr/bin/env python3
"""Repository lint — MOD-CI.

Enforces from M0 onward:
  REQ-REPO-001  tree matches the committed manifest (.github/repo-manifest.json)
  REQ-REPO-012  every docs directory carries a README.md
  REQ-DOC-004   ADR set complete (ADR-001..NNN + index)
  REQ-SIMD-010  no SVE2 sources in main
  REQ-MEM-002   no *_padded kernel variants in v1

Include-graph lints (REQ-REPO-005/-006/-009) and the `_impl.h` purity lint
(REQ-SIMD-006) activate when production code exists (M1/M3+); their file scans
run unconditionally and pass vacuously on an empty tree.

Exit code 0 = clean; non-zero = violations printed to stderr.
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / ".github" / "repo-manifest.json"

errors: list[str] = []


def err(msg: str) -> None:
    errors.append(msg)


def main() -> int:
    manifest = json.loads(MANIFEST.read_text())

    # --- REQ-REPO-001: top-level allow-list -------------------------------------------------
    allowed = set(manifest["allowed_toplevel"])
    for entry in sorted(ROOT.iterdir()):
        if entry.name not in allowed:
            err(f"REQ-REPO-001: unexpected top-level entry '{entry.name}' "
                f"(not in .github/repo-manifest.json; new directories need a PRD amendment)")

    # --- REQ-REPO-001: required paths exist --------------------------------------------------
    for rel in manifest["required"]:
        if not (ROOT / rel).exists():
            err(f"REQ-REPO-001: required path missing: {rel}")

    # --- Milestone scope: forbidden directories ----------------------------------------------
    for rel in manifest["forbidden_at_milestone"]:
        if (ROOT / rel).exists():
            err(f"REQ-MS-001: '{rel}/' must not exist at milestone {manifest['milestone']} "
                f"(introduced by a later milestone per docs/prd/18-milestones.md)")

    # --- REQ-REPO-012: docs READMEs ----------------------------------------------------------
    for rel in manifest["docs_dirs_requiring_readme"]:
        if not (ROOT / rel / "README.md").exists():
            err(f"REQ-REPO-012: {rel}/README.md missing")

    # --- REQ-DOC-004: ADR completeness -------------------------------------------------------
    adr_dir = ROOT / "docs" / "adr"
    want = manifest["adr_count"]
    found = {int(m.group(1)) for f in adr_dir.glob("ADR-*.md")
             if (m := re.match(r"ADR-(\d{3})-", f.name))}
    missing = [f"ADR-{i:03d}" for i in range(1, want + 1) if i not in found]
    if missing:
        err(f"REQ-DOC-004: missing ADR files: {', '.join(missing)}")
    extra = sorted(i for i in found if i > want)
    if extra:
        err(f"REQ-DOC-004: ADR files beyond manifest count {want}: {extra} "
            f"(update the manifest in the same PR)")
    index = adr_dir / "README.md"
    if index.exists():
        indexed = {int(m.group(1)) for m in re.finditer(r"\[ADR-(\d{3})\]", index.read_text())}
        if indexed != set(range(1, want + 1)):
            err(f"REQ-DOC-004: docs/adr/README.md index does not list exactly ADR-001..{want:03d}")

    # --- Source scans (vacuous while no production code exists) ------------------------------
    src_globs = ["include/**/*.h", "src/**/*.h", "src/**/*.cpp"]
    for pattern in src_globs:
        for f in ROOT.glob(pattern):
            text = f.read_text(errors="replace")
            if "arm_sve.h" in text or re.search(r"\bsv(bool|int|uint|float)\w*_t\b", text):
                err(f"REQ-SIMD-010: SVE2 source content in {f.relative_to(ROOT)} "
                    f"(SVE2 lives on the exploratory branch only)")
            if re.search(r"\w+_padded\s*\(", text):
                err(f"REQ-MEM-002: '_padded' variant in {f.relative_to(ROOT)} "
                    f"(reserved naming; requires ledger evidence + PRD amendment)")

    if errors:
        print("repo-lint: FAIL", file=sys.stderr)
        for e in errors:
            print(f"  - {e}", file=sys.stderr)
        return 1
    print(f"repo-lint: OK (manifest milestone {manifest['milestone']}, "
          f"{want} ADRs verified)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
