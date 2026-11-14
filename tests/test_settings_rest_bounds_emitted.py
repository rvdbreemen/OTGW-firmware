#!/usr/bin/env python3
"""TASK-1941: the min/max that GET /api/v2/settings EMITS equal the setter clamps (not only the source literals).

    python tests/test_settings_rest_bounds_emitted.py
    OTGW_BOUNDS_REV=<sha> python tests/test_settings_rest_bounds_emitted.py     # another revision (old vs fix)

09's bench on beta.264: satthermalcoeff advertised "min":0 while the setter clamps to 0.005, because
addBound() (restAPI.ino, sendDeviceSettings) writes a non-integer bound with 2 decimals and JsonEmit prints
0.005 at 2 decimals as 0. This test compiles the revision's real addBound lambda against a JsonEmit stand-in
whose float path is the real JsonEmit::value(float, decimals) body sliced from jsonEmit.h, emits every clamp
bound from settingStuff.ino, and compares the printed number with the clamp.
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_published_curve import compile_cpp  # noqa: E402
from test_settings_rest_range_matches_clamp import CLAMP  # noqa: E402

REPO = Path(__file__).resolve().parent.parent
REV = os.environ.get("OTGW_BOUNDS_REV")


def read(rel, rev=None):
    if rev is None:
        return (REPO / rel).read_text(encoding="utf-8").replace("\r\n", "\n")
    return subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{rel}"], capture_output=True, text=True,
                          encoding="utf-8", check=True).stdout.replace("\r\n", "\n")


HARNESS = r"""
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>
#define PSTR(s) (s)
#define F(s) (s)
#define snprintf_P snprintf
typedef const char __FlashStringHelper;
struct Out { void print(const char* s) { fputs(s, stdout); } };
struct JE {
  Out _out;
  void _sep() {}
@@VALUE@@
  void field(const char*, int32_t v) { printf("%ld", (long)v); }
  void field(const char*, float f, uint8_t dec) { value(f, dec); }
} je;
int main(int argc, char** argv) {
@@BOUND@@
  for (int i = 1; i < argc; ++i) { addBound(F("min"), (float)atof(argv[i])); printf("\n"); }
  return 0;
}
"""


class SettingsRestBoundsEmitted(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        emit = read("src/OTGW-firmware/jsonEmit.h")
        a = emit.find("  void value(float f, uint8_t decimals = 6) {")
        b = emit.find("\n  }\n", a)
        rest = read("src/OTGW-firmware/restAPI.ino", REV)
        c = rest.find("    auto addBound = [&](")
        d = rest.find("    };\n", c)
        if min(a, b, c, d) < 0:
            raise AssertionError("JsonEmit::value(float) or addBound not found")
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text(HARNESS.replace("@@VALUE@@", emit[a:b + 4]).replace("@@BOUND@@", rest[c:d + 7]),
                       encoding="utf-8")
        exe = compile_cpp(cpp, Path(cls.tmp.name) / "t.exe")
        if exe is None:
            raise unittest.SkipTest("no host C++ compiler")
        st = read("src/OTGW-firmware/settingStuff.ino")
        cls.bounds = sorted({float(x) for m in CLAMP.findall(st) for x in (m[1], m[2])})
        out = subprocess.run([str(exe)] + [repr(x) for x in cls.bounds], capture_output=True, text=True,
                             check=True).stdout.split()
        cls.emitted = dict(zip(cls.bounds, out))
        print(cls.emitted)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_satthermalcoeff_min_is_emitted_as_0_005(self):
        self.assertEqual(self.emitted[0.005], "0.005")

    def test_every_clamp_bound_is_emitted_exactly(self):
        bad = {b: e for b, e in self.emitted.items() if abs(float(e) - b) > 1e-6}
        self.assertEqual(bad, {}, "clamp bound: emitted text")


if __name__ == "__main__":
    unittest.main(verbosity=2)
