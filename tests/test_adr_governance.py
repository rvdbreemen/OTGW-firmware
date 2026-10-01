#!/usr/bin/env python3
"""Unit tests for scripts/adr_governance.py (stdlib unittest, no pytest).

Run: python tests/test_adr_governance.py

Proves the governance checks FIRE on drift and PASS on a consistent set, and
that the live docs/adr tree is lint-clean and fully indexed.
"""
import os
import re
import sys
import tempfile
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(REPO, "scripts"))
import adr_governance as gov  # noqa: E402


def adr(num, status="Accepted", supersedes=None, superseded_by=None,
        named_gates=None, is_binding=False):
    return {
        "num": num, "path": f"ADR-{num}.md", "title": f"ADR-{num}",
        "status_raw": status, "status": gov.status_category(status),
        "supersedes": supersedes or [], "superseded_by": superseded_by or [],
        "named_gates": named_gates or [], "is_binding": is_binding,
        "has_frontmatter": True,
    }


class LintTests(unittest.TestCase):
    def test_clean_set_passes(self):
        adrs = {1: adr(1), 2: adr(2, "Superseded by ADR-3", superseded_by=[3]), 3: adr(3, supersedes=[2], superseded_by=[])}
        fails, warns = gov.cmd_lint(adrs, gates=set())
        self.assertEqual(fails, [])

    def test_superseded_by_dangling_fails(self):
        adrs = {1: adr(1, "Superseded by ADR-99", superseded_by=[99])}
        fails, _ = gov.cmd_lint(adrs, gates=set())
        self.assertTrue(any("does not exist" in f for f in fails))

    def test_status_link_mismatch_warns(self):
        # superseded_by populated but status still Accepted
        adrs = {1: adr(1, "Accepted", superseded_by=[2]), 2: adr(2)}
        _, warns = gov.cmd_lint(adrs, gates=set())
        self.assertTrue(any("expected Superseded" in w for w in warns))

    def test_supersedes_missing_backlink_warns(self):
        # A supersedes B, but B has an EMPTY superseded_by (no backlink at all)
        adrs = {1: adr(1, supersedes=[2]), 2: adr(2, superseded_by=[])}
        _, warns = gov.cmd_lint(adrs, gates=set())
        self.assertTrue(any("does not list 1" in w for w in warns))

    def test_binding_adr_missing_gate_fails(self):
        adrs = {1: adr(1, "Accepted", named_gates=["check_missing"], is_binding=True)}
        fails, _ = gov.cmd_lint(adrs, gates={"check_present"})
        self.assertTrue(any("check_missing" in f for f in fails))

    def test_binding_adr_present_gate_ok(self):
        adrs = {1: adr(1, "Accepted", named_gates=["check_present"], is_binding=True)}
        fails, _ = gov.cmd_lint(adrs, gates={"check_present"})
        self.assertEqual(fails, [])


class ParseTests(unittest.TestCase):
    """'binding' comes from the frontmatter only (TASK-1183, maintainer decision)."""
    def parse(self, text):
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "ADR-900-test.md")
            with open(path, "w", encoding="utf-8") as f:
                f.write(text)
            return gov.parse_adr(path)

    def test_frontmatter_binding_true_is_binding(self):
        a = self.parse("---\nstatus: \"Accepted\"\nbinding: true\ngate: \"check_x\"\n---\n# ADR-900\n\nNo such word here.\n")
        self.assertTrue(a["is_binding"])

    def test_the_word_binding_alone_is_not_binding(self):
        # ADR-167's case: 'Binding' only in the title of ADR-080 under Related.
        a = self.parse("---\nstatus: \"Accepted\"\nbinding: false\ngate: null\n---\n# ADR-900\n\n"
                       "## Related\n- ADR-080: Binding ADR rules must have a CI gate\n")
        self.assertFalse(a["is_binding"])

    def test_no_frontmatter_is_not_binding(self):
        a = self.parse("# ADR-900\n\n**Status:** Accepted\n\nBinding rule text.\n")
        self.assertFalse(a["is_binding"])


def adr_list_block(path):
    """The ADR-list block of an instructions file: from the '**Binding ADRs**' line up to
    'Accepted ADRs are binding'. CLAUDE.md and AGENTS.md carry it word for word."""
    with open(path, encoding="utf-8") as f:
        lines = f.read().replace("\r\n", "\n").split("\n")
    start = next(i for i, l in enumerate(lines) if l.startswith("**Binding ADRs**"))
    end = next(i for i in range(start, len(lines)) if lines[i].startswith("Accepted ADRs are binding"))
    return lines[start:end]


def listed_binding(block):
    """ADR numbers listed under '**Binding ADRs**', up to the structural list."""
    nums = set()
    for line in block[1:]:
        if line.startswith("**Structural"):
            break
        m = re.match(r"^- \*\*ADR-(\d+)\*\*", line)
        if m:
            nums.add(int(m.group(1)))
    return nums


class IndexCheckTests(unittest.TestCase):
    def test_all_indexed_passes(self):
        adrs = {1: adr(1), 2: adr(2)}
        fails = gov.cmd_index_check(adrs, counts={1: 1, 2: 1})
        self.assertEqual(fails, [])

    def test_missing_entry_fails(self):
        adrs = {1: adr(1), 2: adr(2)}
        fails = gov.cmd_index_check(adrs, counts={1: 1})
        self.assertTrue(any("ADR-2" in f and "MISSING" in f for f in fails))

    def test_duplicate_entry_fails(self):
        adrs = {1: adr(1)}
        fails = gov.cmd_index_check(adrs, counts={1: 2})
        self.assertTrue(any("ADR-1" in f and "duplicate" in f for f in fails))


class LiveTreeTests(unittest.TestCase):
    """Guards the real docs/adr tree so drift is caught in CI."""
    def setUp(self):
        self.adrs = gov.load_all()

    def test_live_lint_clean(self):
        fails, _ = gov.cmd_lint(self.adrs, gov.evaluate_gate_names())
        self.assertEqual(fails, [], "docs/adr lint failures:\n" + "\n".join(fails))

    def test_live_index_complete(self):
        fails = gov.cmd_index_check(self.adrs, gov.readme_entries())
        self.assertEqual(fails, [], "docs/adr index failures:\n" + "\n".join(fails))

    def test_instruction_lists_match_frontmatter(self):
        # The Accepted ADRs whose frontmatter says binding: true are exactly the
        # ones CLAUDE.md lists as binding (TASK-1183 AC#3), and AGENTS.md carries
        # the same ADR-list block word for word (maintainer, 2026-10-01).
        want = {n for n, a in self.adrs.items() if a["is_binding"] and a["status"] == "Accepted"}
        claude = adr_list_block(os.path.join(REPO, "CLAUDE.md"))
        agents = adr_list_block(os.path.join(REPO, "AGENTS.md"))
        self.assertEqual(sorted(listed_binding(claude)), sorted(want), "CLAUDE.md binding list vs frontmatter")
        self.assertEqual(agents, claude, "the ADR-list block differs between AGENTS.md and CLAUDE.md")


if __name__ == "__main__":
    unittest.main(verbosity=2)
