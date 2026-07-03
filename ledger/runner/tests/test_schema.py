"""QLS-1 accept/reject fixtures for the first-party schema checker (REQ-LEDGER-001;
PRD 11 §10 "schemas exist and validate all committed results")."""

import copy
import json
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from qledger import schema_check  # noqa: E402

FIXTURES = pathlib.Path(__file__).parent / "fixtures"


def load(name: str):
    return json.loads((FIXTURES / name).read_text())


class TestEntrySchema(unittest.TestCase):
    def test_accept_valid_entry(self):
        self.assertEqual(schema_check.validate_entry(load("entry-valid.json")), [])

    def test_reject_missing_field(self):
        entry = load("entry-valid.json")
        del entry["machine_id"]
        self.assertTrue(any("machine_id" in e for e in schema_check.validate_entry(entry)))

    def test_reject_wrong_schema_version(self):
        entry = load("entry-valid.json")
        entry["schema"] = "QLS-0"
        self.assertTrue(any("QLS-1" in e for e in schema_check.validate_entry(entry)))

    def test_reject_unknown_variant(self):
        entry = load("entry-valid.json")
        entry["variant"] = "sse2"
        self.assertTrue(schema_check.validate_entry(entry))

    def test_reject_single_run(self):
        entry = load("entry-valid.json")
        entry["repetitions"] = 1
        self.assertTrue(any("unpublishable" in e for e in schema_check.validate_entry(entry)))

    def test_reject_no_pmu_with_cycles(self):
        entry = load("entry-valid.json")
        entry["results"]["cycles_per_value"] = copy.deepcopy(entry["results"]["ns_per_batch"])
        entry["metrics"] = sorted(entry["results"].keys())
        self.assertTrue(any("REQ-LEDGER-008" in e for e in schema_check.validate_entry(entry)))

    def test_reject_incomplete_results(self):
        entry = load("entry-valid.json")
        del entry["results"]["ns_per_batch"]["cv"]
        self.assertTrue(any("cv" in e for e in schema_check.validate_entry(entry)))


class TestManifestSchema(unittest.TestCase):
    def test_accept_valid_manifest(self):
        self.assertEqual(schema_check.validate_manifest(load("manifest-valid.json")), [])

    def test_reject_missing_field(self):
        m = load("manifest-valid.json")
        del m["frequency_governor"]
        self.assertTrue(any("frequency_governor" in e
                            for e in schema_check.validate_manifest(m)))

    def test_reject_nonstring_deviation(self):
        m = load("manifest-valid.json")
        m["deviations"] = [42]
        self.assertTrue(schema_check.validate_manifest(m))


if __name__ == "__main__":
    unittest.main()
