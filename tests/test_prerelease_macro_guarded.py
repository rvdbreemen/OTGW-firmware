"""TASK-2932: _VERSION_PRERELEASE may only be used where it exists.

A stable release comments out `#define _VERSION_PRERELEASE` in version.h. Any use of the macro outside a
positive `#if defined(_VERSION_PRERELEASE)` / `#ifdef _VERSION_PRERELEASE` block then compiles to something
wrong without an error: OLED.ino:358 stringified the bare token and the device page read
'FW 2.0.0-_VERSION_PRERELEASE' (found by the TASK-2929 production dry run).

This test scans every source file in src/OTGW-firmware line by line, tracks the preprocessor block stack and
fails on a use of the macro that is not: inside a positive prerelease block (the #if branch, not its #else),
in a preprocessor condition (#if / #ifdef / #ifndef / #elif), in a comment, or the #define in version.h.
"""
import re
import unittest
from pathlib import Path

SRC = Path(__file__).resolve().parent.parent / "src" / "OTGW-firmware"
MACRO = "_VERSION_PRERELEASE"
POSITIVE = re.compile(r"^\s*#\s*(?:if\s+defined\s*\(\s*_VERSION_PRERELEASE\s*\)|ifdef\s+_VERSION_PRERELEASE(?!\w))")
COND = re.compile(r"^\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b")


def strip_comments(line, in_block):
    out, i = [], 0
    while i < len(line):
        if in_block:
            j = line.find("*/", i)
            if j < 0:
                return "".join(out), True
            i, in_block = j + 2, False
            continue
        if line.startswith("//", i):
            break
        if line.startswith("/*", i):
            in_block = True
            i += 2
            continue
        out.append(line[i])
        i += 1
    return "".join(out), in_block


def unguarded_uses(text, name):
    stack, hits, in_block = [], [], False   # stack entries: True while inside a positive prerelease #if branch
    for no, raw in enumerate(text.splitlines(), 1):
        code, in_block = strip_comments(raw, in_block)
        m = COND.match(code)
        if m:
            kind = m.group(1)
            if kind in ("if", "ifdef", "ifndef"):
                stack.append(bool(POSITIVE.match(code)))
            elif kind in ("elif", "else"):
                if stack:
                    stack[-1] = False
            elif kind == "endif" and stack:
                stack.pop()
            continue
        if MACRO not in code:
            continue
        if name == "version.h" and re.match(r"^\s*#\s*define\s+_VERSION_PRERELEASE\b", code):
            continue
        if any(stack):
            continue
        hits.append("%s:%d: %s" % (name, no, raw.strip()))
    return hits


class PrereleaseMacroGuarded(unittest.TestCase):
    def test_no_unguarded_use_in_firmware_sources(self):
        hits = []
        for p in sorted(SRC.rglob("*")):
            if p.suffix not in (".ino", ".h", ".cpp", ".c") or "libraries" in p.parts:
                continue
            hits += unguarded_uses(p.read_text(encoding="utf-8", errors="replace"), p.name)
        self.assertEqual(hits, [], "uses of %s that a stable build compiles wrong:\n%s" % (MACRO, "\n".join(hits)))

    def test_the_scanner_catches_the_known_patterns(self):
        bad = '#define S(x) #x\nconst char* v = "FW-" S(_VERSION_PRERELEASE);\n'
        self.assertEqual(len(unguarded_uses(bad, "x.ino")), 1)
        in_else = "#if defined(_VERSION_PRERELEASE)\nint a;\n#else\nconst char* v = S(_VERSION_PRERELEASE);\n#endif\n"
        self.assertEqual(len(unguarded_uses(in_else, "x.ino")), 1)
        ok = "#if defined(_VERSION_PRERELEASE)\nconst char* v = S(_VERSION_PRERELEASE); // fine\n#endif\n// _VERSION_PRERELEASE in a comment\n"
        self.assertEqual(unguarded_uses(ok, "x.ino"), [])


if __name__ == "__main__":
    unittest.main()
