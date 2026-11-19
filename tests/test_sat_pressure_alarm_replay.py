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

Old-vs-fix (TASK-1867, Sergeant D entry 362 option B: pressure readings taken in the 600 s settle window after a CH-mode
change are thrown away, they no longer enter the sample ring):
  PressureAlarmReplayOldCode   replays the SATcontrol.ino from before the fix (git show OLD_COMMIT:..., a commit on
                               origin/dev) and asserts the three field alarms REPRODUCE: proof of the defect.
  PressureAlarmReplayFixed     replays the current SATcontrol.ino and asserts none of the three field alarms fires.
  SyntheticLoss                a steady real pressure loss still raises the alarm on the fix, with and without a CH-mode
                               change in front of it.
OTGW_SAT_INO=<path to another SATcontrol.ino> replaces the file under test of the Fixed and Synthetic classes (point it
at the old copy to see them go red); OTGW_OLD_SAT_INO replaces the old copy of the OldCode class.
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


OLD_COMMIT = "d090823f2"   # last SATcontrol.ino commit on origin/dev before the TASK-1867 fix (same satUpdatePressure())


def old_source():
    """SATcontrol.ino from before the fix: OTGW_OLD_SAT_INO, else git show OLD_COMMIT:... (None if git cannot give it)."""
    env = os.environ.get("OTGW_OLD_SAT_INO")
    if env:
        return Path(env).read_text(encoding="utf-8")
    r = subprocess.run(["git", "show", f"{OLD_COMMIT}:src/OTGW-firmware/SATcontrol.ino"], cwd=base.REPO,
                       capture_output=True)
    return r.stdout.decode("utf-8") if r.returncode == 0 and r.stdout else None


def compile_harness(wd: Path, ino: Path = None):
    cpp = wd / "t.cpp"
    saved = base.SAT_INO
    try:
        if ino is not None:
            base.SAT_INO = ino
        cpp.write_text(base.STUBS + base.extract_block() + MAIN, encoding="utf-8")
    finally:
        base.SAT_INO = saved
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


def run_exe(exe, rows):
    """rows: (ms, pressure, statusflags) per control tick -> list of dict(smoothed, drop, suspended, alarm)."""
    inp = "".join(f"{ms} {p:.5f} {sf}\n" for ms, p, sf in rows)
    out = subprocess.run([str(exe)], input=inp, capture_output=True, text=True, check=True).stdout
    res = []
    for line in out.splitlines():
        sm, dr, susp, al = line.split()
        res.append(dict(smoothed=float(sm), drop=float(dr), suspended=susp == "1", alarm=al == "1"))
    return res


class ReplayBase:
    """Shared helpers of the gruijter replay classes; OLD selects the pre-fix copy of SATcontrol.ino."""
    OLD = False

    @classmethod
    def replay(cls, first_run_seed_p):
        ev = load_fixture()
        t_end = ev[-1][0] + 1
        table = []
        for lo, hi, seed in ((ev[0][0], REBOOT_S, first_run_seed_p), (REBOOT_S, t_end, 0.0)):
            rows = ticks(ev, lo, hi, seed)
            res = run_exe(cls.exe, [(int(round((t - lo) * 1000)) + 1000, p, sb) for t, p, sb in rows])
            for (t, p, sb), r in zip(rows, res):
                table.append(dict(t=t, raw=p, sb=sb, **r))
        return table

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        wd = Path(cls.tmp.name)
        ino, label = None, str(base.SAT_INO)
        if cls.OLD:
            text = old_source()
            if text is None:
                cls.exe, cls.table, cls.no_old = None, [], True
                return
            ino = wd / "SATcontrol_old.ino"
            ino.write_text(text, encoding="utf-8")
            label = f"{OLD_COMMIT}:src/OTGW-firmware/SATcontrol.ino (before the fix)"
        cls.exe = compile_harness(wd, ino)
        cls.table = []
        if not cls.exe:
            return
        print(f"\n[{cls.__name__} under test: {label}]")
        cls.table = cls.replay(0.0)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    no_old = False

    def setUp(self):
        if self.no_old:
            self.skipTest(f"git cannot give {OLD_COMMIT}:SATcontrol.ino (shallow clone?); set OTGW_OLD_SAT_INO")
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


class PressureAlarmReplayOldCode(ReplayBase, unittest.TestCase):
    """The defect: on the code from before the fix the three field alarms reproduce."""
    OLD = True

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


FIELD_ALARMS = (("11 Nov 21:45:44", 21 * 3600 + 45 * 60 + 44.5), ("12 Nov 01:31:24", DAY + 3600 + 31 * 60 + 24.7),
                ("12 Nov 07:22:54", DAY + 7 * 3600 + 22 * 60 + 54.99))


class PressureAlarmReplayFixed(ReplayBase, unittest.TestCase):
    """The fix: the same replay on the current code raises none of the three field alarms (entry 362)."""

    def test_no_field_alarm_on_the_gruijter_replay(self):
        eps = self.episodes()
        print("\n  alarm episodes from the replay: " +
              (", ".join(f"{hms(a)}-{hms(b) if b else 'open'}" for a, b in eps) or "none"))
        for name, t in FIELD_ALARMS:
            near = [a for a, _ in eps if abs(a - t) <= 2 * TICK_S]
            self.assertFalse(near, f"field alarm {name} still fires in the replay at {[hms(a) for a in near]}")
        # Choice A (coordinator 2026-11-15): the first fit after a settle needs 600 s of fresh samples. With the 300 s
        # window (option B only) one 0.027 bar step raised a new alarm at 12 Nov 10:28:24 (drop 0.32-0.38 bar/h).
        self.assertFalse([a for a, _ in eps if abs(a - (DAY + 10 * 3600 + 28 * 60 + 24.99)) <= 2 * TICK_S],
                         "the 10:28:24 alarm of the B-only version fires")
        self.assertEqual(eps, [], "the fix raises no pressure alarm at all in this capture (pressure steady 1.7-2.0 bar)")

    def test_no_first_alarm_with_assumed_pre_capture_pressure(self):
        table = self.replay(2.0)      # same ASSUMPTION as the OldCode test: 2.0 bar before the first MsgID 18 frame
        t = FIELD_ALARMS[0][1]
        near = [a for a, _ in self.episodes(table) if abs(a - t) <= 2 * TICK_S]
        self.assertFalse(near, f"the 21:45 alarm still fires at {[hms(a) for a in near]}")
        self.assertEqual(self.episodes(table), [])

    def test_drop_rate_at_the_old_alarm_ticks_is_below_the_limit(self):
        for name, t in (("01:31", DAY + 3600 + 31 * 60 + 24.99), ("07:22", DAY + 7 * 3600 + 22 * 60 + 54.99)):
            row = min(self.table, key=lambda r: abs(r["t"] - t))
            self.assertFalse(row["alarm"], name)
            self.assertLess(row["drop"], 0.3, f"{name}: drop rate {row['drop']:.3f} bar/h at the old alarm tick")


class SyntheticLoss(unittest.TestCase):
    """A steady real pressure loss (0.5 bar/h) is still alarmed on the fix, with and without a CH-mode change first.
    Synthetic series through the REAL satUpdatePressure(): 30 s ticks, start 2.2 bar (above the 0.8 bar minimum for
    the whole run, so only the drop rate can raise the alarm)."""
    RATE_BAR_H = 0.5
    START_BAR = 2.2
    TICKS = 100 * 60 // 30        # 100 minutes

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = compile_harness(Path(cls.tmp.name))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def setUp(self):
        if not self.exe:
            if os.environ.get("OTGW_REQUIRE_HOST_CC"):
                self.fail("OTGW_REQUIRE_HOST_CC is set but no host C++ compiler was found")
            self.skipTest("no host C++ compiler (g++, clang++ or MSVC)")

    def run_series(self, ch_off_at_s):
        """CH-mode bit on until ch_off_at_s (None = unchanged for the whole run), then off. Returns [(t_s, result)]."""
        rows = []
        for k in range(self.TICKS):
            t = k * TICK_S
            sf = 0x0002 if (ch_off_at_s is None or t < ch_off_at_s) else 0
            rows.append((1000 + int(t * 1000), self.START_BAR - self.RATE_BAR_H * t / 3600.0, sf))
        return [(k * TICK_S, r) for k, r in enumerate(run_exe(self.exe, rows))]

    @staticmethod
    def onset(series):
        return next((t for t, r in series if r["alarm"]), None)

    def test_steady_loss_without_a_ch_mode_change_alarms(self):
        series = self.run_series(None)
        t = self.onset(series)
        print(f"\n  no CH-mode change: alarm onset at t={t} s")
        self.assertIsNotNone(t, "a steady loss of 0.5 bar/h must raise the pressure alarm")
        row = dict(series)[t]
        self.assertAlmostEqual(row["drop"], self.RATE_BAR_H, delta=0.05)
        self.assertGreaterEqual(t, 300 + 120 - TICK_S)       # >= 300 s window, then the 120 s confirmation

    def test_steady_loss_after_a_ch_mode_change_alarms_once_the_ring_has_refilled(self):
        fall = 120.0     # the CH-mode bit falls early (the ring holds < 300 s), the loss goes on unchanged
        series = self.run_series(fall)
        t = self.onset(series)
        print(f"\n  CH-mode bit falls at t={fall:.0f} s: alarm onset at t={t} s")
        self.assertIsNotNone(t, "the loss continues after the CH-mode change: the alarm must still be raised")
        # suspension 600 s, ring refills from there, choice A: 600 s of fresh samples for the first rate (B only: 300 s),
        # 120 s confirmation. Onset on the fix 1440 s; B-only version 1140 s (+300 s later on A); old code 840 s.
        self.assertGreaterEqual(t, fall + 600 + 600 + 120 - TICK_S)
        self.assertLessEqual(t, fall + 600 + 600 + 120 + 2 * TICK_S)
        self.assertAlmostEqual(dict(series)[t]["drop"], self.RATE_BAR_H, delta=0.05)


if __name__ == "__main__":
    unittest.main(verbosity=2)
