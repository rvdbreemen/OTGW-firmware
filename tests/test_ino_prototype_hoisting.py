#!/usr/bin/env python3
"""A function that takes a type declared in an .ino needs its own prototype after that type.

The Arduino builder merges the .ino files (OTGW-firmware.ino first, then the others in
alphabetical order) into one OTGW-firmware.ino.cpp and inserts an auto-generated prototype of
every function just before the first function definition of the sketch. A function whose
parameter list names a struct, class or enum that an .ino declares after that point gets a
prototype above its type, and the build stops with "'<Type>' does not name a type". Host tests
compile slices without those prototypes, so only the real build saw it (TASK-1885: first build
of satEmitStatusLive(JsonEmit&, const SatStatusLive&)).

The builder leaves a function alone when the sketch already declares it, which is how the
firmware does it today (OTDirect.ino: struct FlameRatioBuf, then
`static void flameRatioBufCommit(FlameRatioBuf &b);`). This gate asks for exactly that: such a
function has an explicit declaration somewhere before its definition, or takes plain types.

Old-vs-fix: OTGW_FW_DIR=<a directory with the .ino files of another revision>.
Run: python tests/test_ino_prototype_hoisting.py
"""
import os
import re
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
FW = Path(os.environ.get("OTGW_FW_DIR") or HERE.parent / "src" / "OTGW-firmware")
MAIN = "OTGW-firmware.ino"

TYPE_RX = re.compile(r"^(?:struct|class|enum(?:\s+class)?)\s+(\w+)\b[^;{]*\{", re.M)
# A top-level function definition: return type, name, parameter list, then the body brace.
FUNC_RX = re.compile(r"^(?!\s)(?!(?:if|for|while|switch|return|else)\b)[\w:<>,\*&\s]*?\b(\w+)\s*\(([^;{}()]*)\)\s*(?:const\s*)?\{", re.M)
KEYWORDS = {"if", "for", "while", "switch", "return", "sizeof", "else"}


def strip_comments(src):
    """Blank comments and string/char literals, keeping newlines. String-aware: a "/*" inside a
    literal (a route or MIME pattern) must not open a comment."""
    out, i, n = [], 0, len(src)
    while i < n:
        c = src[i]
        if c == "/" and src.startswith("//", i):
            j = src.find("\n", i)
            i = n if j < 0 else j
        elif c == "/" and src.startswith("/*", i):
            j = src.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append(re.sub(r"[^\n]", " ", src[i:j]))
            i = j
        elif c in "\"'":
            j = i + 1
            while j < n and src[j] != c and src[j] != "\n":
                j += 2 if src[j] == "\\" else 1
            out.append(c + " " * max(0, min(j, n) - i - 1) + c)
            i = j + 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


def merged_sketch(fw):
    names = [MAIN] + sorted(p.name for p in fw.glob("*.ino") if p.name != MAIN)
    return "\n".join(strip_comments((fw / n).read_text(encoding="utf-8")) for n in names)


def offending_functions(fw):
    text = merged_sketch(fw)
    first_func = FUNC_RX.search(text)
    hoist_at = first_func.start() if first_func else 0
    late_types = {m.group(1) for m in TYPE_RX.finditer(text) if m.start() > hoist_at}
    headers = "\n".join(strip_comments(p.read_text(encoding="utf-8")) for p in fw.glob("*.h"))
    bad = []
    for m in FUNC_RX.finditer(text):
        name, params = m.group(1), m.group(2)
        if name in KEYWORDS:
            continue
        used = [t for t in late_types if re.search(r"\b" + t + r"\b", params)]
        if not used:
            continue
        decl = re.compile(r"\b" + name + r"\s*\([^;{}()]*\)\s*;")
        if decl.search(text, 0, m.start()) or decl.search(headers):
            continue
        line = text.count("\n", 0, m.start()) + 1
        bad.append(f"{name}({params.strip()}) uses {', '.join(sorted(used))} (merged line {line})")
    return bad


class PrototypeHoisting(unittest.TestCase):
    def test_no_function_takes_a_late_ino_type_without_its_own_prototype(self):
        bad = offending_functions(FW)
        self.assertEqual(bad, [], "these functions need a prototype after their parameter type:\n  " + "\n  ".join(bad))

    def test_the_gate_sees_the_existing_pattern(self):
        # FlameRatioBuf is declared in OTDirect.ino and passed by reference; the explicit
        # prototype after it is what keeps the build working. Without it the gate must fire.
        text = merged_sketch(FW)
        self.assertIn("flameRatioBufCommit", text)
        self.assertRegex(text, r"static void flameRatioBufCommit\(FlameRatioBuf &b\);")


if __name__ == "__main__":
    unittest.main(verbosity=2)
