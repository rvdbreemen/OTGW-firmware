#!/usr/bin/env python3
"""TASK-1412.19: the closed-loop simulation's hard checks go red on the defect they target, and runs are reproducible.

Each hard threshold of docs/sat/sim-1412-19/thresholds.md gets a mutant: the modelled SAT behaviour is switched off and
the check must fail; with the behaviour on it must pass. This proves the checks can see the defect in the MODEL; it does
not prove anything about the firmware beyond what the model reproduces (see the report).
Run: python tests/test_sat_closedloop_sim_1412_19.py
"""
import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "scripts"))
import sat_closedloop_sim_1412_19 as c
import sat_lowload_study_1412_16 as s16

ARGS = ("3_1983-92", "rad", "variable", 5.5, True)


class ClosedLoop(unittest.TestCase):
    def test_h5_reproducible(self):
        a = c.run("dhw", *ARGS, 3); b = c.run("dhw", *ARGS, 3)
        self.assertEqual(a, b)

    def test_h1_hour_limit_red_without_the_limit(self):
        self.assertTrue(c.run("very_low_load", *ARGS, 2)["pass"]["H1"])
        old = s16.GAS_MAX_STARTS_H
        s16.GAS_MAX_STARTS_H = 99
        try:
            r = c.run("very_low_load", *ARGS, 2)
        finally:
            s16.GAS_MAX_STARTS_H = old
        self.assertFalse(r["pass"]["H1"], r["pwm_starts_max_per_hour"])

    def test_h2_clamp_red_without_the_clamp(self):
        self.assertTrue(c.run("dhw", "3_1983-92", "ufh", "variable", 5.5, True, 1)["pass"]["H2"])
        orig = c.Sat.tick2
        def no_clamp(self, *a):
            spmax = self.e["spmax"]; self.e = dict(self.e, spmax=999.0)
            try:
                return orig(self, *a)
            finally:
                self.e = dict(self.e, spmax=spmax)
        c.Sat.tick2 = no_clamp
        try:
            r = c.run("dhw", "3_1983-92", "ufh", "variable", 5.5, True, 1)
        finally:
            c.Sat.tick2 = orig
        self.assertFalse(r["pass"]["H2"], r["max_cs"])

    def test_h3_silence_red_when_sat_keeps_sending(self):
        self.assertTrue(c.run("transport", *ARGS, 1)["pass"]["H3"])
        c.SILENT_AFTER_HANDBACK = False
        try:
            r = c.run("transport", *ARGS, 1)
        finally:
            c.SILENT_AFTER_HANDBACK = True
        self.assertFalse(r["pass"]["H3"])

    def test_h4_curve_only_red_on_a_stale_room(self):
        self.assertTrue(c.run("room_fault", *ARGS, 1)["pass"]["H4"])
        c.ROOM_LOST_CURVE_ONLY = False
        try:
            r = c.run("room_fault", *ARGS, 1)
        finally:
            c.ROOM_LOST_CURVE_ONLY = True
        self.assertFalse(r["pass"]["H4"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
