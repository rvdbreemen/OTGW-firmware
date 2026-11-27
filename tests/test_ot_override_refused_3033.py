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
import re
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
#include <atomic>
#include <mutex>
#include <thread>
static thread_local uint32_t g_ms = 0;
static uint32_t millis() { return g_ms; }
// portMUX stand-in: a real mutex, so the critical sections of the firmware exclude each other like the spinlock does
struct portMUX_TYPE { std::mutex m; };
#define portMUX_INITIALIZER_UNLOCKED {}
static void portENTER_CRITICAL(portMUX_TYPE* p) { p->m.lock(); }
static void portEXIT_CRITICAL(portMUX_TYPE* p) { p->m.unlock(); }
static std::atomic<long> g_mismatch{0};
static std::atomic<bool> g_race_on{false};
static void OT_RACE_YIELD() { if (g_race_on) std::this_thread::yield(); }
enum : uint8_t { OT_SUPPORT_LIST_BOILER = 1, OT_SUPPORT_LIST_THERMOSTAT = 2, OT_SUPPORT_LIST_OVERRIDE = 4 };
static uint8_t otSupportListsDirty = 0;
static uint8_t otJudgeProbe(float sentC, uint32_t sinceMs, float tr, uint8_t prev);
@BODY@
static uint8_t otJudgeProbe(float sentC, uint32_t sinceMs, float tr, uint8_t prev) {
  // race mode: the writer pairs value 10 with time 1000 and value 20 with time 2000; the reader's clock is 5000
  if (g_race_on && ((sentC == 10.0f && sinceMs != 4000u) || (sentC == 20.0f && sinceMs != 3000u))) g_mismatch++;
  return otOverrideJudge(sentC, sinceMs, tr, prev);
}
static int raceMain() {
  g_race_on = true;
  std::atomic<bool> stop{false};
  std::thread reader([&] { g_ms = 5000; while (!stop) otOverrideOnThermostatSetpoint(15.0f); });
  std::thread writer([&] {
    for (int i = 0; i < 200000; i++) {
      g_ms = 1000; otOverrideNoteCommand("TT=10.0", 7);
      g_ms = 2000; otOverrideNoteCommand("TT=20.0", 7);
    }
    stop = true;
  });
  writer.join(); reader.join();
  printf("mismatch %ld\n", g_mismatch.load());
  return g_mismatch.load() == 0 ? 0 : 3;
}
// stdin lines: "cmd <text> <ms>" | "frame <tr> <ms>"; prints "<verdict> <dirty>" after each line
int main(int argc, char** argv) {
  if (argc > 1 && !strcmp(argv[1], "race")) return raceMain();
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


def instrument(body: str) -> str:
    """Race-mode hooks, test only: the judge goes through a probe that checks the (value, time) pair, and the reader
    yields right after it saw 'pending' (old code: before it reads the fields; fix: after its snapshot, no effect)."""
    body = re.sub(r"const uint8_t v = otOverrideJudge\(", "const uint8_t v = otJudgeProbe(", body)
    body = re.sub(r"(return false;\n)(  const uint8_t v = )", r"\1  OT_RACE_YIELD();\n\2", body)
    body = body.replace("otOverrideSentC   = f;", "otOverrideSentC   = f; OT_RACE_YIELD();")
    return body


class Override(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        cpp = d / "t.cpp"
        cpp.write_text(MAIN.replace("@BODY@", instrument(extract(CORE_INO.read_text(encoding="utf-8")))))
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

    def test_no_pair_of_new_value_and_old_time(self):
        """TASK-3033 review S1: commands noted on another task while the loop task judges a frame must never pair a new
        value with an old time (a false retained override_refused). Stress: red on the code before, green on the fix."""
        if not self.exe:
            self.skipTest("no host C++ compiler")
        r = subprocess.run([str(self.exe), "race"], capture_output=True, text=True, timeout=300)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)

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
        sat = (SRC / "SATcontrol.ino").read_text(encoding="utf-8")
        self.assertRegex(sat, r"void satOnThermostatSetpoint\(float tr\)\s*\{\s*otOverrideOnThermostatSetpoint\(tr\);")
        # F1 (architect review): an override is noted only where addCommandToQueue() accepted it, never before a drop path.
        a = core.index("CmdQueueResult addCommandToQueue(")
        fn = core[a:core.index(chr(10) + "}" + chr(10), a)]
        self.assertEqual(fn.count("otOverrideNoteCommand(buf, len);"), 2, "OT-Direct handler on the loop task, PIC queue")
        self.assertLess(fn.rindex("return CmdQueueResult::Dropped;"), fn.rindex("otOverrideNoteCommand(buf, len);"))
        # S1b (second review): a command in the text ring is noted when the loop task ran it and the handler accepted it,
        # not when it was pushed
        push = fn[fn.index("if (otdCmdTextPush("):]
        self.assertNotIn("otOverrideNoteCommand", push[:push.index("Queued")])
        drain = (SRC / "OTDirect.ino").read_text(encoding="utf-8")
        d = drain[drain.index("void otdCmdTextDrain()"):]
        d = d[:d.index(chr(10) + "}" + chr(10))]
        self.assertRegex(d, r"if \(handleOTDirectCommand\(c\.text, c\.len, c\.satOwn\)\) otOverrideNoteCommand\(c\.text, c\.len\);")
        # S1 (second review): value, time, flag and sequence are written and read under one spinlock
        blk = extract(core)
        self.assertIn("static portMUX_TYPE otOverrideMux", blk)
        self.assertNotIn("std::atomic", blk)
        self.assertEqual(blk.count("portENTER_CRITICAL(&otOverrideMux)"), blk.count("portEXIT_CRITICAL(&otOverrideMux)"))
        self.assertGreaterEqual(blk.count("portENTER_CRITICAL(&otOverrideMux)"), 3)
        self.assertIn("OT_SUPPORT_LIST_OVERRIDE = 4", hdr)
        self.assertIn('F("otgw-firmware/thermostat/override_refused")', mqtt)
        self.assertRegex(mqtt, r"OT_SUPPORT_LIST_BOILER \| OT_SUPPORT_LIST_THERMOSTAT \| OT_SUPPORT_LIST_OVERRIDE")
        docs = (REPO / "docs" / "api" / "MQTT.md").read_text(encoding="utf-8")
        self.assertIn("thermostat/override_refused", docs)


if __name__ == "__main__":
    unittest.main()
