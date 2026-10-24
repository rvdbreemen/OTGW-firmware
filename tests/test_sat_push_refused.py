#!/usr/bin/env python3
"""TASK-1343: host test for satPushRefusedDecide(), cut out of SATcontrol.ino between its markers
and compiled as it is. OTGW_SAT_INO points at another copy (e.g. `git show HEAD:...`) for old-vs-fix.

Timeline from gruijter's capture 20261024-110022 (Honeywell Round): TC=19.5 pushed at 11:01:17,
the thermostat keeps sending MsgID 16 = 15.0 every ~40 s.

Run: python tests/test_sat_push_refused.py
"""
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402

SAT_INO = Path(os.environ.get("OTGW_SAT_INO", REPO / "src" / "OTGW-firmware" / "SATcontrol.ino"))

MAIN = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
@BODY@
int main(int argc, char** argv) {
  // argv: pushOn pushed sinceMs tr pushedTC prev
  bool r = satPushRefusedDecide(atoi(argv[1]), atoi(argv[2]), (uint32_t)strtoul(argv[3], 0, 10),
                                (float)atof(argv[4]), (float)atof(argv[5]), atoi(argv[6]));
  printf("%d\n", r ? 1 : 0);
  return 0;
}
"""


def extract(src: str) -> str:
    a = src.index("// --- satPushRefusedDecide begin")
    b = src.index("// --- satPushRefusedDecide end")
    return src[a:b]


class PushRefused(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        cpp = d / "t.cpp"
        cpp.write_text(MAIN.replace("@BODY@", extract(SAT_INO.read_text(encoding="utf-8"))))
        cls.exe = compile_cpp(cpp, d / "t.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def decide(self, on, pushed, since, tr, tc, prev):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe), str(int(on)), str(int(pushed)), str(since), str(tr), str(tc), str(int(prev))],
                             capture_output=True, text=True, check=True).stdout
        return out.strip() == "1"

    def test_gruijter_timeline(self):
        prev = False
        for t in range(40, 400, 40):          # MsgID 16 = 15.0 every 40 s after the push at t=0
            prev = self.decide(True, True, t * 1000, 15.0, 19.5, prev)
            self.assertEqual(prev, t >= 300, "t=%d s" % t)

    def test_boundary(self):
        self.assertFalse(self.decide(True, True, 299999, 15.0, 19.5, False))
        self.assertTrue(self.decide(True, True, 300000, 15.0, 19.5, False))

    def test_thermostat_follows_clears(self):
        self.assertFalse(self.decide(True, True, 400000, 19.5, 19.5, True))
        self.assertFalse(self.decide(True, True, 400000, 19.7, 19.5, True))   # within 0.25 C: follows
        self.assertTrue(self.decide(True, True, 400000, 19.0, 19.5, False))   # 0.5 C apart: refused

    def test_push_off_clears(self):
        self.assertFalse(self.decide(False, True, 400000, 15.0, 19.5, True))

    def test_not_pushed_clears(self):
        self.assertFalse(self.decide(True, False, 400000, 15.0, 19.5, True))

    def test_inside_window_keeps_verdict(self):
        self.assertTrue(self.decide(True, True, 1000, 15.0, 19.5, True))
        self.assertFalse(self.decide(True, True, 1000, 15.0, 19.5, False))


if __name__ == "__main__":
    unittest.main(verbosity=2)
