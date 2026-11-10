#!/usr/bin/env python3
"""TASK-1784: the KD 0.05 study compares against today's 0.063 on exactly the TASK-1655 model and inputs.
1. The absolute-K gain formula gives kd = round3(K * 8400 * kp): 12700.8 for K 0.063 and 10080.0 for K 0.05 at kp 24
   (coeff 1.6, curve 45 C, radiators), and the port's gain code is restored after each run.
2. Fairness: the K 0.063 baseline reproduces committed TASK-1655 x0.9 rows (docs/sat/sim-1655/runs.json) exactly,
   one warm-up row and one guard row, so the 0.063 column is the same model as the 1655 study.
Run: python tests/test_sat_kd_005_sim.py
"""
import json, os, sys, unittest
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
sys.path.insert(0, os.path.join(ROOT, "tools", "research", "sat"))
import sat_kd_005_sim as s
import sat_kd_warmup_sim_1655 as k
import sat_sim_port as port

SCORED = ("min_to_0_3_scored", "min_to_0_1_scored", "max_over", "max_over_scored", "starts_per_day",
          "share_in_0_3_scored", "rms_after_0_3", "D_approach_mean", "max_cs")


def committed_x09():
    with open(os.path.join(ROOT, "docs", "sat", "sim-1655", "runs.json"), encoding="utf-8") as f:
        return [r for r in json.load(f)["runs"] if r["kd_factor"] == 0.9]


class Kd005(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        s.install()

    def test_absolute_k_in_the_gain_and_restored(self):
        orig = port.Pid._gains
        for key, kd in {1.0: 12700.8, 0.794: 10080.0}.items():
            with k.kd_factor(key):
                p = port.Pid(1.6, False); p._gains(45.0)
                self.assertAlmostEqual(float(p.kp), 24.0, places=3)
                self.assertAlmostEqual(float(p.kd), kd, places=2)
            self.assertIs(port.Pid._gains, orig)

    def _check(self, ref, mine):
        for f in SCORED:
            self.assertEqual(mine[f], ref[f], f)

    def test_baseline_reproduces_1655_x09_warmup_row(self):
        ref = next(r for r in committed_x09() if r["kind"] == "warmup" and r["plant_class"] == "3_1983-92/light/rad"
                   and r["outdoor"] == 5.0 and r["d0"] == -4.0)
        mine = k.run(ref["era"], ref["mass"], ref["emitter"], ref["flow"], ref["pmin_kw"], ref["mm_honoured"],
                     ref["outdoor"], ref["d0"], factor=1.0)
        self._check(ref, mine)

    def test_baseline_reproduces_1655_x09_guard_row(self):
        ref = next(r for r in committed_x09() if r["kind"] == "guard" and r["plant_class"] == "5_2015/heavy/ufh"
                   and r["scenario"] == "gain_step_8")
        fo, fg = k.GUARD_SCENARIOS[ref["scenario"]]
        mine = k.run(ref["era"], ref["mass"], ref["emitter"], ref["flow"], ref["pmin_kw"], ref["mm_honoured"],
                     fo, 0.0, factor=1.0, gain=fg)
        self._check(ref, mine)


if __name__ == "__main__":
    unittest.main()
