#!/usr/bin/env python3
"""TASK-1536: every CS SAT transmits stays at or below satEffectiveMaxSetpoint().

The control tick caps CS at satEffectiveMaxSetpoint() (boiler MaxTSet, the user or default cap of
Sergeant D entries 148/152, the hard ceiling). Three senders did not:
  * the 5 s resend (satCmdResend) and the 30 s PIC keepalive (satPicCSKeepalive) repeat the last CS,
    so a limit lowered after the tick was exceeded until the next tick (up to the control interval);
  * the thermal-safe fallback (CS=45 after 2 h without a room temperature) was capped by the user
    MaxTSet only (satGetMaxSetpoint), not by the boiler's own MaxTSet.

The real command block of src/OTGW-firmware/SATcontrol.ino (same cut as test_sat_cmd_cache.py) and the
real satThermalSafeSetpoint() are compiled with a host C++ compiler; satEffectiveMaxSetpoint() is a stub
whose value the scenario sets. OTGW_SAT_INO points at another copy for old-vs-fix runs
(e.g. `git show HEAD~1:src/OTGW-firmware/SATcontrol.ino`). Without a compiler the tests are skipped.

Run: python tests/test_sat_cs_ceiling_1536.py
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
import test_sat_cmd_cache as cc  # noqa: E402

SAT_INO = Path(os.environ.get("OTGW_SAT_INO", REPO / "src" / "OTGW-firmware" / "SATcontrol.ino"))

PRELUDE = cc.STUBS.split("@BODY@")[0].replace(
    "static float satEffectiveMaxSetpoint() { return 1000.0f; }",
    "static float g_max = 62.0f;\nstatic float satEffectiveMaxSetpoint() { return g_max; }")

MAIN = r"""
static const float SAT_THERMAL_SAFE_FLOW = 45.0f;
@SAFE@
int main(int argc, char** argv) {
  int mode = atoi(argv[1]);
  g_ms = 100000;
  if (mode == 0) {            // resend after the limit was lowered
    g_max = 62; satResetCmdCache(); satSendCS(60.0f); satSendMM(100); satSendCH(1);
    g_max = 40; g_sent[0] = 0; g_ms += 31000; satCmdResend();   // TASK-1924 Part 2: the repeat is due after 31 s
    printf("%s|%.1f\n", g_sent, state.sat.fLastSentCS);
  } else if (mode == 1) {     // PIC keepalive after the limit was lowered
    g_max = 62; satResetCmdCache(); satSendCS(60.0f);
    g_max = 40; g_sent[0] = 0; g_ms += 31000; satPicCSKeepalive();
    printf("%s|%.1f\n", g_sent, state.sat.fLastSentCS);
  } else if (mode == 2) {     // a CS below the limit is repeated unchanged
    g_max = 40; satResetCmdCache(); satSendCS(35.0f); satSendMM(100); satSendCH(1);
    g_sent[0] = 0; g_ms += 31000; satCmdResend();   // TASK-1924 Part 2: the repeat is due after 31 s
    printf("%s|%.1f\n", g_sent, state.sat.fLastSentCS);
  } else if (mode == 3) {     // thermal-safe fallback CS under a boiler MaxTSet of 40 and of 62
    g_max = 40; float a = satThermalSafeSetpoint();
    g_max = 62; float b = satThermalSafeSetpoint();
    printf("%.1f %.1f\n", a, b);
  }
  return 0;
}
"""


def safe_helper(src: str) -> str:
    m = re.search(r"static float satThermalSafeSetpoint\(\) \{.*?\n\}\n", src, re.S)
    return m.group(0) if m else ""


class CsCeiling(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.src = SAT_INO.read_text(encoding="utf-8")
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        cpp = d / "t.cpp"
        helper = safe_helper(cls.src)
        cls.has_helper = bool(helper)
        safe = helper or "static float satThermalSafeSetpoint() { return -1.0f; }   // absent in this copy"
        cpp.write_text(PRELUDE + cc.extract(cls.src) + MAIN.replace("@SAFE@", safe))
        cls.exe = compile_cpp(cpp, d / "t.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def run_mode(self, mode):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        return subprocess.run([str(self.exe), str(mode)], capture_output=True, text=True, check=True).stdout.strip()

    def test_resend_caps_a_cs_above_a_lowered_limit(self):
        # TASK-1570 (entry 237): the 5 s resend repeats CS only; MM and CH are on change
        self.assertEqual(self.run_mode(0), "CS=40.0;|40.0")

    def test_pic_keepalive_caps_a_cs_above_a_lowered_limit(self):
        self.assertEqual(self.run_mode(1), "CS=40.0;|40.0")

    def test_cs_below_the_limit_is_repeated_unchanged(self):
        self.assertEqual(self.run_mode(2), "CS=35.0;|35.0")

    def test_thermal_safe_fallback_respects_the_boiler_max(self):
        self.assertTrue(self.has_helper, "satThermalSafeSetpoint() missing: the fallback has no boiler-MaxTSet cap")
        self.assertEqual(self.run_mode(3), "40.0 45.0")

    def test_fallback_uses_the_capped_helper(self):
        # The thermal-safe branch of the control loop must take its CS from satThermalSafeSetpoint().
        i = self.src.index("if (estElapsed >= SAT_THERMAL_MAX_EST_MS)")
        branch = self.src[i:i + 600]
        self.assertIn("state.sat.fFinalSetpoint = satThermalSafeSetpoint();", branch)
        self.assertNotIn("satGetMaxSetpoint()", branch)


if __name__ == "__main__":
    unittest.main()
