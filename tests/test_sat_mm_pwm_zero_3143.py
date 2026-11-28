#!/usr/bin/env python3
"""TASK-3143 (Sergeant D entries 505, 507, 508): while PWM runs SAT keeps MM=0; MM=T is no longer sent at the maximum PWM duty.

REAL CODE: the SAT control loop, the PIC command queue and the boiler-frame feed of tests/test_sat_loop_integration.py (compiled from
src/, nothing re-implemented). The scenarios are that file's: S1 (steady low load, 40 min, PWM with ON and OFF phases) and S7 (cold
start: PWM-first start that reaches the maximum duty at once, TASK-1942).

What is pinned:
  1. Once PWM has been entered (MODE 2) no MM=T goes out on the wire, whatever the duty. The hand-back MM=T of entry 47 (SAT switched
     off, OT link lost, restart) is not PWM time and stays: tests/test_sat_loop_integration.py ("hand-back MM=T count") and
     tests/test_sat_disable_handback.py cover it and stay green.
  2. In PWM a gas boiler gets MM=0 (satComputeMM, entry 21); MM=0 is on the wire during the ON phase.
  3. The published current_modulation is the MM that is sent (entry 269 no longer has an MM=T to report).
Red on the code before TASK-3143 (MM=T at the maximum duty), green after.
"""
import re
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_sat_loop_integration as L  # noqa: E402

SAT_INO = (Path(__file__).resolve().parent.parent / "src" / "OTGW-firmware" / "SATcontrol.ino")


def mm_frames(r, a=0, b=10 ** 12):
    return [(t, x) for t, x in r.tx(a, b, "MM=")]


class MmInPwm(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.exe = L.build("base")

    def _pwm_start(self, r):
        modes = [t for t, x in r.kind("MODE") if x == "2"]
        self.assertTrue(modes, "PWM (mode 2) was never reached:\n" + r.dump(limit=25))
        return modes[0]

    def test_no_mm_t_on_the_wire_once_pwm_runs_steady_low_load(self):
        r = L.Run(self.exe, L.S1)
        t_pwm = self._pwm_start(r)
        late_t = [(t, x) for t, x in mm_frames(r, t_pwm) if x == "MM=T"]
        self.assertEqual(late_t, [], "MM=T sent while PWM runs (entry 505): %r" % late_t[:6])

    def test_cold_start_stays_continuous_and_sends_the_configured_maximum_never_mm_t(self):
        # TASK-3151.2 (ADR-305): a heating start runs in continuous, not in PWM, so the cold-start scenario S7 has no PWM stint
        # (it had the PWM-first start at the maximum duty before). What goes out is the continuous maximum, never MM=T.
        r = L.Run(self.exe, L.S7)
        self.assertEqual([t for t, x in r.kind("MODE") if x == "2"], [], "the cold start entered PWM:\n" + r.dump(limit=25))
        frames = mm_frames(r)
        self.assertTrue(frames, "no MM= at all in the cold start")
        self.assertTrue({x for t, x in frames} <= {"MM=100"}, "unexpected MM in the continuous cold start: %r" % frames[:6])

    def test_mm_zero_is_what_a_gas_boiler_gets_in_pwm(self):
        r = L.Run(self.exe, L.S1)
        t_pwm = self._pwm_start(r)
        vals = {x for t, x in mm_frames(r, t_pwm)}
        self.assertIn("MM=0", vals, "no MM=0 while PWM runs: %r" % sorted(vals))
        self.assertTrue(vals <= {"MM=0", "MM=100"}, "an MM other than 0 or the continuous maximum went out: %r" % sorted(vals))

    def test_the_control_tick_sends_what_satComputeMM_gave(self):
        src = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        self.assertRegex(src, r"const uint8_t mmTarget = state\.sat\.iCurrentModulation;")
        self.assertNotIn("SAT_MM_T", re.search(r"const uint8_t mmTarget[^\n]*", src).group(0))
        self.assertIsNone(re.search(r"iCurrentModulation\s*=\s*satReportedModulation\(", src),
                          "current_modulation is overridden with 100 for an MM=T that is no longer sent (entry 269)")


if __name__ == "__main__":
    unittest.main(verbosity=2)
