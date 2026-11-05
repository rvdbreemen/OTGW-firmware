#!/usr/bin/env python3
"""TASK-1655: the KD warm-up study's loop is a faithful copy of the TASK-1412.19 closed loop, and the KD factor lands in
the gain exactly as pre-registered (docs/sat/sim-1655/thresholds.md).

1. With KD x1.0 and no start offset, tools/research/sat/sat_kd_warmup_sim_1655.run gives the same scored numbers as
   tools/research/sat/sat_closedloop_sim_1412_19.run for a fault-free 1412.19 class (very_low_load, seed 2 = steady 12 C).
2. kd = round3(factor * 0.07 * 8400 * kp) for factor 1.0 / 0.9 / 0.8, and the original port gain code is restored after.
3. Same inputs give the same output (determinism, as H5 of 1412.19).
This proves the study uses the 1412.19 model; it proves nothing about the firmware beyond what that model reproduces.
Run: python tests/test_sat_kd_warmup_sim_1655.py
"""
import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools", "research", "sat"))
import sat_kd_warmup_sim_1655 as k
import sat_closedloop_sim_1412_19 as c19
import sat_sim_port as port


class KdWarmup(unittest.TestCase):
    def test_copy_matches_1412_19_run(self):
        ref = c19.run("very_low_load", "3_1983-92", "rad", "variable", 5.5, True, 2)
        self.assertEqual(ref["outdoor"], "steady_12")
        mine = k.run("3_1983-92", ref["mass"], "rad", "variable", 5.5, True, 12.0, 0.0, factor=1.0,
                     hours=c19.HOURS, skip_h=c19.SETTLE_H)
        self.assertEqual(mine["share_in_0_3_scored"], ref["S1_share_in_0_3"])
        self.assertEqual(mine["max_over_scored"], ref["S2_max_over"])
        self.assertEqual(mine["starts_per_day"], ref["S4_starts_per_day"])
        self.assertEqual(mine["max_cs"], ref["max_cs"])
        self.assertEqual(mine["pwm_starts_max_per_hour"], ref["pwm_starts_max_per_hour"])

    def test_kd_factor_in_the_gain_and_restored(self):
        orig = port.Pid._gains
        want = {1.0: 14112.0, 0.9: 12700.8, 0.8: 11289.6}   # coeff 1.6, curve 45 C, radiators: kp = 24.0
        for f, kd in want.items():
            with k.kd_factor(f):
                p = port.Pid(1.6, False); p._gains(45.0)
                self.assertAlmostEqual(float(p.kp), 24.0, places=3)
                self.assertAlmostEqual(float(p.ki), 0.003, places=6)
                self.assertAlmostEqual(float(p.kd), kd, places=2)
            self.assertIs(port.Pid._gains, orig)

    def test_lower_kd_changes_the_run(self):
        a = k.run("3_1983-92", "heavy", "rad", "fixed", 5.5, True, 8.0, -3.0, factor=1.0, hours=6)
        b = k.run("3_1983-92", "heavy", "rad", "fixed", 5.5, True, 8.0, -3.0, factor=0.8, hours=6)
        self.assertNotEqual(a["D_approach_mean"], b["D_approach_mean"])

    def test_deterministic(self):
        a = k.run("5_2015", "light", "ufh", "variable", 3.5, False, 0.0, -3.0, factor=0.9, hours=6)
        b = k.run("5_2015", "light", "ufh", "variable", 3.5, False, 0.0, -3.0, factor=0.9, hours=6)
        self.assertEqual(a, b)


if __name__ == "__main__":
    unittest.main(verbosity=2)
