#!/usr/bin/env python3
"""TASK-1412.16: sanity and reproducibility checks of the simulation study (tools/research/sat/sat_lowload_study_1412_16.py).

What this proves: the study is deterministic, conserves energy in steady state, really enters SAT's PWM where the
report says it does, and the 'MM honoured' plant variant runs the burner at its minimum in every PWM period. It does not
prove that the study's SAT loop equals the firmware beyond the ported functions (curve, PID, PWM mapper, proven in
tests/test_sat_sim_port_equivalence.py); the report lists what is modelled and not proven.
Run: python tests/test_sat_lowload_study_1412_16.py
"""
import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools", "research", "sat"))
import sat_lowload_study_1412_16 as st


class Study(unittest.TestCase):
    def test_deterministic(self):
        a = st.simulate("gain_step_8", "sat", "rad", "variable", "3_1983-92", "heavy", 5.5, False)
        b = st.simulate("gain_step_8", "sat", "rad", "variable", "3_1983-92", "heavy", 5.5, False)
        self.assertEqual(a, b)

    def test_energy_balance_steady(self):
        for ctl in ("sat", "pi", "tpi"):
            r = st.simulate("steady_m5", ctl, "rad", "fixed", "3_1983-92", "heavy", 5.5, True,
                            params={"pi": {"kp": 6.0, "ti_h": 1.0}, "tpi": {"band": 1.0, "cycle": 1200.0}}.get(ctl))
            self.assertLess(abs(r["heat_kwh"] / r["loss_kwh"] - 1.0), 0.06, (ctl, r["heat_kwh"], r["loss_kwh"]))

    def test_energy_closes_in_transient_scenarios(self):
        """Delivered heat + gains - losses equals the change in stored heat (air, wall, emitter), every scenario."""
        for scn in ("warmup_5", "gain_step_8", "falling_demand"):
            for ctl in ("sat", "tpi"):
                r = st.simulate(scn, ctl, "ufh", "variable", "5_2015", "heavy", 5.5, True)
                self.assertLess(r["energy_closure"], 1e-6, (scn, ctl))

    def test_sat_enters_pwm_at_low_load(self):
        r = st.simulate("steady_5", "sat", "rad", "fixed", "3_1983-92", "heavy", 5.5, True)
        self.assertGreater(len(r.get("periods", [])), 10)

    def test_mm_honoured_runs_at_minimum(self):
        r = st.simulate("steady_5", "sat", "rad", "fixed", "3_1983-92", "heavy", 5.5, True)
        self.assertTrue(all(abs(p["flame_power_kw"] - 5.5) < 1e-6 for p in r["periods"] if p["on_frac"] > 0))
        r2 = st.simulate("steady_5", "sat", "rad", "fixed", "3_1983-92", "heavy", 5.5, False)
        self.assertTrue(any(p["flame_power_kw"] > 5.6 for p in r2["periods"]))

    def test_mutant_boiler_ignoring_pwm_off_is_seen(self):
        """If the PWM OFF phase did not stop the burner (CH ignored), starts per PWM period would drop to ~0:
        the period metric must see it."""
        orig = st.Boiler.step
        def ignore_ch(self, cs, ch, flow, draw, pwm_active):
            return orig(self, max(cs, 40.0), True, flow, draw, pwm_active)
        st.Boiler.step = ignore_ch
        try:
            r = st.simulate("steady_5", "sat", "rad", "fixed", "3_1983-92", "heavy", 5.5, True)
        finally:
            st.Boiler.step = orig
        ok = st.simulate("steady_5", "sat", "rad", "fixed", "3_1983-92", "heavy", 5.5, True)
        self.assertNotEqual(len(r.get("periods", [])), len(ok["periods"]))


if __name__ == "__main__":
    unittest.main(verbosity=2)
