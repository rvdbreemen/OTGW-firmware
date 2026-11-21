"""TASK-1292: the 'SAT ground truth' rules must not drift.

.claude/rules/sat-ground-truth.md (split out of CLAUDE.md by TASK-1631, loaded in every session) and
AGENTS.md both carry the block between the SAT-GROUND-TRUTH markers, word for word, and since the maintainer directive of 2026-11-21 (TASK-3013) it ranks our own implementation and the recorded
decisions above Alex Wijn's SAT, which is a reference (other-projects/SAT-develop/) only.
Mirrors tests/test_adr_governance.py, which guards the binding-ADR block the same way.

Run: python tests/test_sat_ground_truth_block.py
"""
import os
import re
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
START = "<!-- SAT-GROUND-TRUTH START"
END = "<!-- SAT-GROUND-TRUTH END -->"
RULE = ".claude/rules/sat-ground-truth.md"
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
        for name in (RULE, "AGENTS.md"):
            self.assertIsNotNone(block(name), name + " has no SAT-GROUND-TRUTH block")

    def test_blocks_are_identical(self):
        self.assertEqual(block(RULE), block("AGENTS.md"),
                         "the SAT ground truth block differs between " + RULE + " and AGENTS.md")

    def test_block_makes_our_implementation_leading(self):
        # Maintainer directive 2026-11-21: our SAT implementation plus the recorded decisions is
        # leading; Alex Wijn's SAT is a reference; Sergeant D stays the designer.
        b = block(RULE)
        self.assertIn("our implementation is leading", b)
        self.assertIn("De implementatie die wij hebben van SAT is leidend", b)
        self.assertIn("Reference, not leading", b)
        self.assertIn("other-projects/SAT-develop/", b)
        self.assertIn("Sergeant D stays the SAT designer", b)
        self.assertNotIn("other-projects/SAT-releases-thermo-nova/", b)
        # the old ranking put thermo-nova second, above our firmware
        self.assertNotIn("2. The thermo-nova code", b)
        self.assertIn(PINNED, b, "the historical port commit must stay named")
        self.assertIn("not as ground truth", b)
        self.assertIn("pros and cons", b)
        self.assertIn("replay the same data through both implementations", b)
        for rule in ("No citation, no claim", "not found in the code", "time order",
                     "second reader", "only on purpose"):
            self.assertIn(rule, b, "rule missing from the block: " + rule)

    def test_block_requires_recording_sergeant_d_decisions(self):
        # Maintainer directive 2026-10-23 (TASK-1329): every correction or decision by
        # Sergeant D goes into one named file, in the same session, before implementing.
        b = block(RULE)
        self.assertIn("docs/sat/decisions/sergeant-d-corrections.md", b)
        self.assertIn("in the same session and before the change it leads to is implemented", b)
        # TASK-1330: decisions are checked against the recorded entries, conflicts go to him.
        self.assertIn("checked against Sergeant D's recorded decisions before it is taken", b)
        self.assertIn("never resolved silently", b)
        self.assertIn("The register is private since 2026-11-21", b)
        self.assertIn("rvdbreemen/OTGW-sat-decisions", b)
        self.assertIn("refer to entries by number and never quote them", b)

    def test_register_file_exists_when_the_submodule_is_checked_out(self):
        # The register is private (rvdbreemen/OTGW-sat-decisions); public CI cannot fetch the submodule.
        sub = os.path.join(REPO, "docs", "sat", "decisions")
        if not (os.path.isdir(sub) and os.listdir(sub)):
            self.skipTest("docs/sat/decisions submodule not checked out (private repo)")
        self.assertTrue(os.path.exists(os.path.join(sub, "sergeant-d-corrections.md")),
                        "docs/sat/decisions/sergeant-d-corrections.md is missing")

    def test_reference_snapshot_records_its_upstream_commit(self):
        # The reference snapshot carries an UPSTREAM_COMMIT marker; the block names that commit.
        marker = os.path.join(REPO, "other-projects", "SAT-develop", "UPSTREAM_COMMIT")
        if not os.path.exists(marker):
            self.skipTest("other-projects submodule not checked out")
        with open(marker, encoding="utf-8") as f:
            commit = f.read().strip().split()[0]
        self.assertIn(commit[:7], block(RULE))

    def test_navigation_rows_point_at_the_spec(self):
        # TASK-1631: the navigation table moved from CLAUDE.md to a path-scoped rule.
        for name in (".claude/rules/navigation.md", "AGENTS.md"):
            with open(os.path.join(REPO, name), encoding="utf-8") as f:
                text = f.read()
            row = re.search(r"^\| SAT/BLE feature \|.*$", text, re.M)
            self.assertIsNotNone(row, name + ": SAT/BLE navigation row missing")
            self.assertIn("docs/sat/thermo-nova-spec.md", row.group(0), name)


if __name__ == "__main__":
    unittest.main(verbosity=2)
