"""TASK-1630: docs/sat/sergeant-d-corrections.index.json covers the register and is current.

The register (docs/sat/sergeant-d-corrections.md) is immutable; its provenance metadata lives in
the generated JSON index next to it. This test fails when an entry has no index record, when the
ids differ, when a field holds a value outside the schema, or when the index is stale.
Refresh with: python scripts/sat_corrections_index.py --update

Run: python tests/test_sat_corrections_index.py
"""
import json
import os
import re
import sys
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(REPO, "scripts"))
import sat_corrections_index as idx  # noqa: E402

SOURCES = {"sergeant-d", "maintainer", "port-team", "unknown"}
KINDS = {"correction", "answer", "decision", "interpretation", "unclassified"}


class CorrectionsIndex(unittest.TestCase):
    def setUp(self):
        self.assertTrue(idx.INDEX.exists(), "index missing: run python scripts/sat_corrections_index.py --update")
        with open(idx.INDEX, encoding="utf-8") as f:
            self.index = json.load(f)
        self.records = self.index["entries"]
        text = idx.REGISTER.read_text(encoding="utf-8")
        self.register_ids = [int(n) for n in re.findall(r"^## (\d+)\.", text, re.M)]

    def test_every_entry_has_one_record(self):
        self.assertEqual([r["id"] for r in self.records], self.register_ids)

    def test_fields_follow_the_schema(self):
        ids = {r["id"] for r in self.records}
        for r in self.records:
            self.assertIn(r["source"], SOURCES, r["id"])
            self.assertIn(r["kind"], KINDS, r["id"])
            if r["confirmed_by_entry"] is not None:
                self.assertIn(r["confirmed_by_entry"], ids, r["id"])
                self.assertGreater(r["confirmed_by_entry"], r["id"], "confirmation must come later")
            for s in r["supersedes"]:
                self.assertIn(s, ids, r["id"])

    def test_index_is_current(self):
        self.assertEqual(idx.INDEX.read_text(encoding="utf-8").replace("\r\n", "\n"),
                         idx.render(idx.build()),
                         "index is stale: run python scripts/sat_corrections_index.py --update")

    def test_open_questions_answers_point_at_index_entries(self):
        # TASK-1630 AC3: every "answered" checkmark in docs/sat/open-questions.md names an entry
        # that exists in the index, so the queue and the register cannot drift apart silently.
        ids = {r["id"] for r in self.records}
        text = open(os.path.join(REPO, "docs", "sat", "open-questions.md"), encoding="utf-8").read()
        missing = []
        for line in text.splitlines():
            m = re.match(r"^\| *((?:Q|Conf|QD)[\w.-]*)[^|]*\| *✓\s*(\d+)", line)
            if m and int(m.group(2)) not in ids:
                missing.append(f"{m.group(1)} -> entry {m.group(2)}")
        self.assertEqual(missing, [], "answered questions name entries the register does not have")

    def test_known_provenance(self):
        # Spot checks against the register text (2026-11-05): 257-264 are maintainer decisions
        # that Sergeant D confirmed in entry 265; entry 108 is our reading, confirmed by 131.
        by_id = {r["id"]: r for r in self.records}
        for n in range(257, 265):
            self.assertEqual(by_id[n]["source"], "maintainer", n)
            self.assertEqual(by_id[n]["confirmed_by_entry"], 265, n)
        self.assertEqual(by_id[265]["source"], "sergeant-d")
        self.assertEqual(by_id[108]["source"], "port-team")
        self.assertEqual(by_id[108]["confirmed_by_entry"], 131)


if __name__ == "__main__":
    unittest.main(verbosity=2)
