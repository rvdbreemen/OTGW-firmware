"""Scratch check (TASK-1527, Sergeant D 14:46 UTC): does the real SAT PWM mapper break with a 7200 s heat-pump cycle?
Runs the existing real-code suite tests/test_sat_hp_pwm.py against patched copies (OTGW_SAT_INO / OTGW_SETTINGS_INO)
that add a third option 7200 s, plus 7200 s cases computed from thermo-nova's formulas (pwm.py:247-301) with P = 7200:
lower 180/7200 = 0.025, upper 0.975, min 0.0125, max 0.9875."""
import sys, unittest
sys.path.insert(0, r"D:/Users/Robert/Documents/GitHub/RvdB/OTGW-firmware/tests")
import test_sat_hp_pwm as hp

class HP7200(hp.SatHeatPumpPwm):
    def test_7200_mid_060(self):        self.assertMap(hp.SRC_HP, 7200, 3, 0.60, True, (4320, 2880))
    def test_7200_mid_030(self):        self.assertMap(hp.SRC_HP, 7200, 3, 0.30, True, (2160, 5040))
    def test_7200_low_range(self):      self.assertMap(hp.SRC_HP, 7200, 3, 0.02, False, (180, 8820))
    def test_7200_high_range(self):     self.assertMap(hp.SRC_HP, 7200, 3, 0.98, False, (8820, 180))
    def test_7200_ultra_low_flame(self):   self.assertMap(hp.SRC_HP, 7200, 3, 0.01, True, (180, 14220))
    def test_7200_ultra_low_noflame(self): self.assertMap(hp.SRC_HP, 7200, 3, 0.01, False, (0, 14400))
    def test_7200_over_max(self):       self.assertMap(hp.SRC_HP, 7200, 3, 0.99, False, (14400, 0))
    def test_7200_on_phase_ends(self):
        r = self.run_sc("map", hp.SRC_HP, 7200, 3, 0.30, 1, 2200); self.assertEqual(r["req"], "0")
    def test_7200_on_phase_holds(self):
        r = self.run_sc("map", hp.SRC_HP, 7200, 3, 0.30, 1, 1000); self.assertEqual(r["req"], "1")

if __name__ == "__main__":
    unittest.main(verbosity=1)
