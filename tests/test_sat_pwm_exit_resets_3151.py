#!/usr/bin/env python3
"""TASK-3151.4 (ADR-305 decision 8, architect Must, coordinator 2026-11-28): every PWM exit resets the PWM state, as thermo-nova
pwm.disable() calls reset() (pwm.py:87-100, :106-109), and the effective on-temperature is never persisted (pwm.py:111-115 restores
only the enabled flag).

Source-level checks on the real SATcontrol.ino (the functions touch the whole SAT state and cannot be compiled alone on the host):
(1) satPwmReset() clears _pwm_effectiveBoilerTemp, the last ON flow, the ON/OFF phase, the ignition wait and the anchor;
(2) every exit path runs it: a mode change (satSetControlMode, which saturation, underheat, the verdict, the maximum setpoint and
    the control tick use), the heater-off branch (cold cutoff, valves closed, summer pause) and satDisable (SAT off, trip, OT loss);
(3) nothing keeps an ON time across an exit: no _pwm_onKept*, no satPwmKept*;
(4) the effective on-temperature is not written to flash: it is not named in the PWM-flag persistence block or any save function.
Red on 90e7d7525's parent c433d5387 for (3) is not available (the kept ON time went in c96212949); (3) is red on 3c7314011.

OTGW_SAT_INO points at another copy of SATcontrol.ino (old-vs-fix).
Run: python tests/test_sat_pwm_exit_resets_3151.py
"""
import os
import re
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SAT_INO = Path(os.environ.get("OTGW_SAT_INO", REPO / "src" / "OTGW-firmware" / "SATcontrol.ino"))


def func(src, signature, end_marker):
    i = src.index(signature)
    return src[i:src.index(end_marker, i)]


class PwmExitResets(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.src = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")

    def test_reset_clears_the_effective_on_temperature_and_the_phase(self):
        body = func(self.src, "void satPwmReset()", "\n}\n")
        for needle in (r"_pwm_effectiveBoilerTemp\s*=\s*0\.0f", r"_pwm_lastOnFlowTemp\s*=\s*0\.0f", r"_pwm_flameOnMs\s*=\s*0;",
                       r"_pwm_waitingForFlame\s*=\s*false", r"bPwmFlameRequested\s*=\s*false", r"_pwm_idle\s*=\s*true"):
            self.assertRegex(body, needle)

    def test_a_mode_change_resets(self):
        body = func(self.src, "static void satSetControlMode(SATControlMode m)", "\n}\n")
        self.assertIn("satPwmReset();", body)

    def test_the_heater_off_branch_resets(self):
        body = func(self.src, "static void satPwmDisableOnHeaterOff()", "\n}\n")
        self.assertIn("satPwmReset();", body)

    def test_switching_sat_off_resets(self):
        body = func(self.src, "void satDisable(bool persistPid)", "\n}\n")
        self.assertIn("satPwmReset();", body)

    def test_no_on_time_is_kept_across_an_exit(self):
        self.assertNotRegex(self.src, r"_pwm_onKept|satPwmKept")

    def test_the_effective_on_temperature_is_never_persisted(self):
        flag = func(self.src, "// --- satPwmFlag begin", "// --- satPwmFlag end")
        self.assertNotIn("_pwm_effectiveBoilerTemp", flag)
        for m in re.finditer(r"^(?:static )?\w+ (sat\w*Save\w*|sat\w*Persist\w*)\(", self.src, re.M):
            start = m.start()
            end = self.src.index("\n}\n", start)
            self.assertNotIn("_pwm_effectiveBoilerTemp", self.src[start:end], m.group(0))


if __name__ == "__main__":
    unittest.main(verbosity=2)
