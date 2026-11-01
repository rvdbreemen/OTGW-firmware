"""Source check for TASK-1547 (cold cutoff hysteresis, Sergeant D entry 199).

The latched rule only works when every place that compares the requested setpoint with the cold setpoint
uses the latched threshold (satColdThreshold()), and the latch is updated once per control tick before
those users. A raw "pidOutput <= satGetColdSetpoint()" left anywhere would see a different state than the
CH bit and could send a CS while CH is off (or the other way round).

Fails on the code before TASK-1547 (raw comparisons, no latch); passes after.
Run: python tests/test_sat_cold_hysteresis_source.py   (or with -m unittest)
"""
import os
import re
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "src", "OTGW-firmware", "SATcontrol.ino")


def _read():
    with open(SRC, encoding="utf-8") as f:
        return f.read()


class ColdHysteresisSource(unittest.TestCase):
    def test_no_raw_requested_vs_cold_comparison(self):
        src = _read()
        raw = re.findall(r"(?:pidOutput|_sat_requestedSetpoint)\s*(?:<=|>)\s*satGetColdSetpoint\(\)", src)
        self.assertEqual(raw, [], "raw requested-vs-cold comparisons left: %r" % raw)

    def test_ch_decision_uses_threshold(self):
        self.assertIn("satChEnableDecide(pidOutput, satColdThreshold(),", _read())

    def test_latch_updated_once_before_users(self):
        src = _read()
        upd = [m.start() for m in re.finditer(r"_sat_coldCut = satColdCutoffLatched\(", src)]
        self.assertEqual(len(upd), 1, "the latch must be updated exactly once (control tick)")
        # Inside the tick, the overshoot-switch gate and the cutoff block come after the update.
        gate = src.find("if (pidOutput > satColdThreshold()) {\n    satCycleCheckOvershootSwitch")
        if gate < 0:
            gate = src.find("if (pidOutput > satColdThreshold()) {\r\n    satCycleCheckOvershootSwitch")
        cut = src.find("const bool coldCutoff = _sat_coldCut;")
        self.assertGreater(gate, upd[0])
        self.assertGreater(cut, upd[0])

    def test_band_is_half_degree(self):
        self.assertRegex(_read(), r"SAT_COLD_HYSTERESIS_C\s*=\s*0\.5f")


if __name__ == "__main__":
    unittest.main()
