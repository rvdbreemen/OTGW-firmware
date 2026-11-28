#!/usr/bin/env python3
"""TASK-3099 bench fixtures, re-read for TASK-3151.4 (entry 499 replaced): the three replay files (scripts/gen_sat_pwm_tap_replay.py) fed to the REAL PWM block.

The fixture lines are decoded as the SAT would see them (MsgID 0 slave status: flame 0x08, DHW mode 0x04; MsgID 25 flow; 3 s per
exchange), turned into flame / DHW-bit / flow events and run through the harness of tests/test_sat_pwm_frozen_flow_dhw.py (real
satApplyPWM/satPwmSchedule, control tick every 30 s, a DHW tick runs the schedule and sends nothing, as satControlLoopRun() does since D4).
The ON phase starts at second 0 of the fixture (gas, 3 cycles/h, duty 0.22 at flow 50: ON 264 s, OFF 936 s).

  C (no tap) is the control. A (tap, flame stays on) and B (tap, flame out, cooling flow in the wait, re-ignition) used to give the pause of C
  after the fix of entry 499 (TASK-3099). Entry 499 is replaced (Robert 2026-11-28, ADR-305 decision 8): the OFF phase keeps the flow of the last ON
  tick whatever a draw did to it, so A and B now give the LONGER pause of the heated flow (the one the defect check of TASK-3099 showed) and
  the same pause as the same fixture with the DHW bit removed. The tests pin that, and a mutant with the exception back must turn them red.

OTGW_SAT_INO=<path> points at another SATcontrol.ino. Run: python tests/test_sat_pwm_tap_replay.py
"""
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_sat_hp_pwm as hp  # noqa: E402

FIX_DIR = Path(__file__).resolve().parent / "fixtures" / "sat_pwm_tap_3099"
PERIOD = 1200

MAIN = r"""
#include <iostream>
#include <vector>
#include <string>
struct Ev { int sec; std::string kind; double val; };
int main() {
  settings = Settings(); state = State(); OTcurrentSystemState = OTSys();
  g_flame = false; g_flow = 50; g_ret = 30; g_due = true; g_hourLimit = false; g_hasOT = true; g_cold = 28.2f;
  satPwmReset();
  _pwm_effectiveBoilerTemp = 50.0f;
  _sat_lastControlCS = 0; _sat_thermalSafeActive = false;
  state.sat.eControlMode = SAT_MODE_PWM; state.sat.bDhwActive = false;
  std::string cmd; int cph = 3; double ratio = 0.22, dur = 0; std::vector<Ev> evs;
  while (std::cin >> cmd) {
    if (cmd == "cfg") std::cin >> cph >> ratio;
    else if (cmd == "ev") { Ev e; std::cin >> e.sec >> e.kind >> e.val; evs.push_back(e); }
    else if (cmd == "run") std::cin >> dur;
  }
  settings.sat.bOvershootProtection = true;
  settings.sat.iCyclesPerHour = (uint8_t)cph;
  float pid = 27.2f + (float)ratio * (50.0f - 27.2f);
  unsigned long long baseMs = 100000000ULL; g_ms = (uint32_t)baseMs; const uint32_t base = g_ms;
  g_flame = true; g_flameOnStart = base - 4000000UL; g_flameOffStart = base - 5000000UL;
  state.sat.bPwmFlameRequested = false;
  _pwm_entryPending = true; _pwm_lastUpdateMs = base;
  for (int sec = 0; sec <= (int)dur; sec++) {
    g_ms = base + (uint32_t)sec * 1000UL;
    for (auto& e : evs) {
      if (e.sec != sec) continue;
      if (e.kind == "dhw") state.sat.bDhwActive = e.val != 0;
      else if (e.kind == "flow") g_flow = (float)e.val;
      else if (e.kind == "flame" && (e.val != 0) != g_flame) {
        g_flame = e.val != 0;
        if (g_flame) g_flameOnStart = g_ms; else g_flameOffStart = g_ms;
      }
    }
    if (sec % 30 != 0) continue;
    if (state.sat.bDhwActive) {                                  // the control loop's DHW branch (D4, cbec269e9): the schedule runs with the last request, nothing is sent
      if (state.sat.eControlMode == SAT_MODE_PWM && pid > satColdThreshold()) satPwmSchedule(pid);
      printf("T %d %d %.1f %.1f %.1f %.4f %lu\n", sec, (int)state.sat.bPwmFlameRequested, _sat_lastControlCS, g_flow, _pwm_lastOnFlowTemp,
             state.sat.fPwmDutyCycle, (unsigned long)satPwmLastOffTimeMs());
      continue;
    }
    const float cs = satApplyPWM(pid);
    _sat_lastControlCS = cs;
    printf("T %d %d %.1f %.1f %.1f %.4f %lu\n", sec, (int)state.sat.bPwmFlameRequested, cs, g_flow, _pwm_lastOnFlowTemp,
           state.sat.fPwmDutyCycle, (unsigned long)satPwmLastOffTimeMs());
  }
  return 0;
}
"""


def events(name, seconds=PERIOD):
    """Flame / dhw / flow events from the first `seconds` of a fixture (one exchange = 3 s, the B line carries the boiler answer)."""
    lines = (FIX_DIR / f"{name}.txt").read_text(encoding="ascii").split()
    out, prev = [], {}
    for n in range(len(lines) // 4):
        b = lines[4 * n + 2]
        assert b[0] == "B", b
        frame = int(b[1:], 16)
        mid, val, t = (frame >> 16) & 0xFF, frame & 0xFFFF, 3 * n
        if t >= seconds:
            break
        if mid == 0:
            slave = val & 0xFF
            for kind, bit in (("flame", 0x08), ("dhw", 0x04)):
                v = 1 if slave & bit else 0
                if prev.get(kind) != v:
                    out.append((t, kind, v)); prev[kind] = v
        elif mid == 25:
            flow = round(val / 256.0, 1)
            if prev.get("flow") != flow:
                out.append((t, "flow", flow)); prev["flow"] = flow
    return out


class TapReplay(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        hp.MAIN = MAIN
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = hp.build(Path(cls.tmp.name))
        cls.rows = {}

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def setUp(self):
        if not self.exe:
            self.skipTest("no host C++ compiler (g++, clang++ or MSVC)")

    def run_fixture(self, name, drop_dhw=False, exe=None):
        key = (name, drop_dhw, str(exe))
        if key not in self.rows:
            ev = [e for e in events(name) if not (drop_dhw and e[1] == "dhw")]
            lines = ["cfg 3 0.22", f"run {PERIOD - 30}"] + [f"ev {s} {k} {v}" for s, k, v in ev]
            out = subprocess.run([str(exe or self.exe)], input="\n".join(lines) + "\n", capture_output=True, text=True, check=True).stdout
            rows = {}
            for ln in out.splitlines():
                p = ln.split()
                rows[int(p[1])] = dict(on=int(p[2]), flow=float(p[4]), last=float(p[5]), duty=float(p[6]), off=int(p[7]))
            self.rows[key] = rows
        return self.rows[key]

    def off_pause(self, rows):
        """OFF time the PWM block computed in the first OFF phase (ms), read from the second OFF tick (the first is the ON->OFF edge)."""
        off = [s for s in sorted(rows) if s > 270 and rows[s]["on"] == 0]
        return rows[off[1]]["off"]

    def test_fixtures_match_the_scenarios(self):
        a, b, c = (dict((k, None) for k in ()) for _ in range(3))
        ev = {n: events(n) for n in "ABC"}
        self.assertFalse([e for e in ev["C"] if e[1] in ("dhw",)] and any(e[2] for e in ev["C"] if e[1] == "dhw"), "C has no tap")
        self.assertTrue(any(e[1] == "dhw" and e[2] == 1 for e in ev["A"]) and any(e[1] == "dhw" and e[2] == 0 for e in ev["A"]))
        self.assertFalse(any(e[1] == "flame" and e[2] == 0 for e in ev["A"]), "A: the flame stays on")
        self.assertTrue(any(e[1] == "flame" and e[2] == 0 for e in ev["B"]), "B: the flame goes out after the tap")
        self.assertTrue(any(e[1] == "flame" and e[2] == 1 and e[0] > 252 for e in ev["B"]), "B: and relights")

    def test_control_without_tap_has_the_clean_pause(self):
        rows = self.run_fixture("C")
        self.assertEqual(self.off_pause(rows), 936001)
        self.assertAlmostEqual(rows[300]["duty"], 0.22, places=3)

    CLEAN = 936001
    HEATED = 1016488   # the pause of a stored flow of 60 C (duty 0.153)

    def test_tap_with_the_flame_on_gives_the_pause_of_the_heated_flow(self):
        # Entry 499 replaced: no freeze, the draw-heated flow of the last ON tick is stored, the pause is longer than the control's.
        a, c = self.off_pause(self.run_fixture("A")), self.off_pause(self.run_fixture("C"))
        self.assertEqual(a, self.HEATED)
        self.assertGreater(a, c + 40000, "A: tap with the flame on")

    def test_tap_with_the_flame_out_gives_the_pause_of_the_heated_flow(self):
        b, c = self.off_pause(self.run_fixture("B")), self.off_pause(self.run_fixture("C"))
        self.assertEqual(b, self.HEATED)
        self.assertGreater(b, c + 20000, "B: tap with the flame out")

    def test_the_dhw_bit_changes_nothing_in_the_replays(self):
        # The same fixtures with the DHW bit removed give the same rows: nothing in the PWM block reads it.
        for name in "ABC":
            self.assertEqual(self.run_fixture(name), self.run_fixture(name, drop_dhw=True), name)

    def test_mutant_with_the_hot_water_exception_back_is_caught(self):
        # entry-499 idea in memory: a draw tick does not store its flow. A and B then give the control pause again, which the tests above reject.
        src = hp.SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        old = "  if (state.sat.bPwmFlameRequested) {\n    _pwm_lastOnFlowTemp = effectiveTemp;"
        self.assertEqual(src.count(old), 1)
        d = Path(self.tmp.name) / "m_exc"
        d.mkdir(exist_ok=True)
        (d / "SATcontrol.ino").write_text(src.replace(old, "  if (state.sat.bPwmFlameRequested && !state.sat.bDhwActive) {   // MUTANT\n    _pwm_lastOnFlowTemp = effectiveTemp;"), encoding="utf-8")
        saved = hp.SAT_INO
        hp.SAT_INO = d / "SATcontrol.ino"
        try:
            exe = hp.build(d)
        finally:
            hp.SAT_INO = saved
        a = self.off_pause(self.run_fixture("A", exe=exe))
        self.assertNotEqual(a, self.HEATED, "the mutant must change the pause of A")
        print("\nMUTANT hot-water exception RED: pause of A %d instead of %d" % (a, self.HEATED))


if __name__ == "__main__":
    unittest.main()
