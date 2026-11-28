#!/usr/bin/env python3
"""TASK-3151.4, D1 (Robert 2026-11-28 "D1 ook nu", register entry 527): the PWM duty uses thermo-nova's HELD effective on-temperature.

thermo-nova pwm.py: the temperature BT is seeded from the flow at the first flame (:130-131), at an ON start with a lit flame
(:164-167) and at the first ignition of an ON phase (:189-191), smoothed by EMA 0.3/0.7 for the first 180 s after the flame-on
(:144-149, window = HEATER_STARTUP_TIMEFRAME, inclusive) and then HELD; the duty reads that held value in the ON and in the OFF
phase (:229-239), before any seed the live flow (:232). It replaces the live-flow duty of Sergeant D's entries 437/438/444 (TASK-2993).
Before D1: duty = live flow inside the ON phase after a 30 s filter, and the flow of the last ON tick in the OFF phase.

The code under test is the REAL PWM block of SATcontrol.ino, cut and compiled by tests/test_sat_hp_pwm.py (shared harness), driven with
this file's MAIN. The thermo-nova rule itself is re-stated here as a small oracle (bt_oracle) because pwm.py cannot run on the host:
the tests compare the compiled firmware duty with that oracle and with the pre-D1 side (BASE_REV).
  1. synthetic ON phase: the duty follows the oracle in the window (EMA, inclusive of the tick 180 s after the ignition) and after it
     (held), and differs from the pre-D1 live-flow side after the window;
  2. the OFF phase keeps the HELD value, not the flow of the last ON tick (a late rise of the flow in the ON phase does not move it);
  3. the indigo_light 19 Nov replay (tests/fixtures/sat_replay_2987/): with the held value the duty is pinned at 1.00 from 21:28:30
     while the flow rises 34.5 -> 41.9 (range 5, off time 0), the pre-D1 side gives 0.74 ... 0.47; a documented consequence of D1;
  4. MM release above a raw duty of 1.03 (entry 440, TASK-2992) still depends on the duty value: checked with the held value.
Run: python tests/test_sat_pwm_live_flow_duty_2993.py      Old side: OTGW_DUTY_BASE (default 6e801a216)
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
BASE_REV = os.environ.get("OTGW_DUTY_BASE", "6e801a216")   # lane-poc-modes before D1 (live-flow duty, entries 437/438/444): pinned old side

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


BASE = 27.2   # radiators, satGetBaseOffset()
WINDOW = 180.0 + 1.0   # HEATER_STARTUP_TIMEFRAME, inclusive, with the 1 s tick-jitter slack of the firmware


def bt_oracle(ticks):
    """thermo-nova's held effective on-temperature on a single ON phase that starts with the first tick (flame at ticks[k][1]):
    seed at the first lit tick, EMA 0.3/0.7 while the tick is within the window after that flame-on, the duty reads the value held
    BEFORE this tick's EMA update (pwm.py: duty at :135, EMA at :144-149). ticks: (sec, flame, flow, request)."""
    eff, t_on, out = None, None, {}
    for sec, flame, flow, req in ticks:
        flow, req = float(flow), float(req)
        if eff is None and flame:
            eff = flow
            t_on = sec
        use = eff if eff is not None else flow
        out[sec] = min(1.0, max(0.0, (req - BASE) / (max(use, BASE + 1.0) - BASE)))
        if flame and t_on is not None and (sec - t_on) <= WINDOW and sec != t_on:
            eff = 0.3 * flow + 0.7 * eff
    return out


class HeldBt(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        d = Path(cls.tmp.name)
        fix = hp.SAT_INO.read_text(encoding="utf-8")
        old = subprocess.run(["git", "show", f"{BASE_REV}:src/OTGW-firmware/SATcontrol.ino"], cwd=HERE.parent,
                             capture_output=True, check=True).stdout.decode("utf-8")
        cls.exe_fix = build(fix, d, "fix")
        cls.exe_old = build(old, d, "old")
        if not cls.exe_fix:
            raise unittest.SkipTest("no host C++ compiler")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    # --- 1. one ON phase: ignition at 30 s, flow 40, the flow then changes; request 34, 30 s ticks
    TICKS = [(0, 0, "45.0", "34.0"), (30, 1, "40.0", "34.0"), (60, 1, "30.0", "34.0"), (90, 1, "30.0", "34.0"),
             (120, 1, "30.0", "34.0"), (150, 1, "30.0", "34.0"), (180, 1, "30.0", "34.0"), (210, 1, "30.0", "34.0"),
             (240, 1, "36.0", "34.0"), (270, 1, "36.0", "34.0")]

    def test_duty_follows_the_held_value_in_and_after_the_window(self):
        want = bt_oracle(self.TICKS)
        got = run(self.exe_fix, self.TICKS)
        for sec, flame, flow, req in self.TICKS:
            if sec < 30:
                continue
            with self.subTest(sec=sec):
                self.assertAlmostEqual(got[sec][0], want[sec], delta=0.006, msg="%r vs %r" % (got[sec], want[sec]))

    def test_after_the_window_the_duty_does_not_follow_the_live_flow(self):
        # 240 s is 210 s after the ignition: outside the 180 s window. Live flow 36 would give 0.77; the held value is about 31
        got = run(self.exe_fix, self.TICKS)
        live = (34 - BASE) / (36 - BASE)
        self.assertGreater(got[240][0], live + 0.1, got)
        old = run(self.exe_old, self.TICKS)
        self.assertAlmostEqual(old[240][0], live, delta=0.006, msg="the pre-D1 side follows the live flow: %r" % old)

    def test_window_includes_the_tick_180_s_after_the_ignition(self):
        # the tick at 210 s is 180 s after the ignition at 30 s: its EMA step still runs, the duty at 240 s reads the result
        ticks = [(0, 0, "45.0", "34.0"), (30, 1, "40.0", "34.0"), (210, 1, "30.0", "34.0"), (240, 1, "30.0", "34.0"),
                 (270, 1, "30.0", "34.0")]
        got = run(self.exe_fix, ticks)
        want = bt_oracle(ticks)
        self.assertAlmostEqual(got[240][0], want[240], delta=0.006, msg=(got, want))
        self.assertAlmostEqual(got[270][0], want[240], delta=0.006, msg="held after the window: %r" % got)

    # --- 2. the OFF phase keeps the held value
    def test_off_phase_uses_the_held_value_not_the_last_on_flow(self):
        on = [(0, 0, "45.0", "34.0")] + [(t, 1, "40.0" if t < 300 else "44.0", "34.0") for t in range(30, 1230, 30)]
        r0 = run(self.exe_fix, on)
        started = [t for t in sorted(r0) if r0[t][2] == 1]
        t_off = next(t for t in sorted(r0) if t > started[0] and r0[t][2] == 0)
        off = [t_off + 30 * k for k in range(1, 5)]
        ticks = [x for x in on if x[0] <= t_off] + [(t, 0, "%.1f" % (36.0 - k), "34.0") for k, t in enumerate(off)]
        got = run(self.exe_fix, ticks)
        held = (34 - BASE) / (40 - BASE)   # seed 40; the flow 44 came after the 180 s window: held, not the last ON flow
        for t in off:
            with self.subTest(t=t):
                self.assertEqual(got[t][2], 0, got)
                self.assertAlmostEqual(got[t][0], held, delta=0.006, msg=got[t])

    # --- 3. indigo_light 19 Nov: the pinned duty is the documented consequence
    def test_indigo_replay_pins_the_duty_at_the_maximum_like_before_entry_437(self):
        rws = rows()
        ticks = [(t, int(r["flame"]), r["flow"], r["pid"]) for t, r in rws]
        new, old = run(self.exe_fix, ticks), run(self.exe_old, ticks)
        late = [t for t, r in rws if r["time"] >= "21:28:30" and r["mode"] == "2"]
        self.assertTrue(late)
        for t in late:
            self.assertEqual(new[t][0], 1.0, "held value: pinned at 1.00 (as logged before TASK-2993)")
        self.assertTrue(all(old[t][0] < 0.8 for t in late[:6]), "the pre-D1 side follows the rising flow: %r" % [old[t][0] for t in late])

    # --- 4. entry 440: MM release above a raw duty of 1.03 (the flag still depends on the duty)
    def test_raw_duty_above_1_03_releases(self):
        hi = run(self.exe_fix, [(0, 0, "45.0", "31.2"), (30, 1, "31.0", "31.2"), (70, 1, "31.0", "31.2")])
        lo = run(self.exe_fix, [(0, 0, "45.0", "31.05"), (30, 1, "31.0", "31.05"), (70, 1, "31.0", "31.05")])
        self.assertEqual((hi[70][1], lo[70][1]), (1, 0), (hi, lo))
        self.assertEqual((hi[70][3], lo[70][3]), (0, 0), "both are range 5 (off time 0): %r %r" % (hi, lo))


if __name__ == "__main__":
    unittest.main(verbosity=2)
