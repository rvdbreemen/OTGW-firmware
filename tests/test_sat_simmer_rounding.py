#!/usr/bin/env python3
"""The Summer Simmer index is rounded to 1 decimal on every return path (Sergeant D entry 490, which settles entry 489).

thermo-nova summer_simmer.py:31-38 builds the index in Fahrenheit and returns round(convert(index, F -> C), 1) at the one
return; the below-58 F branch (:34-35) and every other path go through that same line. The firmware's satCalcSimmerIndex()
had three returns and no rounding at all.

The code under test is the REAL satCalcSimmerIndex() cut out of SATcontrol.ino plus the real rounding block of SATpid.ino
(satRoundHalfEvenDecimals), compiled on the host. The oracle is thermo-nova's function transcribed in Python, fed the float32
inputs the firmware sees. The inputs and expected values listed in CASES were generated with that oracle and Python round().

OTGW_SAT_INO / OTGW_SATPID_INO point at other copies (`git show HEAD:...`) for the old-vs-fix run.

Run: python tests/test_sat_simmer_rounding.py
"""
import os
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
SAT_INO = Path(os.environ.get("OTGW_SAT_INO", REPO / "src" / "OTGW-firmware" / "SATcontrol.ino"))
PID_INO = Path(os.environ.get("OTGW_SATPID_INO", REPO / "src" / "OTGW-firmware" / "SATpid.ino"))

MAIN = r"""
#include <cstdio>
#include <cstdlib>
#include <cmath>
using std::isfinite;
@PID@
@SIMMER@
int main() {
  float t, h;
  while (scanf("%f %f", &t, &h) == 2) printf("%.9g\n", satCalcSimmerIndex(t, h));
  return 0;
}
"""


def f32(x):
    return struct.unpack("f", struct.pack("f", x))[0]


def oracle(temp_c, humidity):
    """summer_simmer.py:22-38 at 7beaaf5. The firmware also returns the temperature for a humidity outside 0..100 (the
    Python code has no such check); that path is rounded like the others."""
    if humidity < 0 or humidity > 100:
        return round(temp_c, 1)
    f = temp_c * 1.8 + 32.0
    index = 1.98 * (f - (0.55 - 0.0055 * humidity) * (f - 58.0)) - 56.83
    if f < 58:
        index = f
    return round((index - 32.0) / 1.8, 1)


# (tempC, humidity, python result), float32 inputs; generated with oracle() above
CASES = [
    (22.0, 70.0, 26.9),      # the case the old code comment got wrong ("feels like 23C"): the formula gives 26.94 -> 26.9
    (20.0, 50.0, 22.4),      # above 58 F, mid humidity
    (10.0, 50.0, 10.0),      # below 58 F (50 F): the temperature itself
    (14.3, 40.0, 14.3),      # below 58 F, a value that is not exactly a decimal in float32
    (13.04, 55.0, 13.0),     # a raw 2-decimal temperature, below 58 F (55.5 F)
    (22.0, 120.0, 22.0),     # humidity out of range: temperature, 1 decimal
    (22.04, -5.0, 22.0),
    (23.46, 101.0, 23.5),
]


class SimmerRounding(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        sat = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        pid = PID_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        a = sat.index("static float satCalcSimmerIndex(float tempC, float humidity) {")
        simmer = sat[a:sat.index("\n}\n", a) + 3]
        r = pid.index("//=== PID rounding")
        rounding = pid[r:pid.index("//=== Restore PID internal state")]
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        cpp = d / "s.cpp"
        cpp.write_text(MAIN.replace("@PID@", rounding).replace("@SIMMER@", simmer), encoding="utf-8")
        from test_sat_pwm_override import compile_cpp
        cls.exe = compile_cpp(cpp, d / "s.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def run_pairs(self, pairs):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        text = "\n".join("%.9g %.9g" % (t, h) for t, h in pairs) + "\n"
        out = subprocess.run([str(self.exe)], input=text, capture_output=True, text=True, check=True).stdout.split()
        self.assertEqual(len(out), len(pairs))
        return [f32(float(x)) for x in out]   # %.9g of a float32 round-trips

    def test_listed_cases(self):
        got = self.run_pairs([(t, h) for t, h, _ in CASES])
        for (t, h, want), g in zip(CASES, got):
            with self.subTest(t=t, h=h):
                self.assertEqual(oracle(f32(t), f32(h)), want, "the pasted expectation is what the oracle gives")
                self.assertEqual(f32(want), g, "satCalcSimmerIndex(%r, %r) = %r, want %r" % (t, h, g, want))

    def test_every_return_path_has_one_decimal(self):
        pairs = [(f32(t / 10.0), f32(h)) for t in range(-50, 400, 7) for h in (-10, 0, 35, 70, 100, 130)]
        got = self.run_pairs(pairs)
        bad = [(p, g) for p, g in zip(pairs, got) if abs(g * 10.0 - round(g * 10.0)) > 1e-3]
        self.assertEqual(bad[:8], [], "%d of %d results are not a 1-decimal value" % (len(bad), len(pairs)))

    def test_sweep_equals_the_oracle_away_from_ties(self):
        pairs = [(f32(t / 10.0), f32(h)) for t in range(-100, 450) for h in range(0, 101, 5)]
        got = self.run_pairs(pairs)
        bad, skipped = [], 0
        for (t, h), g in zip(pairs, got):
            f = t * 1.8 + 32.0
            index = f if f < 58 else 1.98 * (f - (0.55 - 0.0055 * h) * (f - 58.0)) - 56.83
            raw = (index - 32.0) / 1.8
            if abs((raw * 10.0) % 1.0 - 0.5) < 2e-3:
                skipped += 1      # within 2e-4 C of a tie: float32 against float64 may fall either side
                continue
            if f32(oracle(t, h)) != g:
                bad.append(((t, h), g, oracle(t, h)))
        print("\n  sweep: %d points, %d near a tie skipped, %d differ" % (len(pairs), skipped, len(bad)))
        self.assertEqual(bad[:8], [])


class SourceShape(unittest.TestCase):
    def test_the_comment_does_not_claim_23c(self):
        sat = SAT_INO.read_text(encoding="utf-8")
        self.assertNotIn('"feels like" 23C', sat)
        self.assertIn('"feels like" 26.9C', sat)


if __name__ == "__main__":
    unittest.main(verbosity=2)
