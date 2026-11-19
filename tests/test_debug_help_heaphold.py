"""TASK-2924: the telnet help menu ('h') lists the prerelease heap-hold key H, and 'l' (toggle MyDEBUG)
no longer prints that help line. Reads the real handleDebug.ino and cuts each case block up to its break."""
import re
import unittest
from pathlib import Path

SRC = (Path(__file__).resolve().parent.parent / "src" / "OTGW-firmware" / "handleDebug.ino").read_text(
    encoding="utf-8").replace("\r\n", "\n")
H_LINE = 'Debugln(F("  H) bench: starve the heap / release it (prerelease builds only)"));'


def case_block(key):
    m = re.search(r"\n\s*case '%s':" % re.escape(key), SRC)
    assert m, "case '%s' not found" % key
    end = SRC.index("break;", m.end())
    return SRC[m.end():end]


class DebugHelpHeapHold(unittest.TestCase):
    def test_the_help_menu_lists_the_H_key_in_prerelease_builds(self):
        block = case_block("h")
        self.assertIn(H_LINE, block)
        before = block[:block.index(H_LINE)]
        self.assertGreater(before.rfind("#if defined(_VERSION_PRERELEASE)"), before.rfind("#endif"),
                           "the H) line must sit inside a _VERSION_PRERELEASE guard")

    def test_toggle_mydebug_does_not_print_the_H_help_line(self):
        self.assertNotIn("H) bench", case_block("l"))

    def test_the_H_line_appears_once(self):
        self.assertEqual(SRC.count(H_LINE), 1)


if __name__ == "__main__":
    unittest.main()
