#!/usr/bin/env python3
"""TASK-3033: host test for the otOverride block of OTGW-Core.ino (does the thermostat follow a TT=/TC= override?).

The block between `// --- otOverride begin` and `// --- otOverride end` is cut out of the source and compiled as it
is (otOverrideNoteCommand, otOverrideOnThermostatSetpoint, getOtOverrideVerdict). OTGW_CORE_INO points at another copy
(e.g. `git show origin/dev:src/OTGW-firmware/OTGW-Core.ino > x.ino`) for old-vs-fix. The wiring (call sites, publish
line, topic, reconnect republish, docs) is checked on the source text: that part is a text check, not a run.

Timeline from gruijter's capture 20261024-110022 (Honeywell Round): the override is sent, the thermostat keeps its own
MsgID 16 value every ~40 s. A follow-up thermostat (will_85903's Honeywell) moves to the sent value.

Run: python tests/test_ot_override_refused_3033.py
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

SRC = REPO / "src" / "OTGW-firmware"
CORE_INO = Path(os.environ.get("OTGW_CORE_INO", SRC / "OTGW-Core.ino"))

MAIN = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
static uint32_t g_ms = 0;
static uint32_t millis() { return g_ms; }
enum : uint8_t { OT_SUPPORT_LIST_BOILER = 1, OT_SUPPORT_LIST_THERMOSTAT = 2, OT_SUPPORT_LIST_OVERRIDE = 4 };
static uint8_t otSupportListsDirty = 0;
@BODY@
// stdin lines: "cmd <text> <ms>" | "frame <tr> <ms>"; prints "<verdict> <dirty>" after each line
int main() {
  char kind[8], arg[32]; unsigned long ms;
  while (scanf("%7s %31s %lu", kind, arg, &ms) == 3) {
    g_ms = (uint32_t)ms;
    if (!strcmp(kind, "cmd")) otOverrideNoteCommand(arg, (int)strlen(arg));
    else otOverrideOnThermostatSetpoint((float)atof(arg));
    printf("%d %d\n", (int)getOtOverrideVerdict(), (int)otSupportListsDirty);
    otSupportListsDirty = 0;
  }
  return 0;
}
"""


def extract(src: str) -> str:
    a = src.index("// --- otOverride begin")
    b = src.index("// --- otOverride end")
    return src[a:b]


class Override(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        cpp = d / "t.cpp"
        cpp.write_text(MAIN.replace("@BODY@", extract(CORE_INO.read_text(encoding="utf-8"))))
        cls.exe = compile_cpp(cpp, d / "t.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def run_script(self, events):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        text = "".join("%s %s %d\n" % e for e in events)
        out = subprocess.run([str(self.exe)], input=text, capture_output=True, text=True, check=True).stdout.split("\n")
        return [tuple(int(x) for x in line.split()) for line in out if line.strip()]

    def test_honeywell_ignores_override(self):
        ev = [("frame", "15.0", 0), ("cmd", "TT=19.5", 1000)]
        ev += [("frame", "15.0", 1000 + t * 1000) for t in range(40, 400, 40)]
        res = self.run_script(ev)
        self.assertEqual(res[1], (0, 0))                       # nothing judged yet, nothing to publish
        for t, r in zip(range(40, 400, 40), res[2:]):
            self.assertEqual(r[0], 2 if t >= 300 else 0, "t=%d s" % t)
        flips = [r for r in res if r[1]]
        self.assertEqual(len(flips), 1)                        # one dirty mark, at the flip
        self.assertEqual(flips[0], (2, 4))

    def test_follower_is_followed(self):
        res = self.run_script([("frame", "15.0", 0), ("cmd", "TC=19.5", 1000), ("frame", "19.5", 41000)])
        self.assertEqual(res[-1], (1, 4))

    def test_follow_within_quarter_degree_and_boundary(self):
        res = self.run_script([("frame", "15.0", 0), ("cmd", "TT=19.5", 0), ("frame", "19.25", 100000)])
        self.assertEqual(res[-1][0], 1)
        res = self.run_script([("frame", "15.0", 0), ("cmd", "TT=19.5", 0), ("frame", "19.2", 100000)])
        self.assertEqual(res[-1][0], 0)                        # 0.3 C apart inside 5 min: undecided
        res = self.run_script([("frame", "15.0", 0), ("cmd", "TT=19.5", 0), ("frame", "15.0", 299999)])
        self.assertEqual(res[-1][0], 0)
        res = self.run_script([("frame", "15.0", 0), ("cmd", "TT=19.5", 0), ("frame", "15.0", 300000)])
        self.assertEqual(res[-1][0], 2)

    def test_cancel_keeps_verdict_and_judges_nothing(self):
        ev = [("frame", "15.0", 0), ("cmd", "TT=19.5", 0), ("frame", "15.0", 300000),    # refused
              ("cmd", "TT=0", 310000), ("frame", "15.0", 700000)]
        res = self.run_script(ev)
        self.assertEqual(res[2][0], 2)
        self.assertEqual(res[-1], (2, 0))                      # cancel: stays refused, no new dirty mark
        res = self.run_script([("frame", "15.0", 0), ("cmd", "TC=19.5", 0), ("cmd", "TC=0", 1000), ("frame", "15.0", 400000)])
        self.assertEqual(res[-1][0], 0)                        # cancelled before a verdict: unknown stays unknown

    def test_override_equal_to_own_setpoint_proves_nothing(self):
        res = self.run_script([("frame", "19.5", 0), ("cmd", "TT=19.5", 1000), ("frame", "19.5", 41000)])
        self.assertEqual(res[-1], (0, 0))

    def test_follow_after_refusal_flips_back(self):
        ev = [("frame", "15.0", 0), ("cmd", "TT=19.5", 0), ("frame", "15.0", 300000),
              ("cmd", "TT=18.0", 400000), ("frame", "18.0", 440000)]
        res = self.run_script(ev)
        self.assertEqual(res[2][0], 2)
        self.assertEqual(res[-1], (1, 4))

    def test_garbage_and_other_commands_ignored(self):
        ev = [("frame", "15.0", 0), ("cmd", "TT=abc", 0), ("cmd", "TX=19.5", 0), ("cmd", "TT=", 0), ("cmd", "CS=60", 0),
              ("frame", "15.0", 400000)]
        res = self.run_script(ev)
        self.assertEqual(res[-1], (0, 0))


class Wiring(unittest.TestCase):
    """Text checks on the sources (not a run)."""

    def test_wiring(self):
        core = (SRC / "OTGW-Core.ino").read_text(encoding="utf-8")
        mqtt = (SRC / "MQTTstuff.ino").read_text(encoding="utf-8")
        hdr = (SRC / "OTGW-firmware.h").read_text(encoding="utf-8")
        self.assertIn("otOverrideOnThermostatSetpoint(roundf(OTdata.f88() * 100.0f) / 100.0f);", core)
        self.assertLess(core.index("otOverrideNoteCommand(buf, len);"), core.index("if (!isPICEnabled()) {"))
        self.assertIn("OT_SUPPORT_LIST_OVERRIDE = 4", hdr)
        self.assertIn('F("otgw-firmware/thermostat/override_refused")', mqtt)
        self.assertRegex(mqtt, r"OT_SUPPORT_LIST_BOILER \| OT_SUPPORT_LIST_THERMOSTAT \| OT_SUPPORT_LIST_OVERRIDE")
        docs = (REPO / "docs" / "api" / "MQTT.md").read_text(encoding="utf-8")
        self.assertIn("thermostat/override_refused", docs)


if __name__ == "__main__":
    unittest.main()
