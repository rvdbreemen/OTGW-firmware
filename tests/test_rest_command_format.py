"""TASK-1709: REST /api/v2/otgw/command(s) accepts C2= and H2=.

The REST handler (restAPI.ino handleCommandSubmit) validates the command shape. Before TASK-1709 it demanded two letters, so the real
PIC commands C2= (CH2 setpoint) and H2= (CH2 enable) got HTTP 400. This test compiles the predicate the handler calls,
otgwCommandFormatOk() from OTGW-firmware.h, and checks that handleCommandSubmit really calls it. On the old code the predicate does not
exist and the handler still carries the two-letter check, so the test fails there.
"""
import re
import sys
import tempfile
import unittest
import subprocess
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402

HDR = REPO / "src" / "OTGW-firmware" / "OTGW-firmware.h"
REST = REPO / "src" / "OTGW-firmware" / "restAPI.ino"

ACCEPT = ("C2=40", "H2=1", "c2=40", "CS=40.0", "CH=0", "HW=1", "PR=A", "GW=1", "TT=19.5")
REJECT = ("2C=40", "1A=0", "C=40", "CS", "CS40", "=AA", "C-=1", "\x01S=1", "\xc3\xa9S=1", "", "C2+40")


def extract_predicate(text: str) -> str:
    m = re.search(r"inline bool otgwCommandFormatOk\(const char\* s, size_t len\) \{", text)
    if not m:
        raise AssertionError("otgwCommandFormatOk() not found in OTGW-firmware.h")
    i, depth = m.end(), 1
    while depth:
        depth += {"{": 1, "}": -1}.get(text[i], 0)
        i += 1
    return text[m.start():i]


def c_literal(s: str) -> str:
    return '"' + "".join("\\x%02x" % b for b in s.encode("latin-1")) + '"'


class RestCommandFormat(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pred = extract_predicate(HDR.read_text(encoding="utf-8"))
        cases = [(c, 1) for c in ACCEPT] + [(c, 0) for c in REJECT]
        rows = ",\n".join("  {%s, %d, %d}" % (c_literal(c), len(c.encode("latin-1")), want) for c, want in cases)
        src = ("#include <cstdio>\n#include <cstddef>\n" + pred +
               "\nstruct C { const char* s; size_t n; int want; };\nstatic const C cases[] = {\n" + rows +
               "\n};\nint main(){ int bad=0; for (const C& c : cases) { int got = otgwCommandFormatOk(c.s, c.n) ? 1 : 0;"
               " std::printf(\"%d %d\\n\", got, c.want); if (got != c.want) bad++; } return bad; }\n")
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        (d / "t.cpp").write_text(src, encoding="utf-8")
        cls.exe = compile_cpp(d / "t.cpp", d / "t.exe")
        cls.cases = cases

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def test_predicate_cases(self):
        r = subprocess.run([str(self.exe)], capture_output=True, text=True, timeout=30)
        got = [line.split() for line in r.stdout.splitlines()]
        wrong = [(c, g) for (c, _), g in zip(self.cases, got) if g[0] != g[1]]
        self.assertEqual(wrong, [], "cases with the wrong verdict (command, [got, want])")
        self.assertEqual(r.returncode, 0)

    def test_rest_handler_uses_predicate(self):
        body = REST.read_text(encoding="utf-8")
        fn = body[body.index("static void handleCommandSubmit(const char* cmdStr) {"):]
        fn = fn[:fn.index("\n}\n")]
        self.assertIn("otgwCommandFormatOk(cmdStr, cmdLen)", fn)
        self.assertNotIn("isalpha(static_cast<unsigned char>(cmdStr[1]))", fn)


if __name__ == "__main__":
    unittest.main()
