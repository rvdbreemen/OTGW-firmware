#!/usr/bin/env python3
"""TASK-2010: SAT thermal drop learning (TASK-21, our own feature, not in thermo-nova) learns only while SAT controls the boiler.

Found by 09 on .63 (2026-11-16): settings.sat.fThermalCoeff went 0.0665 -> 0.0595 with no boiler (ot_link_lost) and the simulation
off. satUpdateThermalLearning() ran on every satControlLoopRun() pass before the safety-trip, boot-silence and OT-link gates; with no
boiler reply satIsFlameOn() reads off, so a falling room counted as a heat-off period of the house.

The code under test is REAL: SATcontrol.ino, SATcycles.ino and SATpid.ino whole, through the shared loop harness
(tests/sat_loop_build.py, the driver of tests/test_sat_loop_integration.py), with one extra driver command `coeff` that prints the saved
coefficient and the running EMA. The room is above the target (no heat demand, the burner stays off) and falls 0.1 C every 5 min for
2 h, outdoor 8 C, the thermostat's room frame keeps coming while the boiler link is down: one sample per 5 min of 1.2 C/h / 17 K = 0.07, inside SAT_THERMAL_COEFF_MIN..MAX, and an hourly save.
  - OT link down from the start: SAT never gets past its gates, so the coefficient must not move (red on the code before the fix).
  - OT link up: SAT controls, the same fall is learned (positive control, green on both).
Run: python tests/test_sat_thermal_learning_gate.py (OTGW_SATCONTROL_INO=<path> runs it on another SATcontrol.ino)
"""
import os
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import sat_loop_build as B  # noqa: E402
from test_sat_loop_integration import Run  # noqa: E402

_TMP = tempfile.TemporaryDirectory()
_EXE = {}


def driver_with_coeff():
    d = B.rd(HERE / "sat_loop_driver.inc")
    old = "    else return 2;"
    assert d.count(old) == 1
    cmd = ('    else if (!strcmp(cmd, "coeff")) ev("COEFF", "%s saved=%.4f ema=%.4f", a, '
           'settings.sat.fThermalCoeff, _thermal_coeffEma);\n')
    d = d.replace(old, cmd + old)
    # The thermostat keeps sending the room temperature while the boiler is gone (the .63 case: room valid, no boiler reply). The
    # shared driver stops every frame on "link 0"; here the room frame alone keeps coming, every pass.
    run_old = "static void run(uint32_t untilRelMs) { while ((uint32_t)(g_ms - g_t0) < untilRelMs) loopOnce(); }"
    assert d.count(run_old) == 1
    return d.replace(run_old, "static void run(uint32_t untilRelMs) { while ((uint32_t)(g_ms - g_t0) < untilRelMs) { if (g_linkDown) tr(); "
                              "loopOnce(); } }")


def exe():
    if "x" in _EXE:
        return _EXE["x"]
    d = Path(_TMP.name) / "gate"
    d.mkdir()
    old = os.environ.get("OTGW_SATCONTROL_INO")   # another SATcontrol.ino for old-vs-fix
    overrides = {"SATcontrol.ino": Path(old).read_text(encoding="utf-8")} if old else None
    (d / "t.cpp").write_text(B.assemble(driver=driver_with_coeff(), overrides=overrides), encoding="utf-8")
    rc, out = B.cl_compile(d / "t.cpp", d / "t.exe")
    if rc is None:
        raise unittest.SkipTest("no host C++ compiler (MSVC) found")
    if rc != 0:
        raise RuntimeError("host compile failed:\n%s" % out[-3000:])
    _EXE["x"] = d / "t.exe"
    return _EXE["x"]


def script(link_up):
    s = ["link %d" % (1 if link_up else 0), "outside 8", "room 25", "run 1000", "coeff start"]
    room = 25.0
    t = 1000
    for _ in range(24):            # 24 x 5 min = 2 h, a 0.1 C fall per 5 min ("run" takes the time since the start)
        t += 300000
        s.append("run %d" % t)
        room -= 0.1
        s.append("room %.2f" % room)
    s += ["run %d" % (t + 300000), "coeff end", "snap end"]
    return s


def coeffs(r, tag):
    for t, x in r.kind("COEFF"):
        if x.startswith(tag + " "):
            return dict(p.split("=") for p in x.split()[1:])
    raise AssertionError("no COEFF %s" % tag)


class ThermalLearningGate(unittest.TestCase):
    def test_no_learning_while_the_ot_link_is_down(self):
        r = Run(exe(), script(link_up=False))
        a, b = coeffs(r, "start"), coeffs(r, "end")
        self.assertEqual(r.snap("end")[0]["otLost"], "1", "the scenario did not reach the OT-link-lost state")
        self.assertEqual(b["saved"], a["saved"], "saved coefficient moved without a boiler: %s -> %s" % (a["saved"], b["saved"]))
        self.assertEqual(b["ema"], a["ema"], "learned EMA moved without a boiler: %s -> %s" % (a["ema"], b["ema"]))

    def test_learning_still_runs_while_sat_controls(self):
        r = Run(exe(), script(link_up=True))
        a, b = coeffs(r, "start"), coeffs(r, "end")
        self.assertNotEqual(b["ema"], a["ema"], "the house's heat-off fall was not learned while SAT controls")
        self.assertNotEqual(b["saved"], a["saved"], "the hourly save did not run while SAT controls")


if __name__ == "__main__":
    unittest.main(verbosity=2)
