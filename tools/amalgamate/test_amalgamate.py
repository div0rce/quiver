#!/usr/bin/env python3
"""Unit tests for the amalgamation generator (MOD-AMALG; REQ-INT-005, REQ-BUILD-013, ADR-018).

Covers the generator's determinism and output invariants plus the REQ-STD-006 `--check` rules.
Stdlib `unittest` only (REQ-INT-004). The heavier "compile the pair + run the unit suite"
integration gate is the CMake `quiver_tests_unit_amalgamated` target (REQ-BUILD-013)."""

import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import amalgamate  # noqa: E402


class TestCheckRules(unittest.TestCase):
    """REQ-STD-006 amalgamation-compatibility rules, exercised on synthetic snippets."""

    def test_clean_header_passes(self):
        text = '#pragma once\n#include <cstdint>\n#include "quiver/core.h"\nint f();\n'
        self.assertEqual(amalgamate.check_file("x.h", text, is_header=True), [])

    def test_header_missing_pragma_once(self):
        errs = amalgamate.check_file("x.h", "int f();\n", is_header=True)
        self.assertTrue(any("missing `#pragma once`" in e for e in errs))

    def test_include_guard_macro_rejected(self):
        text = "#ifndef QUIVER_X_H\n#define QUIVER_X_H\nint f();\n#endif\n"
        errs = amalgamate.check_file("x.h", text, is_header=True)
        self.assertTrue(any("include-guard macro" in e for e in errs))

    def test_project_header_must_be_quoted(self):
        errs = amalgamate.check_file("x.cpp", "#include <quiver/core.h>\n", is_header=False)
        self.assertTrue(any("must be quoted" in e for e in errs))

    def test_internal_include_must_be_project_relative(self):
        errs = amalgamate.check_file("x.cpp", '#include "../core.h"\n', is_header=False)
        self.assertTrue(any("not project-relative" in e for e in errs))

    def test_include_inside_namespace_rejected(self):
        text = '#pragma once\nnamespace n {\n#include "quiver/core.h"\n}\n'
        errs = amalgamate.check_file("x.h", text, is_header=True)
        self.assertTrue(any("inside a namespace" in e for e in errs))

    def test_real_tree_is_clean(self):
        self.assertEqual(amalgamate.check(), 0)


class TestStrip(unittest.TestCase):
    def test_pragma_once_and_internal_includes_dropped(self):
        text = '#pragma once\n#include "quiver/core.h"\n#include "src/x.h"\n#include <bit>\nCODE\n'
        angles: set[str] = set()
        out = "\n".join(amalgamate._strip(text, drop_pragma_once=True, angle_sink=angles))
        self.assertNotIn("#pragma once", out)
        self.assertNotIn('#include "quiver/core.h"', out)
        self.assertNotIn('#include "src/x.h"', out)
        self.assertIn("CODE", out)
        self.assertEqual(angles, {"bit"})  # hoisted, not left in place

    def test_angle_includes_kept_in_place_when_no_sink(self):
        out = "\n".join(amalgamate._strip("#include <immintrin.h>\nX\n",
                                          drop_pragma_once=False, angle_sink=None))
        self.assertIn("#include <immintrin.h>", out)


class TestGenerate(unittest.TestCase):
    def _gen(self, d: Path):
        amalgamate.generate(d)
        return (d / "quiver.h").read_text(), (d / "quiver.cpp").read_text()

    def test_deterministic(self):
        with tempfile.TemporaryDirectory() as a, tempfile.TemporaryDirectory() as b:
            h1, c1 = self._gen(Path(a))
            h2, c2 = self._gen(Path(b))
            self.assertEqual(h1, h2)
            self.assertEqual(c1, c2)

    def test_output_invariants(self):
        with tempfile.TemporaryDirectory() as d:
            header, cpp = self._gen(Path(d))
            # Exactly one #pragma once (the amalgamation's own, at the top of the header).
            self.assertEqual(header.count("#pragma once"), 1)
            self.assertEqual(cpp.count("#pragma once"), 0)
            # No internal quoted includes survive (all inlined) — except quiver.cpp's own header.
            for body in (header, cpp):
                self.assertNotIn('#include "quiver/core.h"', body)
                self.assertNotIn('#include "src/', body)
            self.assertIn('#include "quiver.h"', cpp)
            # System includes are hoisted into the header (sorted block near the top).
            self.assertIn("#include <cstdint>", header)


if __name__ == "__main__":
    unittest.main(verbosity=2)
