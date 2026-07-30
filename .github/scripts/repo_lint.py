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

import fnmatch
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


def git(*args: str) -> tuple[int, str]:
    """Run git inside ROOT. Returns (-1, "") when git is unavailable (see REQ-REL-001 below)."""
    try:
        proc = subprocess.run(["git", "-C", str(ROOT), *args],
                              capture_output=True, text=True, timeout=10)
        return proc.returncode, proc.stdout
    except (OSError, subprocess.TimeoutExpired):
        return -1, ""


def check_local_tree(entry: Path) -> None:
    """REQ-REPO-001: a developer-local pattern tolerates a gitignored *directory* holding no
    committed content — never a tracked path. Name-matching alone would accept a regular file
    named `.idea` (`.gitignore` ignores only `.idea/`) or a force-added `.serena/secret`."""
    if entry.is_symlink() or not entry.is_dir():
        err(f"REQ-REPO-001: '{entry.name}' matches a developer-local allow-list pattern but is "
            f"not a real directory; those patterns cover gitignored directories only")
        return
    rc, out = git("ls-files", "--", f"{entry.name}/")
    if rc == 0 and out.strip():
        tracked = out.strip().splitlines()
        err(f"REQ-REPO-001: developer-local tree '{entry.name}/' holds {len(tracked)} committed "
            f"path(s) (e.g. {tracked[0]}); these trees may never hold committed content")
    rc, _ = git("check-ignore", "-q", f"{entry.name}/")
    if rc == 1:  # 0 = ignored, 1 = not ignored, other = git unavailable or erroring
        err(f"REQ-REPO-001: developer-local tree '{entry.name}/' is not gitignored; add it to "
            f".gitignore or remove it")


def check_toplevel(manifest: dict) -> None:
    """REQ-REPO-001 top-level allow-list. Kept separate from main() so test_repo_lint.py can
    exercise it against a throwaway tree without stubbing every other check's inputs.

    Both lists hold fnmatch globs; a literal name matches only itself, so plain entries keep
    exact-match semantics. `allowed_toplevel` is repository layout. `allowed_toplevel_local` is
    developer-local tooling state (IDE metadata, virtualenvs, tool-chosen build trees such as
    `cmake-build-*`) which the lint tolerates but holds to the stricter contract in
    check_local_tree: gitignored directory, no committed content."""
    allowed = manifest["allowed_toplevel"]
    local = manifest.get("allowed_toplevel_local", [])
    for entry in sorted(ROOT.iterdir()):
        if any(fnmatch.fnmatchcase(entry.name, pat) for pat in allowed):
            continue
        if any(fnmatch.fnmatchcase(entry.name, pat) for pat in local):
            check_local_tree(entry)
            continue
        err(f"REQ-REPO-001: unexpected top-level entry '{entry.name}' "
            f"(not in .github/repo-manifest.json; new directories need a PRD amendment)")


def check_required_paths(manifest: dict) -> None:
    # --- REQ-REPO-001: required paths exist --------------------------------------------------
    for rel in manifest["required"]:
        if not (ROOT / rel).exists():
            err(f"REQ-REPO-001: required path missing: {rel}")



def check_forbidden_dirs(manifest: dict) -> None:
    # --- Milestone scope: forbidden directories ----------------------------------------------
    for rel in manifest["forbidden_at_milestone"]:
        if (ROOT / rel).exists():
            err(f"REQ-MS-001: '{rel}/' must not exist at milestone {manifest['milestone']} "
                f"(introduced by a later milestone per docs/prd/18-milestones.md)")



def check_docs_readmes(manifest: dict) -> None:
    # --- REQ-REPO-012: docs READMEs ----------------------------------------------------------
    for rel in manifest["docs_dirs_requiring_readme"]:
        if not (ROOT / rel / "README.md").exists():
            err(f"REQ-REPO-012: {rel}/README.md missing")



def check_adrs(manifest: dict) -> int:
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

    return want


# Shipped code may include these and nothing else (REQ-REPO-006).
STD_HEADERS = {
    "algorithm", "array", "atomic", "bit", "cassert", "concepts", "cstddef", "cstdint",
    "cstdio", "cstdlib", "cstring", "limits", "memory", "new", "numeric", "ranges", "span",
    "string", "string_view", "thread", "type_traits", "utility", "vector",
}
OS_HEADERS_BY_FILE = {
    "src/cpu/cpu_features.cpp": {"cpuid.h", "intrin.h", "immintrin.h",
                                 "sys/auxv.h", "sys/sysctl.h"},
}
# Per-ISA kernel TUs use the compiler-provided intrinsic headers — toolchain surface,
# not a third-party dependency; same class as <cpuid.h> (PRD 09, ADR-003).
ISA_HEADERS_BY_SUFFIX = {
    "_avx2.cpp": {"immintrin.h"},
    "_avx512.cpp": {"immintrin.h"},
    "_neon.cpp": {"arm_neon.h"},
}
# module -> allowed quoted-include prefixes (dependency direction, PRD 02 §6)
QUOTED_RULES = [
    ("include/quiver/", ("quiver/",)),
    ("src/cpu/", ("quiver/detail/", "src/cpu/")),
    ("src/dispatch/", ("quiver/", "src/cpu/", "src/dispatch/")),
    ("src/kernels/common/", ("quiver/", "src/kernels/common/")),
    # kernel families: own dir + common only (REQ-REPO-009); family dirs checked generically
    ("src/kernels/", ("quiver/", "src/kernels/common/", "src/dispatch/")),
]
INC_RE = re.compile(r'^\s*#\s*include\s+([<"])([^">]+)[">]', re.M)
FAMILY_RE = re.compile(r"src/kernels/(?!common)([a-z0-9_]+)/")


def angle_allowed_for(rel: str) -> set[str]:
    isa = {h for suffix, hdrs in ISA_HEADERS_BY_SUFFIX.items() if rel.endswith(suffix)
           for h in hdrs}
    return STD_HEADERS | OS_HEADERS_BY_FILE.get(rel, set()) | isa


def quoted_prefixes_for(rel: str) -> tuple[str, ...]:
    # Longest matching base wins. Several bases overlap (src/kernels/common/ is inside
    # src/kernels/), and resolving by declaration order would let a future reordering silently
    # apply the broader rule with no test to catch it.
    matches = [(base, pfx) for base, pfx in QUOTED_RULES if rel.startswith(base)]
    prefixes = tuple(max(matches, key=lambda bp: len(bp[0]))[1]) if matches else ()
    fam_dir = re.match(r"(src/kernels/[a-z0-9_]+/)", rel)
    if fam_dir:  # a family may include its own directory (the 5-file pattern)
        prefixes = prefixes + (fam_dir.group(1),)
    return prefixes


def check_angle_include(rel: str, target: str) -> None:
    if target not in angle_allowed_for(rel):
        err(f"REQ-REPO-006: {rel} includes <{target}> — not in the std/OS "
            f"allow-list (shipped code: std + documented OS headers only)")


def check_quoted_include(rel: str, target: str) -> None:
    prefixes = quoted_prefixes_for(rel)
    if not any(target.startswith(p) for p in prefixes):
        err(f"REQ-REPO-005/-009: {rel} includes \"{target}\" — violates the "
            f"module dependency rules of PRD 02 §6")
    if not (ROOT / "include" / target).exists() and not (ROOT / target).exists():
        err(f"REQ-REPO-006: {rel} includes \"{target}\" which does not resolve "
            f"within include/ or the repo root")


# A family may not include another family's internals (REQ-REPO-009): vacuous until M3, but the
# generic rule keeps it enforced forever.
def check_family_isolation(rel: str, text: str) -> None:
    fam = FAMILY_RE.match(rel)
    if not fam:
        return
    for m in INC_RE.finditer(text):
        other = FAMILY_RE.match(m.group(2))
        if other and other.group(1) != fam.group(1):
            err(f"REQ-REPO-009: {rel} includes another family's internals: {m.group(2)}")


def check_include_graph(manifest: dict) -> None:
    # --- Include-graph lint (REQ-REPO-005/-006/-009; layering per PRD 02 §6) -----------------
    del manifest  # graph rules are structural, not manifest-driven
    for f in sorted(list(ROOT.glob("include/**/*.h")) + list(ROOT.glob("src/**/*.h"))
                    + list(ROOT.glob("src/**/*.cpp"))):
        rel = f.relative_to(ROOT).as_posix()
        text = f.read_text(errors="replace")
        for m2 in INC_RE.finditer(text):
            if m2.group(1) == "<":
                check_angle_include(rel, m2.group(2))
            else:
                check_quoted_include(rel, m2.group(2))
        check_family_isolation(rel, text)


def check_autovec_baseline_coverage() -> None:
    """REQ-BENCH-010 / ADR-011: the equal-ISA autovec baseline recompiles EVERY family's
    `_scalar_impl.h`, so every family with an explicit AVX2 backend has a verdict pair
    (REQ-LEDGER-011). arith_guarded was omitted, and the gap was invisible until an x86 machine
    was registered: on ARM the portable scalar build IS the autovec baseline (REQ-BENCH-010), so
    the NEON verdict existed while the AVX2 one could never be produced.

    Structural, not behavioural: a family is covered iff the baseline TU re-includes its
    reference header and its microbenchmark registers the `autovec-avx2` variant name.
    """
    baseline = ROOT / "bench" / "baselines" / "baseline_avx2.cpp"
    if not baseline.exists():
        return  # baselines land at their owning milestone
    recompiled = baseline.read_text(errors="replace")
    for impl in sorted(ROOT.glob("src/kernels/*/*_scalar_impl.h")):
        family = impl.parent.name
        # Only families that actually ship an explicit AVX2 backend owe a verdict pair.
        if not (impl.parent / f"{family}_avx2.cpp").exists():
            continue
        if f"src/kernels/{family}/{impl.name}" not in recompiled:
            err(f"REQ-BENCH-010: {family} has an explicit AVX2 backend but "
                f"bench/baselines/baseline_avx2.cpp does not recompile {impl.name} — "
                f"no autovec-avx2 baseline, so REQ-LEDGER-011 can derive no verdict")
        bench = ROOT / "bench" / "micro" / f"bench_{family}.cpp"
        if bench.exists() and '"autovec-avx2"' not in bench.read_text(errors="replace"):
            err(f"REQ-BENCH-010: bench/micro/bench_{family}.cpp registers no `autovec-avx2` "
                f"variant, so its AVX2 entries have no equal-ISA pair (ADR-011)")


def check_source_scans(manifest: dict) -> None:
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



def committed_entry_ids() -> set[str]:
    ids: set[str] = set()
    for entries_file in ROOT.glob("ledger/results/*/*/entries.json"):
        try:
            for entry in json.loads(entries_file.read_text()):
                ids.add(entry.get("entry_id", ""))
        # AttributeError covers a top-level object or non-object elements: entry.get would
        # raise, and this lint must report a clean violation rather than crash mid-scan.
        except (json.JSONDecodeError, TypeError, AttributeError):
            err(f"REQ-LEDGER-001: {entries_file.relative_to(ROOT)} is not a valid entry array")
    return ids


def check_ledger_refs(manifest: dict) -> None:
    # --- Ledger entry_id references in docs (REQ-LEDGER-015): every `qle:<entry_id>` inline
    # --- reference must exist in a committed entries.json — no hand-copied numbers without
    # --- provenance (Charter T2).
    committed_ids = committed_entry_ids()
    qle_re = re.compile(r"`qle:([A-Za-z0-9._-]+)`")
    for doc in ROOT.glob("docs/**/*.md"):
        for m in qle_re.finditer(doc.read_text(errors="replace")):
            if m.group(1) not in committed_ids:
                err(f"REQ-LEDGER-015: {doc.relative_to(ROOT)} references entry_id "
                    f"'{m.group(1)}' which exists in no committed entries.json")



def config_version() -> str | None:
    cfg = (ROOT / "include/quiver/detail/config.h").read_text(errors="replace")
    parts = {k: m.group(1) for k in ("MAJOR", "MINOR", "PATCH")
             if (m := re.search(rf"#define QUIVER_VERSION_{k} (\d+)", cfg))}
    if len(parts) != 3:
        err("REQ-REL-001: cannot parse QUIVER_VERSION_* from config.h")
        return None
    return f"{parts['MAJOR']}.{parts['MINOR']}.{parts['PATCH']}"


# Each place a release version is written down, and how to read it back out.
VERSION_SOURCES = [
    ("CHANGELOG latest release", "CHANGELOG.md", r"^## \[(\d+\.\d+\.\d+)\]"),
    ("README 'Current release'", "README.md", r"Current release: \[v(\d+\.\d+\.\d+)\]"),
    ("conanfile.py version", "cmake/packaging/conan/conanfile.py",
     r'version = "(\d+\.\d+\.\d+)"'),
]


def check_declared_versions(ver: str) -> None:
    for label, rel, pattern in VERSION_SOURCES:
        text = (ROOT / rel).read_text(errors="replace")
        m = re.search(pattern, text, re.MULTILINE)
        if not m or m.group(1) != ver:
            err(f"REQ-REL-001: {label} is '{m.group(1) if m else '?'}' but config.h says {ver}")
    vcpkg = json.loads((ROOT / "cmake/packaging/vcpkg/vcpkg.json").read_text())
    if vcpkg.get("version") != ver:
        err(f"REQ-REL-001: vcpkg.json version '{vcpkg.get('version')}' != config.h {ver}")


# config.h AHEAD of the tag is the legal release-prep state (the tag lands after the release PR
# merges); the tag being AHEAD of config.h means the post-release bump was forgotten — the drift
# this check exists to catch. Tags may be absent entirely in CI shallow checkouts.
def check_tag_not_ahead(ver: str) -> None:
    try:
        tag = subprocess.run(["git", "-C", str(ROOT), "describe", "--tags", "--abbrev=0"],
                             capture_output=True, text=True, timeout=10)
        if tag.returncode != 0:
            return
        latest = tag.stdout.strip().lstrip("v")
        if tuple(map(int, latest.split("."))) > tuple(map(int, ver.split("."))):
            err(f"REQ-REL-001: latest git tag v{latest} is ahead of config.h {ver} "
                f"(run the post-release bump)")
    except (OSError, subprocess.TimeoutExpired, ValueError):
        pass  # no git, no tags, or unparseable: the file-level checks above still hold


def check_release_status(manifest: dict) -> None:
    # --- Release-status coherence (REQ-REL-001): the version strings humans keep forgetting to
    # --- synchronize must agree. Single source of truth: include/quiver/detail/config.h.
    del manifest  # versions come from the tree, not the manifest
    ver = config_version()
    if ver is None:
        return
    check_declared_versions(ver)
    check_tag_not_ahead(ver)


def main() -> int:
    manifest = json.loads(MANIFEST.read_text())

    check_toplevel(manifest)

    check_required_paths(manifest)
    check_forbidden_dirs(manifest)
    check_docs_readmes(manifest)
    want = check_adrs(manifest)
    check_include_graph(manifest)
    check_source_scans(manifest)
    check_autovec_baseline_coverage()
    check_ledger_refs(manifest)
    check_release_status(manifest)

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
