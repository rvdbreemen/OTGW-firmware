#!/usr/bin/env python3
"""TASK-1867: replay gruijter's MsgID 0 / MsgID 18 series (capture 20261111-213113, beta.182) through the REAL
satUpdatePressure() and show what raises the pressure alarm.

The function under test is NOT re-implemented: the real pressure block is cut out of SATcontrol.ino and compiled with a
host C++ compiler, as tests/test_sat_pressure_health.py does (its STUBS, extract_block() and _vcvars() are reused). The
fixture tests/fixtures/sat_pressure_gruijter_20261111.csv holds only timestamps, the boiler's MsgID 0 slave-status low
byte (change-only) and the MsgID 18 values; no other capture data.

Inputs per control tick: Statusflags = last MsgID 0 slave byte (bit 1 CH mode, bit 3 flame), CHPressure = last MsgID 18.
The control tick is 30 s (settings.sat.iControlInterval) and satUpdatePressure() runs once per tick; the tick phase is
the one the device logged (alarm and clear lines at second 24.99 / 54.99 of the minute).
The ESP rebooted at ~01:07:55 on 12 Nov (task watchdog, reboot #96): the state is reset there (second run).

Field log of the same capture (SAT: PRESSURE ALARM lines, set / cleared):
  11 Nov 21:45:44.5 (smoothed 1.82, drop 1.126) / 21:58:14.5
  12 Nov 01:31:24.7 (smoothed 1.79, drop 1.695) / 01:52:54.7
  12 Nov 07:22:54.99 (smoothed 1.81, drop 1.758) / 07:25:24.99

Not verifiable from the capture: the device booted ~21:22 on 11 Nov, before the capture starts at 21:31, and the MsgID 18
reads between boot and 21:34:33 are not in it, so the first run starts with an empty pressure history.

Old-vs-fix: OTGW_SAT_INO=<path to another SATcontrol.ino> replays that copy.
Print the whole table: OTGW_REPLAY_VERBOSE=1. Run: python tests/test_sat_pressure_alarm_replay.py
"""
import csv
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_sat_pressure_health as base  # noqa: E402

FIXTURE = Path(__file__).resolve().parent / "fixtures" / "sat_pressure_gruijter_20261111.csv"
DAY = 86400.0
TICK_S = 30.0
PHASE_S = 24.99          # tick = second 24.99 (mod 30) of the minute: the device's logged alarm/clear stamps
REBOOT_S = DAY + 1 * 3600 + 7 * 60 + 55   # 12 Nov 01:07:55 (task watchdog reboot #96)

MAIN = r"""
int main() {
  unsigned long ms; float p; unsigned sf;
  while (scanf("%lu %f %u", &ms, &p, &sf) == 3) {
    g_ms = (uint32_t)ms;
    OTcurrentSystemState.CHPressure = p;
    OTcurrentSystemState.Statusflags = (uint16_t)sf;
    satUpdatePressure();
    printf("%.4f %.4f %d %d\n", state.sat.fSmoothedPressure, state.sat.fPressureDropRate,
           _press_suspended ? 1 : 0, state.sat.bPressureAlarm ? 1 : 0);
  }
  return 0;
}
"""


def load_fixture():
    ev = []
    with open(FIXTURE, newline="") as fh:
        rows = csv.reader(l for l in fh if not l.startswith("#"))
        next(rows)
        for t, mid, v in rows:
            ev.append((float(t), int(mid), float(v) if mid == "18" else int(v)))
    return ev


def hms(t):
    t = t % DAY
    return f"{int(t // 3600):02d}:{int(t % 3600 // 60):02d}:{t % 60:05.2f}"


def ticks(ev, t0, t1, seed_p=0.0):
    """One row per control tick in [t0, t1): (t, raw bar, slave byte). Values are the last frames before the tick.
    seed_p is the pressure the device held before the first MsgID 18 frame of the window (0 = no reading yet)."""
    first = t0 - (t0 % TICK_S) + PHASE_S
    if first < t0:
        first += TICK_S
    rows, i, p, sb = [], 0, seed_p, 0
    while first < t1:
        while i < len(ev) and ev[i][0] <= first:
            if ev[i][1] == 18:
                p = ev[i][2]
            else:
                sb = ev[i][2]
            i += 1
        rows.append((first, p, sb))
        first += TICK_S
    return rows


def compile_harness(wd: Path):
    cpp = wd / "t.cpp"
    cpp.write_text(base.STUBS + base.extract_block() + MAIN, encoding="utf-8")
    exe = wd / "t.exe"
    for cc in ("g++", "clang++"):
        if shutil.which(cc):
            subprocess.run([cc, "-std=c++17", "-o", str(exe), str(cpp)], check=True)
            return exe
    bat = base._vcvars()
    if bat:
        cmd = f'call "{bat}" >nul && cl /nologo /utf-8 /std:c++17 /EHsc /Fe:"{exe}" /Fo:"{wd}\\\\" "{cpp}"'
        r = subprocess.run(cmd, shell=True, cwd=wd, capture_output=True, text=True)
        if r.returncode != 0:
            raise AssertionError("host compile failed:\n" + r.stdout + r.stderr)
        return exe
    return None


class PressureAlarmReplay(unittest.TestCase):
    @classmethod
    def replay(cls, first_run_seed_p):
        ev = load_fixture()
        t_end = ev[-1][0] + 1
        table = []
        for lo, hi, seed in ((ev[0][0], REBOOT_S, first_run_seed_p), (REBOOT_S, t_end, 0.0)):
            rows = ticks(ev, lo, hi, seed)
            inp = "".join(f"{int(round((t - lo) * 1000)) + 1000} {p:.5f} {sb}\n" for t, p, sb in rows)
            out = subprocess.run([str(cls.exe)], input=inp, capture_output=True, text=True, check=True).stdout
            for (t, p, sb), line in zip(rows, out.splitlines()):
                sm, dr, susp, al = line.split()
                table.append(dict(t=t, raw=p, sb=sb, smoothed=float(sm), drop=float(dr),
                                  suspended=susp == "1", alarm=al == "1"))
        return table

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = compile_harness(Path(cls.tmp.name))
        cls.table = []
        if not cls.exe:
            return
        print(f"\n[under test: {base.SAT_INO}]")
        cls.table = cls.replay(0.0)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def setUp(self):
        if not self.exe:
            if os.environ.get("OTGW_REQUIRE_HOST_CC"):
                self.fail("OTGW_REQUIRE_HOST_CC is set but no host C++ compiler was found")
            self.skipTest("no host C++ compiler (g++, clang++ or MSVC)")

    def episodes(self, table=None):
        eps, cur = [], None
        for r in (self.table if table is None else table):
            if r["alarm"] and cur is None:
                cur = r["t"]
            elif not r["alarm"] and cur is not None:
                eps.append((cur, r["t"]))
                cur = None
        if cur is not None:
            eps.append((cur, None))
        return eps

    def window(self, t, before, after, table=None):
        return [r for r in (self.table if table is None else table) if t - before <= r["t"] <= t + after]

    @staticmethod
    def dump(rows):
        print("   time      raw   smoothed  drop(bar/h)  CHmode flame suspended alarm")
        for r in rows:
            print(f"  {hms(r['t'])}  {r['raw']:.3f}   {r['smoothed']:.3f}    {r['drop']:7.3f}      "
                  f"{(r['sb'] >> 1) & 1}      {(r['sb'] >> 3) & 1}      {int(r['suspended'])}        {int(r['alarm'])}")

    def test_replay_reproduces_the_field_alarms(self):
        eps = self.episodes()
        print("\n  alarm episodes from the replay: " +
              ", ".join(f"{hms(a)}-{hms(b) if b else 'open'}" for a, b in eps))
        if os.environ.get("OTGW_REPLAY_VERBOSE"):
            self.dump(self.table)
        field = [("11 Nov 21:45:44", 21 * 3600 + 45 * 60 + 44.5), ("12 Nov 01:31:24", DAY + 3600 + 31 * 60 + 24.7),
                 ("12 Nov 07:22:54", DAY + 7 * 3600 + 22 * 60 + 54.99)]
        for name, t in field:
            near = [a for a, _ in eps if abs(a - t) <= TICK_S]
            print(f"  field alarm {name}: replay onset {'at ' + hms(near[0]) if near else 'NOT reproduced'}")
            if near:
                print("   -- table around it --")
                self.dump(self.window(near[0], 1200, 180))
        # 12 Nov 01:31 and 07:22 lie after the 01:08 reboot with a known history: both must reproduce
        for name, t in field[1:]:
            self.assertTrue([a for a, _ in eps if abs(a - t) <= TICK_S], f"alarm {name} not reproduced")

    def test_alarm_values_match_the_device_log(self):
        # the device logged smoothed=1.81 drop=1.758 at 07:22:54.99 and smoothed=1.79 drop=1.695 at 01:31:24.7
        for t, sm, dr in ((DAY + 7 * 3600 + 22 * 60 + 54.99, 1.81, 1.758), (DAY + 3600 + 31 * 60 + 24.7, 1.79, 1.695)):
            row = min(self.table, key=lambda r: abs(r["t"] - t))
            # 01:31: the EMA seed after the reboot is the first MsgID 18 read, which is not in the capture window; 0.03 bar
            self.assertAlmostEqual(row["smoothed"], sm, delta=0.03, msg=hms(t))
            self.assertAlmostEqual(row["drop"], dr, delta=0.1, msg=hms(t))

    def test_first_alarm_with_assumed_pre_capture_pressure(self):
        # ASSUMPTION, not capture data: the boiler held 2.0 bar (pump on, as at every run in the capture) up to the first
        # MsgID 18 frame in the capture (21:34:33, 1.707 bar). The capture starts 21:31:14, after the 21:22 boot.
        table = self.replay(2.0)
        eps = self.episodes(table)
        t = 21 * 3600 + 45 * 60 + 44.5
        near = [a for a, _ in eps if abs(a - t) <= 2 * TICK_S]   # field onset is 50 s later than the assumed-seed replay
        print("\n  21:45 alarm with the assumed 2.0 bar before 21:34:33: replay onset " +
              (hms(near[0]) if near else "NOT reproduced"))
        if near:
            self.dump(self.window(near[0], 900, 90, table))
        self.assertTrue(near, "assumed 2.0 bar before the capture does not reproduce the 21:45 alarm")

    def test_trigger_is_the_pressure_step_after_the_ch_mode_bit_fell(self):
        # For the 01:31 and 07:22 alarms: the CH-mode bit (slave bit 1) fell, the 600 s suspension ran, the pressure
        # step came AFTER the bit fell (pump stop), and the alarm followed when the suspension ended with the step still
        # in the fresh sample window. Flame-off lies more than 600 s before the bit fell (or there is no flame at all),
        # so a 600 s suspension from flame-off (entry 262) would already be over.
        for name, t in (("01:31", DAY + 3600 + 31 * 60 + 24.99), ("07:22", DAY + 7 * 3600 + 22 * 60 + 54.99)):
            before = [r for r in self.table if r["t"] <= t]
            onset = before[-1]
            self.assertTrue(onset["alarm"] and not onset["suspended"], name)
            fall = next(r for r in reversed(before) if (r["sb"] >> 1) & 1)      # last tick with CH mode still on
            fall = next(r for r in before if r["t"] > fall["t"])                # first tick with it off
            step = next(r for r in before if r["t"] > fall["t"] and r["raw"] < before[before.index(fall) - 1]["raw"] - 0.2)
            flames = [r["t"] for r in before if (r["sb"] >> 3) & 1 and r["t"] < fall["t"] and r["t"] >= REBOOT_S]
            last_flame = flames[-1] if flames else None
            print(f"\n  {name}: CH-mode bit falls {hms(fall['t'])}, pressure step seen {hms(step['t'])} "
                  f"(+{step['t'] - fall['t']:.0f} s), alarm {hms(onset['t'])} (+{onset['t'] - fall['t']:.0f} s), "
                  f"last flame tick before the fall: {hms(last_flame) if last_flame else 'none since the reboot'}")
            self.assertGreater(step["t"], fall["t"])
            self.assertLess(step["t"] - fall["t"], 600)           # inside the suspension, so the ring keeps the step
            self.assertGreaterEqual(onset["t"] - fall["t"], 600)  # alarm only after the suspension ended
            if last_flame is not None:
                # flame-off to pressure step: a 600 s suspension from flame-off (entry 262) is over before the step
                self.assertGreater(step["t"] - last_flame, 600)


if __name__ == "__main__":
    unittest.main(verbosity=2)
