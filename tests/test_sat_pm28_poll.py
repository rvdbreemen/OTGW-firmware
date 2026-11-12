#!/usr/bin/env python3
"""TASK-1759 part C (Sergeant D, corrections entry 296): SAT on a PIC board asks for MsgID 28 itself (PM=28) when no valid
return temperature came for 30 s.

Same host program as tests/test_sat_pm25_poll.py (built by its build_source(): the PIC command queue of OTGW-Core.ino, the
processOT() pieces with the flow/return feed hook, SATpm25Poll.h as it is), plus satPm28PollStep() and its statics cut from
SATcontrol.ino, run in the same pass after satPm25PollStep() as the control loop does. The bus model serves a pending PM= in
the next request for a DataID the boiler rejects; the boiler answers MsgID 28 with Read-Ack 45.0.

Not reached here: the call site in satControlLoopRun() (checked at source level), the real PIC and a real OpenTherm bus
(AC3 needs a PIC capture with a boiler and a thermostat: field check).

Old-vs-fix: OTGW_SAT_INO / OTGW_PM25_H point at other copies; without satPm28PollStep() the harness sends no PM=28 and the
cases go red, not the build.
Run: python tests/test_sat_pm28_poll.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402
import test_sat_pm25_poll as p25  # noqa: E402


def build(core, sat, pm25_h):
    src = p25.build_source(core, sat, pm25_h)
    sat = sat.replace("\r\n", "\n")
    m = re.search(r"^static uint32_t _sat_pm28SentMs.*?^static void satPm28PollStep\(\)\n\{.*?^\}\n", sat, re.S | re.M)
    if m and "satPm28Decide" in pm25_h:
        step28 = m.group(0)
    else:
        step28 = "static void satPm28PollStep() {}\n"   # the code before TASK-1759 part C: SAT never asks for 28
    def sub(old, new):
        nonlocal src
        assert old in src, old
        src = src.replace(old, new, 1)
    sub("uint32_t iPm25Sent = 0, iPm25Answered = 0, iPm25Unanswered = 0;",
        "uint32_t iPm25Sent = 0, iPm25Answered = 0, iPm25Unanswered = 0, iPm28Sent = 0, iPm28Answered = 0, iPm28Unanswered = 0;")
    sub("static bool g_satOn = true, g_unsup25 = false;", "static bool g_satOn = true, g_unsup25 = false, g_unsup28 = false;")
    sub("return id == 25 && g_unsup25; }", "return (id == 25 && g_unsup25) || (id == 28 && g_unsup28); }")
    sub("// ---- bus model", step28 + "\n// ---- bus model")
    sub("static int  g_pm25 = 0;", "static int  g_pm25 = 0, g_pm28 = 0, g_maxOut = 0; static char g_sent28[8192];")
    sub("if (g_picPrio == 25) {",
        "if (g_picPrio == 28) { char ts[16]; snprintf(ts, sizeof(ts), \"%lu,\", (unsigned long)t); strcat(g_sent28, ts); g_pm28++; }\n"
        "      if (g_picPrio == 25) {")
    sub("    satPm25PollStep();\n#endif\n",
        "    satPm25PollStep();\n#endif\n    satPm28PollStep();\n"
        "    { int o = 0; for (int i = 0; i < cmdQueueSize; i++) if (cmdqueue[i].cmd[0] == 'P' && cmdqueue[i].cmd[1] == 'M') o++;\n"
        "      if (s_prioSlotId != PRIO_SLOT_FREE || g_picPrio) o++; if (o > g_maxOut) g_maxOut = o; }\n")
    sub("static void report() {\n",
        "static void report() {\n"
        "  printf(\"pm28=%d\\nsent28_ms=%s\\nmax_out=%d\\nn28_sent=%u\\nn28_answered=%u\\nn28_unanswered=%u\\n\", g_pm28, g_sent28, g_maxOut,\n"
        "         (unsigned)state.sat.iPm28Sent, (unsigned)state.sat.iPm28Answered, (unsigned)state.sat.iPm28Unanswered);\n")
    # extra scenarios: the thermostat reads 28 itself; the boiler marks 28 unsupported; the thermostat reads 25 (PM=28 alone)
    sub("  if (id == 25) { f('T', RD(25));",
        "  if (id == 28) { f('T', RD(28)); f('B', 0x40000000u | RD(28) | 0x2800); return; }\n  if (id == 25) { f('T', RD(25));")
    sub("  else if (!strcmp(sc, \"quiet_aa\"))",
        "  else if (!strcmp(sc, \"t28\")) { static const int ids[] = { 0, 18, 28, 3 }; static const bool rj[] = { false, true, false, false };\n"
        "                                 g_ids = ids; g_nIds = 4; g_rej = rj; }\n"
        "  else if (!strcmp(sc, \"unsup28\")) { g_unsup28 = true; }\n"
        "  else if (!strcmp(sc, \"only28\")) { g_t25Period = 0; static const int ids[] = { 0, 18, 25, 3 }; static const bool rj[] = { false, true, false, false };\n"
        "                                    g_ids = ids; g_nIds = 4; g_rej = rj; }\n"
        "  else if (!strcmp(sc, \"quiet_aa\"))")
    return src


class Pm28Poll(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        pm25 = p25.PM25_H.read_text(encoding="utf-8")
        cpp = d / "t.cpp"
        cpp.write_text(build(p25.CORE_INO.read_text(encoding="utf-8"), p25.SAT_INO.read_text(encoding="utf-8"), pm25), encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / "t.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def sc(self, name, until="420000"):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        r = subprocess.run([str(self.exe), name, until], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        return dict(line.split("=", 1) for line in r.stdout.splitlines())

    @staticmethod
    def times(s):
        return [int(x) for x in s.split(",") if x]

    def test_1_no_return_temperature_pm28_every_30s(self):
        o = self.sc("only28")          # the thermostat reads 25 itself, never 28; a rejected DataID gives the slot
        t = self.times(o["sent28_ms"])
        self.assertGreaterEqual(len(t), 10, o)
        gaps = [b - a for a, b in zip(t, t[1:])]
        self.assertTrue(all(g >= 30000 for g in gaps), gaps)
        self.assertTrue(all(g <= 36000 for g in gaps), gaps)     # answered: about every 30 s
        self.assertEqual(o["n28_unanswered"], "0", o)
        self.assertEqual(o["pm25"], "0", o)

    def test_2_both_due_never_two_outstanding(self):
        o = self.sc("quiet")           # neither 25 nor 28 is read by the thermostat
        self.assertGreater(int(o["pm28"]), 0, o)
        self.assertGreater(int(o["pm25"]), 0, o)
        self.assertEqual(o["max_out"], "1", "PM=25 and PM=28 share the PIC's one priority slot")

    def test_3_thermostat_reads_28_no_pm28(self):
        self.assertEqual(self.sc("t28")["pm28"], "0")

    def test_4_no_slot_no_pm28(self):
        self.assertEqual(self.sc("noslot")["pm28"], "0")

    def test_5_unknown_dataid_28_idle(self):
        self.assertEqual(self.sc("unsup28")["pm28"], "0")

    def test_6_gates(self):
        for g in ("ps", "monitor", "satoff"):
            self.assertEqual(self.sc(g)["pm28"], "0", g)

    def test_7_control_loop_calls_the_step_after_pm25(self):
        sat = p25.SAT_INO.read_text(encoding="utf-8")
        a = sat.find("  satPm25PollStep();")
        b = sat.find("  satPm28PollStep();", a)
        self.assertTrue(0 < a < b < a + 300, "satPm28PollStep() runs right after satPm25PollStep() in the control loop")


if __name__ == "__main__":
    unittest.main(verbosity=2)
