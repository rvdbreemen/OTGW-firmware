#!/usr/bin/env python3
"""TASK-1639 (GitHub #694): the firmware scans strings with siscanf, never sscanf.

newlib ships two string scanners: __ssvfscanf_r (sscanf, with floating point, 8,765 bytes
in the beta.86 esp32-combo ELF) and __ssvfiscanf_r (siscanf, integer only, 7,529 bytes).
The integer one is always linked, because newlib's own _tzset_unlocked_r calls siscanf.
The float one was linked only because the firmware and OTGWSerial called sscanf. Both are
built from the same newlib source (libc/stdio/vfscanf.c); INTEGER_ONLY only drops the
%e/%f/%g/%a conversions, so the integer formats parse the same.

This test fails when:
  1. a source file under src/OTGW-firmware/ or src/libraries/OTGWSerial/ calls sscanf
     (which would pull the float scanner back into the image), or
  2. a siscanf call has a floating-point conversion in its literal format (siscanf would
     silently not convert it).

Old-vs-fix: OTGW_SCAN_ROOT points at another checkout (or a `git archive` export) of the
repository, e.g. the beta.86 tree, which fails test 1.

Run: python tests/test_no_float_sscanf.py
"""
import os
import re
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
ROOT = Path(os.environ.get("OTGW_SCAN_ROOT", REPO))
DIRS = [ROOT / "src" / "OTGW-firmware", ROOT / "src" / "libraries" / "OTGWSerial"]
SUFFIXES = {".ino", ".h", ".cpp", ".c"}

SSCANF_CALL = re.compile(r"(?<![A-Za-z0-9_])sscanf\s*\(")
SISCANF_CALL = re.compile(r"(?<![A-Za-z0-9_])siscanf\s*\(([^;]*)")
FLOAT_CONV = re.compile(r"%[*]?[0-9]*(?:hh|h|ll|l|L)?[eEfFgGaA]")


def strip_comments(text):
    """Drop // and /* */ comments, keep string literals and line count."""
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if c == '"' or c == "'":
            q, j = c, i + 1
            while j < n and text[j] != q:
                j += 2 if text[j] == "\\" else 1
            out.append(text[i:j + 1])
            i = j + 1
        elif text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("\n" * text.count("\n", i, j))
            i = j
        else:
            out.append(c)
            i += 1
    return "".join(out)


def sources():
    for d in DIRS:
        for p in sorted(d.rglob("*")):
            if p.suffix in SUFFIXES and p.is_file():
                yield p, strip_comments(p.read_text(encoding="utf-8", errors="replace"))


def line_of(text, pos):
    return text.count("\n", 0, pos) + 1


class NoFloatScanf(unittest.TestCase):
    def test_sources_found(self):
        self.assertGreater(sum(1 for _ in sources()), 20, "source tree not found under " + str(ROOT))

    def test_no_sscanf_calls(self):
        hits = [f"{p.relative_to(ROOT)}:{line_of(t, m.start())}"
                for p, t in sources() for m in SSCANF_CALL.finditer(t)]
        self.assertEqual(hits, [], "sscanf links newlib's float scanner; use siscanf: " + ", ".join(hits))

    def test_siscanf_formats_are_integer_only(self):
        bad = []
        calls = 0
        for p, t in sources():
            for m in SISCANF_CALL.finditer(t):
                calls += 1
                for lit in re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(1)):
                    if FLOAT_CONV.search(lit.replace("%%", "")):
                        bad.append(f"{p.relative_to(ROOT)}:{line_of(t, m.start())} {lit!r}")
        self.assertEqual(bad, [], "siscanf has no floating point: " + ", ".join(bad))
        if ROOT == REPO:
            self.assertGreaterEqual(calls, 19, "expected the converted call sites")

    def test_float_pattern_self_check(self):
        for fmt in ("%f", "%lf", "%5f", "%e", "%g", "%*f"):
            self.assertRegex(fmt, FLOAT_CONV)
        for fmt in ("%d", "%u", "%8x", "%2x%4x%2x", "%d %n%*s%n %x %d %x", "boot#%u", "%04x"):
            self.assertNotRegex(fmt, FLOAT_CONV)


if __name__ == "__main__":
    unittest.main()
