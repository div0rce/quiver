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
import subprocess
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

    # --- Include-graph lint (REQ-REPO-005/-006/-009; layering per PRD 02 §6) -----------------
    std_headers = {
        "algorithm", "array", "atomic", "bit", "cassert", "concepts", "cstddef", "cstdint",
        "cstdio", "cstdlib", "cstring", "limits", "memory", "new", "numeric", "span",
        "string", "string_view", "thread", "type_traits", "utility", "vector",
    }
    os_headers_by_file = {
        "src/cpu/cpu_features.cpp": {"cpuid.h", "intrin.h", "immintrin.h",
                                     "sys/auxv.h", "sys/sysctl.h"},
    }
    # Per-ISA kernel TUs use the compiler-provided intrinsic headers — toolchain surface,
    # not a third-party dependency; same class as <cpuid.h> (PRD 09, ADR-003).
    isa_headers_by_suffix = {
        "_avx2.cpp": {"immintrin.h"},
        "_avx512.cpp": {"immintrin.h"},
        "_neon.cpp": {"arm_neon.h"},
    }
    # module -> allowed quoted-include prefixes (dependency direction, PRD 02 §6)
    quoted_rules = [
        ("include/quiver/", ("quiver/",)),
        ("src/cpu/", ("quiver/detail/", "src/cpu/")),
        ("src/dispatch/", ("quiver/", "src/cpu/", "src/dispatch/")),
        ("src/kernels/common/", ("quiver/", "src/kernels/common/")),
        # kernel families: own dir + common only (REQ-REPO-009); family dirs checked generically
        ("src/kernels/", ("quiver/", "src/kernels/common/", "src/dispatch/")),
    ]
    inc_re = re.compile(r'^\s*#\s*include\s+([<"])([^">]+)[">]', re.M)
    for f in sorted(list(ROOT.glob("include/**/*.h")) + list(ROOT.glob("src/**/*.h"))
                    + list(ROOT.glob("src/**/*.cpp"))):
        rel = f.relative_to(ROOT).as_posix()
        text = f.read_text(errors="replace")
        for m2 in inc_re.finditer(text):
            style, target = m2.group(1), m2.group(2)
            if style == "<":
                allowed = std_headers | os_headers_by_file.get(rel, set())
                for suffix, hdrs in isa_headers_by_suffix.items():
                    if rel.endswith(suffix):
                        allowed = allowed | hdrs
                if target not in allowed:
                    err(f"REQ-REPO-006: {rel} includes <{target}> — not in the std/OS "
                        f"allow-list (shipped code: std + documented OS headers only)")
            else:
                rules = [pfx for base, pfx in quoted_rules if rel.startswith(base)]
                prefixes = tuple(rules[0]) if rules else ()
                fam_dir = re.match(r"(src/kernels/[a-z0-9_]+/)", rel)
                if fam_dir:  # a family may include its own directory (the 5-file pattern)
                    prefixes = prefixes + (fam_dir.group(1),)
                if not any(target.startswith(p) for p in prefixes):
                    err(f"REQ-REPO-005/-009: {rel} includes \"{target}\" — violates the "
                        f"module dependency rules of PRD 02 §6")
                if not (ROOT / "include" / target).exists() and not (ROOT / target).exists():
                    err(f"REQ-REPO-006: {rel} includes \"{target}\" which does not resolve "
                        f"within include/ or the repo root")
        # kernel-family cross-include check (REQ-REPO-009): a family may not include another
        # family's directory (vacuous until M3; generic rule keeps it enforced forever).
        fam = re.match(r"src/kernels/(?!common)([a-z0-9_]+)/", rel)
        if fam:
            for m3 in inc_re.finditer(text):
                tgt = m3.group(2)
                other = re.match(r"src/kernels/(?!common)([a-z0-9_]+)/", tgt)
                if other and other.group(1) != fam.group(1):
                    err(f"REQ-REPO-009: {rel} includes another family's internals: {tgt}")

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

    # --- Ledger entry_id references in docs (REQ-LEDGER-015): every `qle:<entry_id>` inline
    # --- reference must exist in a committed entries.json — no hand-copied numbers without
    # --- provenance (Charter T2).
    committed_ids: set[str] = set()
    for entries_file in ROOT.glob("ledger/results/*/*/entries.json"):
        try:
            for entry in json.loads(entries_file.read_text()):
                committed_ids.add(entry.get("entry_id", ""))
        except (json.JSONDecodeError, TypeError):
            err(f"REQ-LEDGER-001: {entries_file.relative_to(ROOT)} is not a valid entry array")
    qle_re = re.compile(r"`qle:([A-Za-z0-9._-]+)`")
    for doc in ROOT.glob("docs/**/*.md"):
        for m in qle_re.finditer(doc.read_text(errors="replace")):
            if m.group(1) not in committed_ids:
                err(f"REQ-LEDGER-015: {doc.relative_to(ROOT)} references entry_id "
                    f"'{m.group(1)}' which exists in no committed entries.json")

    # --- Release-status coherence (REQ-REL-001): the version strings humans keep forgetting to
    # --- synchronize must agree. Single source of truth: include/quiver/detail/config.h.
    cfg = (ROOT / "include/quiver/detail/config.h").read_text(errors="replace")
    ver_parts = {k: m.group(1) for k in ("MAJOR", "MINOR", "PATCH")
                 if (m := re.search(rf"#define QUIVER_VERSION_{k} (\d+)", cfg))}
    if len(ver_parts) != 3:
        err("REQ-REL-001: cannot parse QUIVER_VERSION_* from config.h")
    else:
        ver = f"{ver_parts['MAJOR']}.{ver_parts['MINOR']}.{ver_parts['PATCH']}"
        changelog = (ROOT / "CHANGELOG.md").read_text(errors="replace")
        m = re.search(r"^## \[(\d+\.\d+\.\d+)\]", changelog, re.MULTILINE)
        if not m or m.group(1) != ver:
            err(f"REQ-REL-001: CHANGELOG latest release is "
                f"'{m.group(1) if m else '?'}' but config.h says {ver}")
        readme = (ROOT / "README.md").read_text(errors="replace")
        m = re.search(r"Current release: \[v(\d+\.\d+\.\d+)\]", readme)
        if not m or m.group(1) != ver:
            err(f"REQ-REL-001: README 'Current release' is "
                f"'v{m.group(1) if m else '?'}' but config.h says {ver}")
        vcpkg = json.loads((ROOT / "cmake/packaging/vcpkg/vcpkg.json").read_text())
        if vcpkg.get("version") != ver:
            err(f"REQ-REL-001: vcpkg.json version '{vcpkg.get('version')}' != config.h {ver}")
        conan = (ROOT / "cmake/packaging/conan/conanfile.py").read_text(errors="replace")
        m = re.search(r'version = "(\d+\.\d+\.\d+)"', conan)
        if not m or m.group(1) != ver:
            err(f"REQ-REL-001: conanfile.py version "
                f"'{m.group(1) if m else '?'}' != config.h {ver}")
        # Latest reachable tag, when tags are available (CI shallow checkouts may lack them).
        try:
            tag = subprocess.run(["git", "-C", str(ROOT), "describe", "--tags", "--abbrev=0"],
                                 capture_output=True, text=True, timeout=10)
            if tag.returncode == 0:
                latest = tag.stdout.strip().lstrip("v")
                if latest != ver:
                    err(f"REQ-REL-001: latest git tag v{latest} != config.h {ver} "
                        f"(run the post-release bump)")
        except (OSError, subprocess.TimeoutExpired):
            pass  # no git or no tags: the file-level checks above still hold

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
