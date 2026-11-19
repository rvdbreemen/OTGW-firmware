#!/usr/bin/env python3
"""TASK-1804 (Bug 7 part): the PIC's "Error 01" line is counted, like "Error 02".."Error 04".

The PIC prints "Error NN" as its own line (PrintError, other-projects/otgw-6.6/gateway.asm, ErrorStr "Error ").
handlePICSerial() cuts every PIC line at CR or LF before it is queued (OTGW-Core.ino, `outByte == '\r' || outByte ==
'\n'`), so the line that reaches the matcher never contains CR/LF. The matcher looked for "\r\nError 01" (since
25663ea01), so error01 never counted on any board; "Error 02".."Error 04" matched without CR/LF.

Source test on the real matcher literals: each strstr_P(buf, PSTR("...")) pattern of the Error 0N chain must be found
in the line as the reader delivers it. Old-vs-fix: OTGW_CORE_INO=<older OTGW-Core.ino> python tests/test_core_error_lines.py
"""
import os
import re
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
CORE = Path(os.environ.get("OTGW_CORE_INO", REPO / "src" / "OTGW-firmware" / "OTGW-Core.ino"))
SRC = CORE.read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")


def c_unescape(s):
    return s.replace(r"\r", "\r").replace(r"\n", "\n")


class ErrorLines(unittest.TestCase):
    def test_the_reader_cuts_lines_at_cr_and_lf(self):
        self.assertIn("if (outByte == '\\r' || outByte == '\\n') {", SRC)

    def test_every_error_pattern_matches_a_line_without_crlf(self):
        found = {}
        for m in re.finditer(r'strstr_P\(buf, PSTR\("([^"]*Error 0([1-4]))"\)\)', SRC):
            found[m.group(2)] = c_unescape(m.group(1))
        self.assertEqual(sorted(found), ["1", "2", "3", "4"], f"matcher patterns found: {found}")
        for n, pattern in sorted(found.items()):
            with self.subTest(error=n):
                line = f"Error 0{n}"   # as handlePICSerial() delivers it: no CR/LF
                self.assertIn(pattern, line, f"pattern {pattern!r} never matches the line {line!r}")


if __name__ == "__main__":
    unittest.main(verbosity=2)
