#!/usr/bin/env python3
"""TASK-1352: host test for the 6 h expiry of the reported SAT last cycle class.

thermo-nova (cycles/history.py:48-57, cycles/const.py:14) returns no last cycle once it ended more
than 6 h ago. The real helper satLastCycleExpired() is cut out of SATtypes.h between its
`// --- satLastCycleExpired begin/end` markers and compiled with a host C++ compiler; nothing is
re-implemented. OTGW_SATTYPES_H points at another copy (e.g. `git show HEAD:...`) for old-vs-fix runs.
Without a compiler the compiled tests are skipped.

Run: python tests/test_sat_last_cycle_expiry.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402

FW = REPO / "src" / "OTGW-firmware"
TYPES_H = Path(os.environ.get("OTGW_SATTYPES_H", FW / "SATtypes.h"))

STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
@BODY@
int main(int argc, char** argv) {
  uint32_t now = (uint32_t)strtoul(argv[1], 0, 10);
  uint32_t end = (uint32_t)strtoul(argv[2], 0, 10);
  bool have = atoi(argv[3]) != 0;
  printf("%d\n", satLastCycleExpired(now, end, have) ? 1 : 0);
  return 0;
}
"""

SIX_H = 6 * 3600 * 1000


def extract(src: str) -> str:
    m = re.search(r"// --- satLastCycleExpired begin\r?\n(.*?)// --- satLastCycleExpired end", src, re.S)
    if not m:
        raise AssertionError("satLastCycleExpired helper missing from %s (TASK-1352 not applied)" % TYPES_H)
    return m.group(1)


class LastCycleExpiry(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        body = extract(TYPES_H.read_text(encoding="utf-8"))
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        cpp = d / "t.cpp"
        cpp.write_text(STUBS.replace("@BODY@", body))
        cls.exe = compile_cpp(cpp, d / "t.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def expired(self, now, end, have=True):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe), str(now), str(end), "1" if have else "0"],
                             capture_output=True, text=True, check=True).stdout.strip()
        return out == "1"

    def test_fresh_cycle_not_expired(self):
        self.assertFalse(self.expired(1000, 1000))

    def test_just_under_six_hours_not_expired(self):
        self.assertFalse(self.expired(100000 + SIX_H - 1, 100000))

    def test_exactly_six_hours_not_expired(self):
        # thermo-nova: `seconds_since(end) > LAST_CYCLE_MAX_AGE_SECONDS`, so exactly 6 h is still kept
        self.assertFalse(self.expired(100000 + SIX_H, 100000))

    def test_after_six_hours_expired(self):
        self.assertTrue(self.expired(100000 + SIX_H + 1, 100000))

    def test_wrap_safe_not_expired(self):
        end = 4294967295 - 1000
        now = (end + 3600 * 1000) & 0xFFFFFFFF          # 1 h later, millis() wrapped
        self.assertLess(now, end)
        self.assertFalse(self.expired(now, end))

    def test_wrap_safe_expired(self):
        end = 4294967295 - 1000
        now = (end + SIX_H + 5000) & 0xFFFFFFFF          # just over 6 h later, wrapped
        self.assertLess(now, end)
        self.assertTrue(self.expired(now, end))

    def test_no_cycle_is_expired(self):
        self.assertTrue(self.expired(5, 5, have=False))


class PublishersUseIt(unittest.TestCase):
    """The reporting paths must go through satReportedLastCycleClass(); control paths stay raw."""

    def src(self, name):
        return (FW / name).read_text(encoding="utf-8")

    def test_reporting_files_have_no_raw_read(self):
        for name in ("restAPI.ino", "handleDebug.ino", "networkStuff.ino"):
            self.assertNotIn("eLastCycleClass", self.src(name).replace("satReportedLastCycleClass", ""),
                             name + " reads the raw last cycle class")
        self.assertIn("satReportedLastCycleClass", self.src("restAPI.ino"))

    def test_sat_control_has_no_raw_last_cycle_class_read(self):
        raw = [l for l in self.src("SATcontrol.ino").splitlines() if "state.sat.eLastCycleClass" in l]
        self.assertEqual(len(raw), 0, raw)   # the only raw reader was the removed auto-tune (TASK-1413)
        self.assertIn("satReportedLastCycleClass", self.src("SATcontrol.ino"))

    def test_end_time_recorded_with_class(self):
        s = self.src("SATcycles.ino")
        self.assertRegex(s, r"state\.sat\.eLastCycleClass = cls;\s*state\.sat\.iLastCycleEndMs = millis\(\);")


if __name__ == "__main__":
    unittest.main(verbosity=2)
