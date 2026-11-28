#!/usr/bin/env python3
"""TASK-2993 (Sergeant D entry 437, start-up filter 30 s per entry 438): the PWM duty uses the live flow.
TASK-2992 (entries 436/440): MM=T only above a raw duty of 1.03 (class MmReleaseAbove103).

duty = (request - base) / (flow - base), base = satGetBaseOffset() (27.2 radiators, 20.0 underfloor), floor base + 1 against a
division by zero (entry 1). Only inside the first 30 s after the ignition of an ON phase the EMA 0.3/0.7 of the flow is used.
Before: thermo-nova's BT, an EMA for 180 s after the ignition and then held until the next ON phase (pwm.py:144-149, 229-239),
which pinned the duty at the maximum while the flow rose (indigo_light 19 Nov 21:27-21:33: logged duty 0.97-1.00).

The code under test is the REAL PWM block of SATcontrol.ino, cut and compiled by tests/test_sat_hp_pwm.py (shared harness),
driven with this file's MAIN: one control tick per line of the indigo_light 19 Nov fixture (21:22:30-21:33:00, flame, flow and
requested setpoint as recorded; tests/fixtures/sat_replay_2987/), through satApplyPWM(), printing state.sat.fPwmDutyCycle and
satPwmAtMaxDuty() (range 5: MM=T, entries 242/243). Checks:
  1. the OLD code (OTGW_SAT_INO, default origin/dev) reproduces the logged duty of every tick within 0.01, so the harness drives
     the same code path as the device did (red/green proof that the test reaches the real duty);
  2. the fix gives (request - 27.2)/(flow - 27.2) after the window: about 1.00 at 21:28:30-21:29:00, then 0.74, 0.57, 0.52 ...
     about 0.47 while the flow rises to 41.9; no range 5 (no MM=T) after 21:28:30;
  3. a synthetic ON phase: the EMA applies only inside the 30 s window (window edge).
Run: python tests/test_sat_pwm_live_flow_duty_2993.py      Old side: OTGW_DUTY_BASE (default 939f9406b, the code before TASK-2993/2992)
"""
import csv
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import test_sat_hp_pwm as hp  # noqa: E402

FIX = HERE / "fixtures" / "sat_replay_2987" / "indigo_light_20261119_beta392.csv"
BASE_REV = os.environ.get("OTGW_DUTY_BASE", "939f9406b")   # origin/dev before TASK-2993/2992: pinned, so the old side stays old after the push

MAIN = r"""
#include <iostream>
#include <string>
int main() {
  settings = Settings(); state = State(); OTcurrentSystemState = OTSys();
  g_flame = false; g_flow = 50; g_ret = 30; g_due = true; g_hourLimit = false; g_hasOT = true; g_cold = 28.2f;
  satPwmReset();
  _sat_lastControlCS = 0; _sat_thermalSafeActive = false;
  settings.sat.iCyclesPerHour = 4;                    // indigo: dutyMax 0.90 at 4 cycles/h
  state.sat.eControlMode = SAT_MODE_PWM; state.sat.bDhwActive = false; state.sat.bPwmFlameRequested = false;
  const uint32_t base = 100000000UL;
  _pwm_entryPending = true; _pwm_lastUpdateMs = base; // PWM just entered: the first step starts ON (entry 43)
  g_flameOnStart = base - 4000000UL; g_flameOffStart = base - 5000000UL;
  double sec; int flame; float flow, req;
  while (std::cin >> sec >> flame >> flow >> req) {
    g_ms = base + (uint32_t)(sec * 1000.0 + 0.5);   // fractional seconds model the tick jitter
    if ((flame != 0) != g_flame) { g_flame = flame != 0; if (g_flame) g_flameOnStart = g_ms; else g_flameOffStart = g_ms; }
    g_flow = flow;
    const float cs = satApplyPWM(req);
    _sat_lastControlCS = cs;
    printf("D %ld %.4f %d %d %lu\n", (long)sec, state.sat.fPwmDutyCycle, (int)satPwmAtMaxDuty(), (int)state.sat.bPwmFlameRequested,
           (unsigned long)satPwmLastOffTimeMs());
  }
  return 0;
}
"""


def rows(a="21:22:30", b="21:33:00"):
    out = []
    with open(FIX, encoding="utf-8") as f:
        for r in csv.DictReader(l for l in f if not l.startswith("#")):
            if a <= r["time"] <= b:
                h, m, s = (int(x) for x in r["time"].split(":"))
                out.append((h * 3600 + m * 60 + s, r))
    t0 = out[0][0]
    return [(t - t0, r) for t, r in out]


def build(src_text, d, name):
    path = d / f"{name}_SATcontrol.ino"
    path.write_text(src_text, encoding="utf-8")
    old = hp.SAT_INO
    try:
        hp.SAT_INO = path
        hp.MAIN = MAIN
        sub = d / name
        sub.mkdir()
        return hp.build(sub)
    finally:
        hp.SAT_INO = old


def run(exe, ticks):
    stdin = "".join("%s %d %s %s\n" % t for t in ticks)
    out = subprocess.run([str(exe)], input=stdin, capture_output=True, text=True, check=True).stdout
    return {int(p[1]): (float(p[2]), int(p[3]), int(p[4]), int(p[5])) for p in (l.split() for l in out.splitlines() if l.startswith("D "))}


class LiveFlowDuty(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        d = Path(cls.tmp.name)
        fix = hp.SAT_INO.read_text(encoding="utf-8")
        old = subprocess.run(["git", "show", f"{BASE_REV}:src/OTGW-firmware/SATcontrol.ino"], cwd=HERE.parent,
                             capture_output=True, check=True).stdout.decode("utf-8")
        cls.exe_fix = build(fix, d, "fix")
        try:
            cls.exe_old = build(old, d, "old")
        except AssertionError:
            cls.exe_old = None   # the pre-TASK-2993 source needs cold-cut stubs the current shared harness no longer has: old-side cases skip
        if not cls.exe_fix:
            raise unittest.SkipTest("no host C++ compiler")
        cls.rows = rows()
        ticks = [(t, int(r["flame"]), r["flow"], r["pid"]) for t, r in cls.rows]
        cls.fix = run(cls.exe_fix, ticks)
        cls.old = run(cls.exe_old, ticks) if cls.exe_old else None
        print()
        for t, r in cls.rows:
            if cls.old:
                print("%s logged=%s old=%.2f fix=%.2f mmT(old/fix)=%d/%d flow=%s pid=%s" %
                      (r["time"], r["duty"], cls.old[t][0], cls.fix[t][0], cls.old[t][1], cls.fix[t][1], r["flow"], r["pid"]))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_old_code_reproduces_the_logged_duty(self):
        if not self.old:
            self.skipTest("the old side does not build with the current harness")
        # from the first lit tick on (before it the harness and the device started the phase differently)
        lit = [t for t, r in self.rows if r["flame"] == "1"]
        for t, r in self.rows:
            if t < lit[0]:
                continue
            with self.subTest(r["time"]):
                self.assertAlmostEqual(self.old[t][0], float(r["duty"]), delta=0.011)

    def expected(self):
        # Entry 444: a tick in the ON phase (the phase the previous tick left, column 3) uses the live flow, a tick in the OFF phase
        # the flow of the last ON tick, both with this tick's request. In this replay the harness ends the ON phase at its on time
        # while the device stayed in range 5, so 21:31:30-21:33:00 are OFF ticks here.
        out, prev_on, held = {}, 0, None
        for t, r in self.rows:
            flow, req = float(r["flow"]), float(r["pid"])
            if prev_on:
                held = flow
            use = flow if (prev_on or held is None) else held
            out[t] = min(1.0, max(0.0, (req - 27.2) / (max(use, 28.2) - 27.2)))
            prev_on = self.fix[t][2]
        return out

    def test_fix_follows_the_live_flow_after_the_window(self):
        want = self.expected()
        for t, r in self.rows:
            if r["time"] < "21:24:30" or r["mode"] != "2":
                continue
            with self.subTest(r["time"]):
                self.assertAlmostEqual(self.fix[t][0], want[t], delta=0.005)

    def test_no_range_5_after_21_28_30(self):
        late = [(r["time"], self.fix[t]) for t, r in self.rows if r["time"] >= "21:29:30" and r["mode"] == "2"]
        self.assertTrue(late)
        self.assertEqual([x for x in late if x[1][1]], [], "range 5 (MM=T) with the live flow: %r" % (late,))
        t = [t for t, r in self.rows if r["time"] == "21:32:00"][0]
        self.assertEqual(self.fix[t][2], 0, "21:32:00 is an OFF tick in this replay")
        self.assertAlmostEqual(self.fix[t][0], (33.85 - 27.2) / (40.7 - 27.2), delta=0.005)   # 0.49: last ON flow 40.7 (entry 444)

    def test_old_code_was_at_range_5(self):
        if not self.old:
            self.skipTest("the old side does not build with the current harness")
        self.assertTrue(any(self.old[t][1] for t, r in self.rows if r["time"] >= "21:29:30"), "red run proves nothing")


class FilterWindow(unittest.TestCase):
    """The EMA counts only inside 30 s after the ignition. A tick's duty uses the value filtered up to the previous tick (the EMA
    update runs after the duty in satPwmSchedule(), as thermo-nova pwm.py:148-149 before :229). Ignition at 30 s with flow 40
    (seed 40); 40 s: duty from 40, EMA -> 0.3*30 + 0.7*40 = 37; 50 s (20 s after the ignition): duty from 37; 70 s (40 s after):
    the live flow 30. Request 34."""
    setUpClass = classmethod(LiveFlowDuty.setUpClass.__func__)
    tearDownClass = classmethod(LiveFlowDuty.tearDownClass.__func__)

    def test_window_edge(self):
        ticks = [(0, 0, "45.0", "34.0"), (30, 1, "40.0", "34.0"), (40, 1, "30.0", "34.0"), (50, 1, "30.0", "34.0"),
                 (70, 1, "36.0", "34.0")]
        res = run(self.exe_fix, ticks)
        self.assertAlmostEqual(res[40][0], (34 - 27.2) / (40 - 27.2), delta=0.005, msg=res)    # seed, inside the window
        self.assertAlmostEqual(res[50][0], (34 - 27.2) / (37 - 27.2), delta=0.005, msg=res)    # EMA, 20 s after the ignition
        self.assertAlmostEqual(res[70][0], (34 - 27.2) / (36 - 27.2), delta=0.005, msg=res)    # 40 s after: live flow 36
        if not self.exe_old:
            return
        old = run(self.exe_old, ticks)
        self.assertNotAlmostEqual(old[70][0], (34 - 27.2) / (36 - 27.2), delta=0.005, msg=old)  # old: still the EMA/held BT

    def test_window_includes_the_tick_30_s_after_the_ignition(self):
        # The default control tick is 30 s (SATtypes.h), so the tick after the ignition tick is exactly 30 s later. Thermo-nova's
        # window is inclusive (pwm.py:148 "<="); with "<" the filter would never reach the duty at the default cadence (review
        # finding, 2026-11-21). Light-off dip of entry 437 (flow 38.7 then 32.3, request 34): the +30 s tick uses the seed 38.7.
        ticks = [(0, 0, "45.0", "34.0"), (30, 1, "38.7", "34.0"), (60, 1, "32.3", "34.0"), (90, 1, "32.3", "34.0")]
        res = run(self.exe_fix, ticks)
        self.assertAlmostEqual(res[60][0], (34 - 27.2) / (38.7 - 27.2), delta=0.005, msg=res)   # 0.59, inside (30 s after)
        self.assertEqual((res[60][1], res[90][0], res[90][1]), (0, 1.0, 1), res)                 # +60 s: live 32.3, raw 1.33, MM=T


class WindowToleratesTickJitter(unittest.TestCase):
    """Bench .61 (control interval 10 s, 21 Nov): the tick 30 s after the ignition ran 30003 / 30023 ms later and used the live
    flow, so with an exact 30000 ms bound the start-up filter depends on ms jitter (at the default 30 s interval it covers that
    single tick only part of the time). The window has 1 s slack (SAT_PWM_BT_FILTER_SLACK_MS). Ignition at 10.004 s with flow 40
    (seed); 20.007: duty from 40, EMA 37; 30.010: duty from 37, EMA 34.9; 40.030 (+30.026 s): duty from 34.9, not the live 36."""
    setUpClass = classmethod(LiveFlowDuty.setUpClass.__func__)
    tearDownClass = classmethod(LiveFlowDuty.tearDownClass.__func__)

    TICKS = [("0", 0, "45.0", "34.0"), ("10.004", 1, "40.0", "34.0"), ("20.007", 1, "30.0", "34.0"), ("30.010", 1, "30.0", "34.0"),
             ("40.030", 1, "36.0", "34.0"), ("51.200", 1, "36.0", "34.0")]

    def test_tick_30_s_late_by_ms_is_inside_the_window(self):
        res = run(self.exe_fix, self.TICKS)
        self.assertAlmostEqual(res[40][0], (34 - 27.2) / (34.9 - 27.2), delta=0.005, msg=res)   # 0.88, EMA

    def test_slack_is_bounded(self):
        res = run(self.exe_fix, self.TICKS)
        self.assertAlmostEqual(res[51][0], (34 - 27.2) / (36 - 27.2), delta=0.005, msg=res)     # +41.2 s: live 36

    def test_default_30_s_interval_one_filtered_tick_only(self):
        # Default interval 30 s: ignition at 30.004; +30.02 s (60.024) is the one filtered tick (seed 40, not the live 30, which
        # would give raw 2.4 -> 1.00); +60.03 s (90.030) uses the live 36, so the slack cannot reach a second tick (BlueFlame).
        ticks = [("0", 0, "45.0", "34.0"), ("30.004", 1, "40.0", "34.0"), ("60.024", 1, "30.0", "34.0"),
                 ("90.030", 1, "36.0", "34.0")]
        res = run(self.exe_fix, ticks)
        self.assertAlmostEqual(res[60][0], (34 - 27.2) / (40 - 27.2), delta=0.005, msg=res)     # 0.53, filtered
        self.assertAlmostEqual(res[90][0], (34 - 27.2) / (36 - 27.2), delta=0.005, msg=res)     # 0.77, live


class OffPhaseKeepsTheLastOnFlow(unittest.TestCase):
    """Entry 444 (Sergeant D, Q-438b B): the live flow counts only in the ON phase. In the OFF phase the duty uses the flow of the
    last ON tick with the current request, so a cooling flow does not shorten the pause. Red side: the same source with the OFF
    branch removed (the 1693b64 behaviour, live flow in both phases)."""
    OFF_LINE = "  } else if (_pwm_lastOnFlowTemp > 0.0f) effectiveTemp = _pwm_lastOnFlowTemp;\n"   # entry 494 put braces around the ON branch

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        d = Path(cls.tmp.name)
        fix = hp.SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        if fix.count(cls.OFF_LINE) != 1:
            raise AssertionError("entry 444 OFF branch not found in SATcontrol.ino")
        cls.exe_fix = build(fix, d, "fix")
        if not cls.exe_fix:
            raise unittest.SkipTest("no host C++ compiler")
        cls.exe_mut = build(fix.replace(cls.OFF_LINE, "  }\n"), d, "mut")
        # ON phase at flow 40, request 34; find where the harness ends it, then cool the flow 1 C per tick in the OFF phase
        on = [(0, 0, "45.0", "34.0")] + [(t, 1, "40.0", "34.0") for t in range(30, 1230, 30)]
        r0 = run(cls.exe_fix, on)
        started = [t for t in sorted(r0) if r0[t][2] == 1]
        cls.t_off = next(t for t in sorted(r0) if t > started[0] and r0[t][2] == 0)
        cls.ticks = [x for x in on if x[0] <= cls.t_off]
        cls.off = [cls.t_off + 30 * k for k in range(1, 7)]
        cls.ticks += [(t, 0, "%.1f" % (39.0 - k), "34.0") for k, t in enumerate(cls.off)]
        cls.ticks.append((cls.off[-1] + 30, 0, "33.0", "35.0"))   # request change inside the OFF phase
        cls.fix = run(cls.exe_fix, cls.ticks)
        cls.mut = run(cls.exe_mut, cls.ticks)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_off_phase_duty_uses_the_last_on_flow(self):
        want = (34 - 27.2) / (40 - 27.2)   # 0.53
        got = [(t, self.fix[t]) for t in self.off]
        self.assertTrue(all(v[2] == 0 for _, v in got), got)   # still OFF: the pause is not cut short
        for t, v in got:
            with self.subTest(t=t):
                self.assertAlmostEqual(v[0], want, delta=0.005)

    def test_off_time_is_not_shortened_by_the_cooling_flow(self):
        offs = {self.fix[t][3] for t in self.off}
        self.assertEqual(len(offs), 1, offs)
        self.assertLess(self.mut[self.off[-1]][3], self.fix[self.off[-1]][3], (self.mut, self.fix))   # red side: shorter pause

    def test_request_change_in_off_uses_the_held_flow(self):
        t = self.off[-1] + 30
        self.assertAlmostEqual(self.fix[t][0], (35 - 27.2) / (40 - 27.2), delta=0.005, msg=self.fix)

    def test_pwm_reset_clears_the_held_flow(self):
        # Static check (the harness MAIN has no PWM re-entry): every way out of PWM goes through satPwmReset(), which must clear
        # the held flow so a new PWM session starts from the live flow (review finding, 2026-11-21).
        src = hp.SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        body = src.split("\nvoid satPwmReset()", 1)[1].split("\n}\n", 1)[0]
        self.assertIn("_pwm_lastOnFlowTemp         = 0.0f;", body)

    def test_red_side_follows_the_cooling_flow(self):
        t = self.off[-1]
        self.assertAlmostEqual(self.mut[t][0], (34 - 27.2) / (34.0 - 27.2), delta=0.005, msg=self.mut)


class MmReleaseAbove103(unittest.TestCase):
    """TASK-2992 (Sergeant D entries 436/440): MM=T (satPwmAtMaxDuty(), entries 242/243) only while the raw duty is above 1.03;
    range 5 itself (continuous ON, off time 0) still starts at dutyMax (0.90 at 4 cycles/h)."""
    setUpClass = classmethod(LiveFlowDuty.setUpClass.__func__)
    tearDownClass = classmethod(LiveFlowDuty.tearDownClass.__func__)

    def test_indigo_flame_never_releases_the_modulation(self):
        # the live-flow raw duty peaks at (31.97 - 27.2)/(31.9 - 27.2) = 1.015 (21:27:30): below 1.03, so no MM=T in the whole flame,
        # where the duty fix alone flipped MM=T at 21:25:00 / 21:26:00 / 21:26:30 (range 5 at 0.93, 0.86, 0.91)
        self.assertEqual([r["time"] for t, r in self.rows if self.fix[t][1]], [])

    def test_range_5_off_time_is_unchanged(self):
        # 21:27:30: clamped duty 1.00 (raw 1.015): range 5, off time 0 (the saturation signal), but no MM=T
        t = [t for t, r in self.rows if r["time"] == "21:27:30"][0]
        self.assertEqual((self.fix[t][0], self.fix[t][1], self.fix[t][3]), (1.0, 0, 0))

    def test_raw_duty_above_1_03_releases(self):
        # live flow 31.0, request 31.2: raw (31.2 - 27.2)/(31.0 - 27.2) = 1.053 > 1.03 -> MM=T; request 31.05: raw 1.013 -> no MM=T
        hi = run(self.exe_fix, [(0, 0, "45.0", "31.2"), (30, 1, "31.0", "31.2"), (70, 1, "31.0", "31.2")])
        lo = run(self.exe_fix, [(0, 0, "45.0", "31.05"), (30, 1, "31.0", "31.05"), (70, 1, "31.0", "31.05")])
        self.assertEqual((hi[70][1], lo[70][1]), (1, 0), (hi, lo))
        self.assertEqual((hi[70][3], lo[70][3]), (0, 0), "both are range 5 (off time 0): %r %r" % (hi, lo))

if __name__ == "__main__":
    unittest.main(verbosity=2)
