#!/usr/bin/env python3
"""TASK-1480: host test for satUpdatePressure() (sat/pressure_alarm, sat/pressure_drop_rate).

The function under test is NOT re-implemented here. The test cuts the real pressure block
(the sample FIFO, the CH-active tracking and satUpdatePressure()) out of
src/OTGW-firmware/SATcontrol.ino, wraps it in a stub header and compiles it with a host C++
compiler (g++, clang++ or MSVC cl via vswhere). Without a compiler the test is skipped.

The reference is a line-by-line transcription of thermo-nova's pressure health sensor
(custom_components/sat/binary_sensor.py at 7beaaf5): is_on :253-295, the EMA :315-323,
the confirmation :325-333, the CH-active tracking :335-345, the suspension :347-355, the
sample window :357-365 and the least-squares drop rate :367-388, constants :25-28.
coordinator.active is the master CH enable bit (serial coordinator, coordinator/serial/__init__.py:53-55),
which is Statusflags bit 8 in the firmware.

The field-shaped series is synthetic, built from the values reported for gruijter's capture
(TASK-1474, alpha.527): 2.0 bar while the boiler's CH-mode bit is set (pump incl. overrun),
1.70-1.74 bar after the pump stops, MsgID 18 read about every 5 min. The capture itself is not
part of the repo (private tester data). To replay a real capture-mqtt-debug transcript through the
same compiled function and the same oracle, set OTGW_PRESSURE_CAPTURE=<transcript.txt>; the case
prints the field, firmware and thermo-nova (bit 8 and bit 1) alarm-episode counts.

OTGW_REQUIRE_HOST_CC=1 turns the missing-compiler skip into a failure (for CI hosts).

Old-vs-fix: OTGW_SAT_INO=<path to an older SATcontrol.ino> runs the same cases against it, e.g.
  git show origin/dev:src/OTGW-firmware/SATcontrol.ino > old.ino
  OTGW_SAT_INO=old.ino python tests/test_sat_pressure_health.py

Run: python tests/test_sat_pressure_health.py
"""
import collections
import os
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SAT_INO = Path(os.environ.get("OTGW_SAT_INO", REPO / "src" / "OTGW-firmware" / "SATcontrol.ino"))

STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#define PSTR(s) (s)
#define F(s) (s)
#define SATDebugTf(...) do {} while (0)
#define SATDebugTln(...) do {} while (0)
struct SatSettings { float fMinPressure = 0.8f, fMaxPressure = 2.5f, fMaxPressureDrop = 0.3f; };
struct Settings { SatSettings sat; };
struct SatState { bool bActive = false; float fSmoothedPressure = 0, fPressureDropRate = 0;
                  bool bPressureAlarm = false; uint32_t iPressureAlarmSinceMs = 0; bool bPressureHealthy = true;
                  float fLastPressure = 0; uint32_t iLastPressureMs = 0, iLastSeenPressureMs = 0; };
struct State { SatState sat; };
struct OTSys { float CHPressure = 0; uint16_t Statusflags = 0; };
static Settings settings; static State state; static OTSys OTcurrentSystemState;
static uint32_t g_ms = 0;
static uint32_t millis() { return g_ms; }
"""

MAIN = r"""
// stdin: one evaluation per line "ms pressure statusflags bActive"
// stdout: one line per evaluation "dropRate alarm"
int main() {
  unsigned long ms; float p; unsigned sf; int act;
  while (scanf("%lu %f %u %d", &ms, &p, &sf, &act) == 4) {
    g_ms = (uint32_t)ms;
    OTcurrentSystemState.CHPressure = p;
    OTcurrentSystemState.Statusflags = (uint16_t)sf;
    state.sat.bActive = act != 0;
    satUpdatePressure();
    printf("%.6f %d\n", state.sat.fPressureDropRate, state.sat.bPressureAlarm ? 1 : 0);
  }
  return 0;
}
"""

MASTER_CH = 0x0100   # Statusflags bit 8: master CH enable
SLAVE_CH = 0x0002    # Statusflags bit 1: slave CH mode


def extract_block() -> str:
    src = SAT_INO.read_text(encoding="utf-8")
    start = src.find("// --- Pressure sample")
    m = re.search(r"^static void satUpdatePressure\(\)\s*\{.*?^\}\s*$", src[start:], re.S | re.M)
    if start < 0 or not m:
        raise AssertionError("pressure block / satUpdatePressure() not found in SATcontrol.ino")
    return src[start:start + m.end()]


def _vcvars():
    vswhere = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / \
        "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if not vswhere.exists():
        return None
    out = subprocess.run([str(vswhere), "-latest", "-products", "*", "-requires",
                          "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                          "-property", "installationPath"],
                         capture_output=True, text=True).stdout.strip()
    bat = Path(out) / "VC" / "Auxiliary" / "Build" / "vcvars64.bat" if out else None
    return bat if bat and bat.exists() else None


def build(workdir: Path):
    cpp = workdir / "t.cpp"
    cpp.write_text(STUBS + extract_block() + MAIN, encoding="utf-8")
    exe = workdir / "t.exe"
    for cc in ("g++", "clang++"):
        if shutil.which(cc):
            subprocess.run([cc, "-std=c++17", "-o", str(exe), str(cpp)], check=True)
            return exe
    bat = _vcvars()
    if bat:
        cmd = f'call "{bat}" >nul && cl /nologo /utf-8 /std:c++17 /EHsc /Fe:"{exe}" /Fo:"{workdir}\\\\" "{cpp}"'
        r = subprocess.run(cmd, shell=True, cwd=workdir, capture_output=True, text=True)
        if r.returncode != 0:
            raise AssertionError("host compile failed:\n" + r.stdout + r.stderr)
        return exe
    return None


class ThermoNovaPressureHealth:
    """Transcription of SatPressureHealthSensor (binary_sensor.py at 7beaaf5). Reference only."""
    SETTLE = 600            # :25
    ALPHA = 0.05            # :26
    CONFIRM = 120           # :27
    MIN_WINDOW = 300        # :28

    def __init__(self, minimum=0.8, maximum=2.5, max_age=1800, max_drop=0.3):
        # defaults: const.py:157-160 (pressure_max_value_age "00:30:00")
        self.minimum, self.maximum, self.max_age, self.max_drop = minimum, maximum, max_age, max_drop
        self.samples = collections.deque()
        self.smoothed = None
        self.first_detected = None
        self.last_active = None
        self.suspended_until = None
        self.last_drop_rate = None

    def is_on(self, now, raw, active):                                   # :253-295
        self._track_active_state(now, active)                            # :262
        smoothed = self._update_smoothed(raw)                            # :274
        self._record(now, raw, self.max_age)                             # :275
        drop_rate = self._drop_rate()                                    # :277
        low, high = smoothed < self.minimum, smoothed > self.maximum     # :282-283
        allowed = self._allowed(now)                                     # :284
        if not allowed:                                                  # :286-288
            drop_rate = None
            self.last_drop_rate = None
        elif drop_rate is not None:                                      # :289-290
            self.last_drop_rate = round(drop_rate, 3)
        drop_high = drop_rate is not None and drop_rate > self.max_drop  # :292
        return self._confirm(now, low or high or drop_high), drop_rate, allowed

    def _update_smoothed(self, raw):                                     # :315-323
        if self.smoothed is None:
            self.smoothed = raw
        else:
            self.smoothed = self.ALPHA * raw + (1 - self.ALPHA) * self.smoothed
        return self.smoothed

    def _confirm(self, now, condition):                                  # :325-333
        if not condition:
            self.first_detected = None
            return False
        if self.first_detected is None:
            self.first_detected = now
        return (now - self.first_detected) >= self.CONFIRM

    def _track_active_state(self, now, active):                          # :335-345
        if self.last_active is None:
            self.last_active = active
            return
        if self.last_active != active:
            self.suspended_until = now + self.SETTLE
            self.samples.clear()
        self.last_active = active

    def _allowed(self, now):                                             # :347-355
        if self.suspended_until is None:
            return True
        if now >= self.suspended_until:
            self.suspended_until = None
            return True
        return False

    def _record(self, now, pressure, max_age):                           # :357-365
        if max_age <= 0:
            max_age = 3600
        window = max(max_age, 3600)
        self.samples.append((now, pressure))
        while self.samples and (now - self.samples[0][0]) > window:
            self.samples.popleft()

    def _drop_rate(self):                                                # :367-388
        if len(self.samples) < 3:
            return None
        if self.samples[-1][0] - self.samples[0][0] < self.MIN_WINDOW:
            return None
        n = len(self.samples)
        st = sp = stp = st2 = 0.0
        for t, p in self.samples:
            st += t
            sp += p
            stp += t * p
            st2 += t * t
        denom = n * st2 - st * st
        if denom == 0:
            return None
        return -((n * stp - st * sp) / denom) * 3600


def field_series(hours=4.5):
    """Synthetic series shaped like TASK-1474: 40-min cycles, master CH enable on for 15 min,
    the boiler's CH-mode bit (pump) on for 25 min (10 min overrun), MsgID 18 read every 300 s:
    2.0 bar with the pump on, 1.70-1.74 bar with it off. SAT stays on (bActive true).
    Calls every 30-37 s (deterministic jitter), as from the control tick."""
    rows, ms, k, held = [], 1000, 0, None
    next_read = 0
    end = int(hours * 3600 * 1000)
    while ms < end:
        cyc = (ms // 1000) % 2400
        master = cyc < 900
        pump = cyc < 1500
        if ms >= next_read:
            held = 2.0 if pump else 1.70 + 0.01 * ((ms // 300000) % 5)
            next_read += 300000
        sf = (MASTER_CH if master else 0) | (SLAVE_CH if pump else 0)
        rows.append((ms, held, sf, 1))
        ms += 30000 + (k * 7919) % 7001
        k += 1
    return rows


def episodes(alarms):
    return sum(1 for i, a in enumerate(alarms) if a and (i == 0 or not alarms[i - 1]))


_FRAME = re.compile(r"^(\d\d):(\d\d):(\d\d\.\d+) .*?processOT.*?\s([TRBA])([0-9A-F]{8})\s+(\d+)\s+\S+\s+([>-])")
_PUB = re.compile(r"^(\d\d):(\d\d):(\d\d\.\d+) .*?TopicId \[[^\]]*/sat/(active|pressure_alarm)\] --> Message \[(\w+)\]")


def capture_series(path, tick_s=30.0):
    """Replay a capture-mqtt-debug transcript (TASK-1474 shape) as satUpdatePressure() inputs.

    Uses only the frames the firmware itself stored (log marker '>', i.e. is_value_valid_for_master_topic:
    a T-frame replaced by an R-frame, or a B-frame replaced by an A-frame, is marked '-').
    MsgID 0 master byte -> Statusflags bits 8-15, slave byte -> bits 0-7; MsgID 18 -> CHPressure (f8.8).
    bActive follows the published sat/active. Evaluations on a fixed tick_s grid from the first
    pressure reading (the firmware calls satUpdatePressure() once per 30 s control tick).
    Returns (rows, field_alarm_episodes)."""
    events, master, slave = [], 0, 0
    field_alarm, prev_alarm = 0, False
    with open(path, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            m = _FRAME.match(line)
            if m:
                hh, mm, ss, kind, hexw, msgid, mark = m.groups()
                if mark != ">":
                    continue
                t = int(hh) * 3600 + int(mm) * 60 + float(ss)
                w = int(hexw, 16)
                hb, lb = (w >> 8) & 0xFF, w & 0xFF
                if msgid == "0" and kind in "TR":
                    events.append((t, "master", hb))
                elif msgid == "0" and kind in "BA":
                    events.append((t, "slave", lb))
                elif msgid == "18" and kind in "BA" and ((w >> 28) & 7) == 4:
                    events.append((t, "p", hb + lb / 256.0))
                continue
            m = _PUB.match(line)
            if m:
                hh, mm, ss, topic, val = m.groups()
                t = int(hh) * 3600 + int(mm) * 60 + float(ss)
                on = val == "true"
                if topic == "active":
                    events.append((t, "active", on))
                else:
                    if on and not prev_alarm:
                        field_alarm += 1
                    prev_alarm = on
    events.sort(key=lambda e: e[0])
    first_p = next(t for t, k, _ in events if k == "p")
    end = events[-1][0]
    rows, i, p, act = [], 0, 0.0, False
    tick = first_p
    while tick <= end:
        while i < len(events) and events[i][0] <= tick:
            _, k, v = events[i]
            if k == "master":
                master = v
            elif k == "slave":
                slave = v
            elif k == "p":
                p = v
            else:
                act = v
            i += 1
        rows.append((int(round((tick - first_p) * 1000)) + 1000, p, (master << 8) | slave, 1 if act else 0))
        tick += tick_s
    return rows, field_alarm


class SatPressureHealth(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = build(Path(cls.tmp.name))
        print(f"\n[under test: {SAT_INO}]")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def setUp(self):
        if not self.exe:
            if os.environ.get("OTGW_REQUIRE_HOST_CC"):
                self.fail("OTGW_REQUIRE_HOST_CC is set but no host C++ compiler was found (g++, clang++ or MSVC)")
            self.skipTest("no host C++ compiler (g++, clang++ or MSVC)")

    def firmware(self, rows):
        # rows carry a monotonic ms (the oracle's clock); the firmware sees millis(), which wraps at 2^32
        inp = "".join(f"{ms & 0xFFFFFFFF} {p:.4f} {sf} {act}\n" for ms, p, sf, act in rows)
        out = subprocess.run([str(self.exe)], input=inp, capture_output=True, text=True, check=True).stdout
        return [(float(a), b == "1") for a, b in (l.split() for l in out.splitlines())]

    @staticmethod
    def oracle(rows, bit=MASTER_CH):
        tn = ThermoNovaPressureHealth()
        return [tn.is_on(ms / 1000.0, p, (sf & bit) != 0) for ms, p, sf, _ in rows]

    def assert_matches_oracle(self, rows):
        fw, ref = self.firmware(rows), self.oracle(rows)
        for i, ((rate, alarm), (on, drop, allowed)) in enumerate(zip(fw, ref)):
            t = rows[i][0] / 1000.0
            self.assertEqual(alarm, on, f"alarm differs at t={t:.0f}s (fw {alarm}, thermo-nova {on})")
            if not allowed:
                self.assertEqual(rate, 0.0, f"suspended at t={t:.0f}s but drop rate {rate}")
            elif drop is not None and drop >= 0:
                self.assertAlmostEqual(rate, drop, delta=2e-3, msg=f"drop rate at t={t:.0f}s")
        return fw, ref

    def test_field_series_matches_thermo_nova(self):
        rows = field_series()
        fw, ref = self.firmware(rows), self.oracle(rows)
        slave = self.oracle(rows, SLAVE_CH)
        print(f"  field series: {len(rows)} evaluations over {rows[-1][0] / 3.6e6:.1f} h; alarm episodes: "
              f"firmware {episodes([a for _, a in fw])}, thermo-nova (master CH enable) "
              f"{episodes([o[0] for o in ref])}, thermo-nova (slave CH-mode bit) {episodes([o[0] for o in slave])}")
        self.assert_matches_oracle(rows)

    @unittest.skipUnless(os.environ.get("OTGW_PRESSURE_CAPTURE"),
                         "set OTGW_PRESSURE_CAPTURE=<capture transcript> to replay a field capture (kept out of the repo)")
    def test_capture_replay_matches_thermo_nova(self):
        rows, field = capture_series(os.environ["OTGW_PRESSURE_CAPTURE"])
        fw = self.firmware(rows)
        ref, slave = self.oracle(rows), self.oracle(rows, SLAVE_CH)
        suspended = sum(1 for o in ref if not o[2]) / len(ref)
        print(f"  capture replay: {len(rows)} evaluations over {(rows[-1][0] - rows[0][0]) / 3.6e6:.2f} h; "
              f"alarm episodes: field (published sat/pressure_alarm) {field}, firmware under test "
              f"{episodes([a for _, a in fw])}, thermo-nova (master CH enable) {episodes([o[0] for o in ref])}, "
              f"thermo-nova (slave CH-mode bit) {episodes([o[0] for o in slave])}; "
              f"drop rate suspended {suspended:.0%} of evaluations (master CH enable)")
        self.assert_matches_oracle(rows)

    def test_ch_enable_transition_suspends_and_clears(self):
        # SAT stays on; master CH enable drops at t=1800 s. 600 s suspension (drop rate 0), then the
        # rate restarts from samples taken after the transition only.
        rows = []
        for i in range(120):
            ms = 1000 + i * 30000
            ch = MASTER_CH if ms < 1800000 else 0
            p = 2.0 if ms < 1800000 else 1.7
            rows.append((ms, p, ch, 1))
        fw = self.firmware(rows)
        for i, (ms, *_rest) in enumerate(rows):
            if 1800000 <= ms < 1800000 + 600000:
                self.assertEqual(fw[i][0], 0.0, f"t={ms/1000:.0f}s: drop rate not suspended after CH enable fell")
        self.assertFalse(any(a for _, a in fw), "the cleared samples must not raise an alarm")
        self.assert_matches_oracle(rows)

    def test_suspension_survives_millis_wrap(self):
        # millis() wraps after ~49.7 days. A CH enable change 300 s before the wrap must still suspend
        # the drop rate for 600 s (thermo-nova uses a monotonic clock, binary_sensor.py:335-355).
        wrap = 1 << 32
        rows = []
        for i in range(80):
            ms = wrap - 1800000 + i * 30000
            ch = MASTER_CH if ms < wrap - 300000 else 0
            rows.append((ms, 2.0 - 0.0005 * i, ch, 1))
        fw = self.firmware(rows)
        for i, (ms, *_rest) in enumerate(rows):
            if wrap - 300000 <= ms < wrap + 300000:
                self.assertEqual(fw[i][0], 0.0, f"t={ms - wrap:+d} ms around the wrap: drop rate not suspended")
        self.assert_matches_oracle(rows)

    def test_sat_on_off_alone_does_not_suspend(self):
        # CH enable constant; SAT's own bActive toggles. thermo-nova does not look at SAT on/off.
        rows = []
        for i in range(60):
            ms = 1000 + i * 30000
            rows.append((ms, 2.0 - 0.0001 * i, MASTER_CH, 1 if ms < 900000 else 0))
        fw = self.firmware(rows)
        after = [r for (r, _), row in zip(fw, rows) if 900000 <= row[0] < 1500000]
        self.assertTrue(all(r > 0 for r in after), f"drop rate suspended on SAT on/off: {after[:5]}")
        self.assert_matches_oracle(rows)

    def test_long_run_wraps_and_evicts_like_thermo_nova(self):
        # 3 h with CH enable on throughout (no clear), jittered 30-37 s calls: the FIFO fills, wraps and
        # evicts by age. Pressure: slow leak 0.25 bar/h for 90 min, a 0.3 bar step, then flat.
        rows, ms, k = [], 1000, 0
        while ms < 3 * 3600 * 1000:
            h = ms / 3.6e6
            p = 2.0 - 0.25 * min(h, 1.5) - (0.3 if h > 2.0 else 0.0)
            rows.append((ms, round(p, 3), MASTER_CH, 1))
            ms += 30000 + (k * 7919) % 7001
            k += 1
        fw, ref = self.assert_matches_oracle(rows)
        print(f"  long run: {len(rows)} evaluations, alarm episodes firmware {episodes([a for _, a in fw])}, "
              f"thermo-nova {episodes([o[0] for o in ref])}")

    def test_samples_up_to_one_hour_old_count(self):
        # 40 min at 2.0 bar, then 1.9 bar flat. thermo-nova still sees the step 15 min later
        # (window 3600 s); a 10-minute window sees a flat line.
        rows = [(1000 + i * 30000, 2.0 if i < 80 else 1.9, MASTER_CH, 1) for i in range(110)]
        fw, ref = self.assert_matches_oracle(rows)
        self.assertGreater(fw[-1][0], 0.05, "a step 25 min back no longer shapes the drop rate")


if __name__ == "__main__":
    unittest.main(verbosity=2)
