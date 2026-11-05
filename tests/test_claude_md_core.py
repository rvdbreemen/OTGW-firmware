"""TASK-1631: CLAUDE.md stays a small core and every file it points to exists.

CLAUDE.md is loaded in full in every session, so it holds only the core (roles, tasks, prohibitions, gates, push
policy, the binding-ADR list) and points to .claude/rules/ (auto-loaded by Claude Code) and docs/guides/ for the
rest. This test fails when the core grows past its budget again, when a pointer leads nowhere, or when a
path-scoped rule loses its `paths:` frontmatter (it would then load in every session).

Run: python tests/test_claude_md_core.py
"""
import os
import re
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAX_LINES = 230
ALWAYS = ("sat-ground-truth.md", "discord.md")
SCOPED = ("firmware-coding.md", "navigation.md")


def read(rel):
    with open(os.path.join(REPO, rel), encoding="utf-8") as f:
        return f.read().replace("\r\n", "\n")


def frontmatter(text):
    m = re.match(r"---\n(.*?)\n---\n", text, re.S)
    return m.group(1) if m else None


class ClaudeMdCore(unittest.TestCase):
    def test_core_stays_small(self):
        n = len(read("CLAUDE.md").rstrip("\n").split("\n"))
        self.assertLessEqual(n, MAX_LINES, "CLAUDE.md has %d lines; move detail to .claude/rules or docs/guides" % n)

    def test_pointers_exist(self):
        text = read("CLAUDE.md")
        refs = set(re.findall(r"`((?:\.claude/rules|docs/guides|docs/process)/[A-Za-z0-9_.-]+\.md)`", text))
        self.assertTrue(refs, "CLAUDE.md points to no rules or guides")
        for name in ALWAYS + SCOPED:
            self.assertIn(".claude/rules/" + name, refs, "CLAUDE.md does not point to .claude/rules/" + name)
        for rel in sorted(refs):
            self.assertTrue(os.path.isfile(os.path.join(REPO, rel)), "CLAUDE.md points to a missing file: " + rel)

    def test_always_loaded_rules_have_no_paths(self):
        for name in ALWAYS:
            fm = frontmatter(read(".claude/rules/" + name))
            self.assertTrue(fm is None or "paths" not in fm, name + " must load in every session (no paths:)")

    def test_scoped_rules_have_paths(self):
        for name in SCOPED:
            fm = frontmatter(read(".claude/rules/" + name))
            self.assertIsNotNone(fm, name + " has no frontmatter")
            self.assertRegex(fm, r"(?m)^paths:\s*$\n(\s+- \"[^\"]+\"\n?)+", name + " has no paths: list")

    def test_core_keeps_the_guarded_blocks(self):
        text = read("CLAUDE.md")
        self.assertIn("<!-- AGENT-CORE START", text)
        self.assertIn("**Binding ADRs**", text)
        self.assertNotIn("SAT-GROUND-TRUTH START", text, "the SAT block lives in .claude/rules/sat-ground-truth.md")


if __name__ == "__main__":
    unittest.main()
