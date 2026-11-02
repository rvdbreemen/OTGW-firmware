"""TASK-1547 AC#3: mild-day simulation of the SAT cold cutoff, old rule against the 0.5 C latched band.

tests/sim_sat_cold_hysteresis_day.cpp includes the real src/OTGW-firmware/SATchEnable.h and runs one day of a
radiator house whose requested setpoint (curve + PI on a 0.1 C sensor) hovers on the 28.2 C cold setpoint.
Band 0 uses the old bare satColdCutoff(), band 0.5 the latched satColdCutoffLatched(). The house, radiators,
boiler and PI are a simple model in that file, not the firmware PID: the test proves the latch's effect on
starts in a hovering situation, not the field numbers.

Pass: with the band, burner starts fall by at least half on every profile, delivered heat stays within 2 %,
the mean room temperature within 0.05 C, and no minute is spent more than 0.3 C below target.
"""
import re
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_sat_curve_selftune as cc   # noqa: E402  (shared MSVC/g++ compile helper)

SIM = Path(__file__).resolve().parent / "sim_sat_cold_hysteresis_day.cpp"
PROFILES = [(8, 3), (10, 3), (11, 3), (12, 3), (13, 2)]
SEEDS = [1, 2, 3, 4, 5]


def run(exe, band, mean, amp, seed):
    import subprocess
    out = subprocess.run([str(exe), str(band), str(mean), str(amp), str(seed)],
                         capture_output=True, text=True, check=True).stdout
    return {k: float(v) for k, v in re.findall(r"(\w+)=([-\d.]+)", out.strip().splitlines()[-1])}


class ColdHysteresisDay(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = cc.compile_cpp(SIM, Path(cls.tmp.name) / "sim.exe")
        if cls.exe is None:
            raise unittest.SkipTest("no C++ compiler")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()
        for obj in SIM.parent.glob("sim_sat_cold_hysteresis_day*.obj"):
            obj.unlink()

    def test_band_cuts_starts_without_losing_heat(self):
        for mean, amp in PROFILES:
            old = [run(self.exe, 0, mean, amp, s) for s in SEEDS]
            new = [run(self.exe, 0.5, mean, amp, s) for s in SEEDS]
            avg = lambda rows, k: sum(r[k] for r in rows) / len(rows)
            with self.subTest(outside=f"{mean}+-{amp}"):
                self.assertLessEqual(avg(new, "starts"), 0.5 * avg(old, "starts"))
                self.assertAlmostEqual(avg(new, "heat_kWh"), avg(old, "heat_kWh"), delta=0.02 * avg(old, "heat_kWh"))
                self.assertAlmostEqual(avg(new, "room_mean"), avg(old, "room_mean"), delta=0.05)
                self.assertEqual(sum(r["below_min"] for r in new), 0)


if __name__ == "__main__":
    unittest.main()
