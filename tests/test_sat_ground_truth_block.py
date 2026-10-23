"""TASK-1292: the 'SAT ground truth' rules must not drift.

CLAUDE.md and AGENTS.md both carry the block between the SAT-GROUND-TRUTH markers, word for
word, and it pins the thermo-nova commit that the local copy and docs/sat/ are written against.
Mirrors tests/test_adr_governance.py, which guards the binding-ADR block the same way.

Run: python tests/test_sat_ground_truth_block.py
"""
import os
import re
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
START = "<!-- SAT-GROUND-TRUTH START"
END = "<!-- SAT-GROUND-TRUTH END -->"
PINNED = "7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb"


def block(name):
    with open(os.path.join(REPO, name), encoding="utf-8") as f:
        text = f.read().replace("\r\n", "\n")
    i, j = text.find(START), text.find(END)
    if i < 0 or j < 0:
        return None
    return text[i:j + len(END)]


class SatGroundTruthBlock(unittest.TestCase):
    def test_both_files_carry_the_block(self):
        for name in ("CLAUDE.md", "AGENTS.md"):
            self.assertIsNotNone(block(name), name + " has no SAT-GROUND-TRUTH block")

    def test_blocks_are_identical(self):
        self.assertEqual(block("CLAUDE.md"), block("AGENTS.md"),
                         "the SAT ground truth block differs between CLAUDE.md and AGENTS.md")

    def test_block_pins_the_thermo_nova_commit_and_the_rules(self):
        b = block("CLAUDE.md")
        self.assertIn(PINNED, b, "pinned thermo-nova commit missing")
        self.assertIn("releases/thermo-nova", b)
        self.assertIn("other-projects/SAT-releases-thermo-nova/", b)
        self.assertIn("docs/sat/thermo-nova-spec.md", b)
        for rule in ("No citation, no claim", "not found in the code", "time order",
                     "Sergeant D", "second reader", "pinned commit only on purpose"):
            self.assertIn(rule, b, "rule missing from the block: " + rule)

    def test_block_requires_recording_sergeant_d_decisions(self):
        # Maintainer directive 2026-10-23 (TASK-1329): every correction or decision by
        # Sergeant D goes into one named file, in the same session, before implementing.
        b = block("CLAUDE.md")
        self.assertIn("docs/sat/sergeant-d-corrections.md", b)
        self.assertIn("in the same session and before the change it leads to is implemented", b)
        self.assertTrue(os.path.exists(os.path.join(REPO, "docs", "sat", "sergeant-d-corrections.md")),
                        "docs/sat/sergeant-d-corrections.md is missing")

    def test_pinned_commit_matches_the_local_copy_when_it_is_a_git_repo(self):
        # The local copy is a snapshot (no upstream history), so only check the marker file
        # that records which commit it mirrors, when present.
        marker = os.path.join(REPO, "other-projects", "SAT-releases-thermo-nova", "UPSTREAM_COMMIT")
        if not os.path.exists(marker):
            self.skipTest("no UPSTREAM_COMMIT marker in the local copy")
        with open(marker, encoding="utf-8") as f:
            self.assertEqual(f.read().strip().split()[0], PINNED)

    def test_navigation_rows_point_at_the_spec(self):
        for name in ("CLAUDE.md", "AGENTS.md"):
            with open(os.path.join(REPO, name), encoding="utf-8") as f:
                text = f.read()
            row = re.search(r"^\| SAT/BLE feature \|.*$", text, re.M)
            self.assertIsNotNone(row, name + ": SAT/BLE navigation row missing")
            self.assertIn("docs/sat/thermo-nova-spec.md", row.group(0), name)


if __name__ == "__main__":
    unittest.main(verbosity=2)
