#!/usr/bin/env python3
"""Self-tests for the REQ-REPO-001 top-level allow-list in repo_lint.py.

Covers both allow-list semantics and the developer-local contract:
  * repository entries match by exact name; unknown entries are rejected;
  * `allowed_toplevel_local` globs tolerate tool-chosen names (cmake-build-*) without over-matching;
  * a developer-local match must be a real directory, gitignored, and free of committed content.

Each case builds a throwaway git repo and calls repo_lint.check_toplevel against it directly, so the
shipped implementation is exercised without stubbing the inputs of every unrelated check.
Exit 0 = clean.
"""

from __future__ import annotations

import importlib.util
import subprocess
import sys
import tempfile
from pathlib import Path

SCRIPT = Path(__file__).resolve().parent / "repo_lint.py"

MANIFEST = {
    "allowed_toplevel": [".git", ".github", ".gitignore", "docs"],
    "allowed_toplevel_local": [".idea", ".serena", ".venv", ".vscode", "cmake-build-*", "venv"],
}
IGNORES = "build/\n.venv/\nvenv/\n.idea/\n.vscode/\n.serena/\ncmake-build-*/\n"

failures: list[str] = []


def load_repo_lint():
    spec = importlib.util.spec_from_file_location("repo_lint_under_test", SCRIPT)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def run(root: Path, manifest: dict | None = None) -> list[str]:
    """Point repo_lint at `root` and return the findings from the allow-list check alone."""
    mod = load_repo_lint()
    mod.ROOT = root
    mod.errors.clear()
    mod.check_toplevel(manifest or MANIFEST)
    return list(mod.errors)


def new_repo(ignores: str = IGNORES) -> Path:
    root = Path(tempfile.mkdtemp(prefix="repolint-")) / "repo"
    root.mkdir(parents=True)
    subprocess.run(["git", "init", "-q", "-b", "main", str(root)], check=True)
    subprocess.run(["git", "-C", str(root), "config", "user.email", "t@example.invalid"], check=True)
    subprocess.run(["git", "-C", str(root), "config", "user.name", "t"], check=True)
    (root / ".gitignore").write_text(ignores)
    (root / ".github").mkdir()
    (root / "docs").mkdir()
    subprocess.run(["git", "-C", str(root), "add", "-A"], check=True)
    subprocess.run(["git", "-C", str(root), "commit", "-qm", "base"], check=True)
    return root


def check(name: str, found: list[str], *, expect: str | None) -> None:
    if expect is None:
        if found:
            failures.append(f"{name}: expected no findings, got {found}")
            return
    elif not any(expect in f for f in found):
        failures.append(f"{name}: expected a finding containing {expect!r}, got {found}")
        return
    print(f"  ok  {name}")


def check_submission(name: str, setup, *, expect: str | None) -> None:
    """`submission/` is admitted by allowed_toplevel (CONTRIBUTING.md Lane A), so the shape check
    is the only thing stopping it becoming an arbitrary top-level directory."""
    root = new_repo()
    (root / "submission").mkdir()
    setup(root / "submission")
    mod = load_repo_lint()
    mod.ROOT = root
    mod.errors.clear()
    mod.check_lane_a_submission()
    check(name, list(mod.errors), expect=expect)


BUNDLE = ("manifest.json", "entries.json", "rejected_noisy.json",
          "commands.txt", "git-status.txt", "checksums.txt")


def _complete(sub):
    for f in BUNDLE:
        (sub / f).write_text("{}")
    (sub / "raw").mkdir()
    (sub / "raw" / "rep0.json").write_text("{}")


def main() -> int:
    print("repo_lint allow-list self-tests:")

    root = new_repo()
    check("baseline tree is clean", run(root), expect=None)

    # Repository entries stay exact-match; unknown names are still rejected.
    root = new_repo()
    (root / "node_modules").mkdir()
    check("unknown entry rejected", run(root), expect="unexpected top-level entry 'node_modules'")

    # Developer-local globs accept tool-chosen names ...
    root = new_repo()
    (root / "cmake-build-relwithdebinfo").mkdir()
    check("cmake-build-* glob accepted", run(root), expect=None)

    # ... without over-matching neighbouring names.
    root = new_repo()
    (root / "cmake-buildX-evil").mkdir()
    check("glob does not over-match", run(root),
          expect="unexpected top-level entry 'cmake-buildX-evil'")

    # The conventional virtualenv name is tolerated (it is in .gitignore).
    root = new_repo()
    (root / "venv").mkdir()
    check("venv accepted", run(root), expect=None)

    # A developer-local pattern must be a directory: `.gitignore` ignores `.idea/`, not `.idea`.
    root = new_repo()
    (root / ".idea").write_text("")
    check("file matching a local pattern rejected", run(root), expect="not a real directory")

    # A developer-local tree must actually be gitignored.
    root = new_repo(ignores="build/\n")
    (root / ".idea").mkdir()
    check("non-gitignored local tree rejected", run(root), expect="is not gitignored")

    # A developer-local tree may never hold committed content, even force-added.
    root = new_repo()
    (root / ".serena").mkdir()
    (root / ".serena" / "leaked.txt").write_text("secret\n")
    subprocess.run(["git", "-C", str(root), "add", "-f", ".serena/leaked.txt"], check=True)
    subprocess.run(["git", "-C", str(root), "commit", "-qm", "leak"], check=True)
    check("committed content in a local tree rejected", run(root),
          expect="may never hold committed content")

    print("\nlane A submission shape:")
    check_submission("complete bundle accepted", _complete, expect=None)

    def missing_raw(sub):
        _complete(sub)
        (sub / "raw" / "rep0.json").unlink()
        (sub / "raw").rmdir()
    check_submission("missing raw/ rejected", missing_raw, expect="missing 'raw'")

    def empty_raw(sub):
        _complete(sub)
        (sub / "raw" / "rep0.json").unlink()
    check_submission("empty raw/ rejected", empty_raw, expect="raw/ is empty")

    def no_manifest(sub):
        _complete(sub)
        (sub / "manifest.json").unlink()
    check_submission("missing manifest rejected", no_manifest, expect="missing 'manifest.json'")

    def raw_is_file(sub):
        _complete(sub)
        (sub / "raw" / "rep0.json").unlink()
        (sub / "raw").rmdir()
        (sub / "raw").write_text("not a directory")
    check_submission("raw as a file rejected", raw_is_file, expect="raw must be a directory")

    def manifest_is_dir(sub):
        _complete(sub)
        (sub / "manifest.json").unlink()
        (sub / "manifest.json").mkdir()
    check_submission("artifact as a directory rejected", manifest_is_dir,
                     expect="manifest.json must be a regular file")

    def stray(sub):
        _complete(sub)
        (sub / "notes.txt").write_text("hi")
    check_submission("stray file rejected", stray, expect="not part of a community-run bundle")

    root = new_repo()
    (root / "submission").write_text("not a bundle")
    mod = load_repo_lint()
    mod.ROOT = root
    mod.errors.clear()
    mod.check_lane_a_submission()
    check("submission as a file rejected", list(mod.errors),
          expect="must be the Lane A bundle directory, not a file")

    if failures:
        print("\nrepo_lint allow-list self-tests: FAIL", file=sys.stderr)
        for f in failures:
            print(f"  - {f}", file=sys.stderr)
        return 1
    print("repo_lint allow-list self-tests: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
