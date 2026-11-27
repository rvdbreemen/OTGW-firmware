"""TASK-3111: no em dash (U+2014) in the text people read in the docs.

Robert's rule: no em dash in any text a person reads. Use a colon when the second half
explains, a period when two thoughts stand alone, a comma or parentheses for an aside.
Fenced code blocks and inline code spans are left alone (they quote code, JSON or a
literal format). Already published release notes (docs/releases) and historic material
(ADRs, plans, reviews, research, backlog) are not checked here.
"""
import glob
import os
import re
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EM = "—"

PATTERNS = [
    "README.md",
    "CHANGELOG.md",
    "docs/guides/*.md",
    "docs/hardware/*.md",
    "docs/process/*.md",
    "docs/manuals/en/*.md",
    "docs/manuals/nl/*.md",
    "docs/api/*.md",
    "docs/MANUAL.md",
    "docs/features/*.md",
    "docs/templates/*.md",
    "docs/BREAKING_CHANGES.md",
]


def offending_lines(path):
    with open(path, encoding="utf-8") as f:
        text = f.read()
    hits = []
    fence = False
    for n, line in enumerate(text.split("\n"), 1):
        if line.lstrip().startswith("```"):
            fence = not fence
            continue
        if fence or EM not in line:
            continue
        stripped = re.sub(r"`[^`]*`", "", line)
        if EM in stripped:
            hits.append((n, line.strip()[:100]))
    return hits


class DocsNoEmDash(unittest.TestCase):
    def test_no_em_dash_in_reader_text(self):
        bad = []
        for pat in PATTERNS:
            for path in sorted(glob.glob(os.path.join(REPO, pat))):
                for n, line in offending_lines(path):
                    bad.append("%s:%d: %s" % (os.path.relpath(path, REPO), n, line))
        self.assertEqual(bad, [], "em dash in reader text (use a colon, a period, a comma or parentheses):\n" + "\n".join(bad[:40]))

    def test_the_checker_catches_a_dash(self):
        import tempfile
        with tempfile.NamedTemporaryFile("w", suffix=".md", delete=False, encoding="utf-8") as f:
            f.write("Fine line.\nBad " + EM + " line.\n`code " + EM + " span` is ok.\n```\nfenced " + EM + " ok\n```\n")
            name = f.name
        try:
            self.assertEqual([n for n, _ in offending_lines(name)], [2])
        finally:
            os.unlink(name)


if __name__ == "__main__":
    unittest.main()
