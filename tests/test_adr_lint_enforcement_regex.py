#!/usr/bin/env python3
"""TASK-1200: bin/adr-lint's ENFORCEMENT_BLOCK_RE must not backtrack exponentially.

The v0.15.0 pattern, (?:.*?\\n)*?, nests a quantifier under DOTALL. On an ADR whose
'## Enforcement' section holds no ```json block (ADR-116) the policy gate then ran for
24 h. Each search runs in a subprocess with a time limit, so a regression fails the
test instead of hanging it.

    python tests/test_adr_lint_enforcement_regex.py
    ADR_LINT=/path/to/adr-lint python tests/test_adr_lint_enforcement_regex.py   # another copy
"""
import os
import subprocess
import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
ADR_LINT = Path(os.environ.get("ADR_LINT", REPO / "bin" / "adr-lint"))
LIMIT_S = 10

PROSE_ONLY = ("# ADR-999 Prose-only enforcement\n\n## Enforcement\n\n"
              + "Manual review only. No mechanical rule expresses this decision.\n" * 40
              + "\n## Related Decisions\n\n" + "- ADR-001 something related.\n" * 60)
WITH_JSON = ("# ADR-998 JSON enforcement\n\n## Enforcement\n\nThe rule:\n\n```json\n"
             '{"forbid_pattern": []}\n```\n\n## Related Decisions\n\n- none\n')

# Runs in a child process: load the script under test, search the text from stdin,
# print the captured JSON block (or None).
CHILD = r"""
import runpy, sys
ns = runpy.run_path(sys.argv[1])
m = ns["ENFORCEMENT_BLOCK_RE"].search(sys.stdin.read())
print(repr(m.group(1) if m else None))
"""


def search(text):
    r = subprocess.run([sys.executable, "-c", CHILD, str(ADR_LINT)], input=text,
                       capture_output=True, text=True, encoding="utf-8", timeout=LIMIT_S)
    if r.returncode != 0:
        raise AssertionError(f"child failed: {r.stderr[-400:]}")
    return r.stdout.strip()


class EnforcementRegex(unittest.TestCase):
    def test_prose_only_section_finishes_and_finds_nothing(self):
        try:
            self.assertEqual(search(PROSE_ONLY), "None")
        except subprocess.TimeoutExpired:
            self.fail(f"ENFORCEMENT_BLOCK_RE did not finish within {LIMIT_S} s on a prose-only "
                      "Enforcement section (catastrophic backtracking)")

    def test_json_block_is_still_found(self):
        self.assertEqual(search(WITH_JSON), repr('{"forbid_pattern": []}'))


if __name__ == "__main__":
    unittest.main(verbosity=2)
