#!/usr/bin/env python3
"""Unit tests for scripts/adr_governance.py (stdlib unittest, no pytest).

Run: python tests/test_adr_governance.py

Proves the governance checks FIRE on drift and PASS on a consistent set, and
that the live docs/adr tree is lint-clean and fully indexed.
"""
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "scripts"))
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


if __name__ == "__main__":
    unittest.main(verbosity=2)
