#!/usr/bin/env python3
"""SAT rounding like thermo-nova (Sergeant D entries 482 and 490, which amend entry 95 Q28a): the tie helper.

The code under test is the REAL satRoundHalfEvenDecimals() of SATpid.ino, cut out of the file and compiled on the host.
The oracle is Python's round(x, n), which is what thermo-nova calls (pid.py:71, :82, :93, :132, summer_simmer.py:38):
it rounds the exact binary value of x, and an exact tie goes to the even neighbour.

The inputs and expected values below were generated with Python 3.12 (round(x, n)) and pasted in; the random sweep
compares the helper with round(x, n) for 20 000 values per decimal count, including values one ulp either side of a tie.

Run: python tests/test_sat_pid_rounding.py
"""
import os
import random
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
FW = Path(os.environ.get("OTGW_FW_DIR", REPO / "src" / "OTGW-firmware"))

MAIN = r"""
#include <cstdio>
#include <cstdlib>
#include <cmath>
using std::isfinite;
@CODE@
int main() {
  double v; int n;
  while (scanf("%lf %d", &v, &n) == 2) printf("%.17g\n", satRoundHalfEvenDecimals(v, n));
  return 0;
}
"""


def rounding_code(src: str) -> str:
    a = src.index("//=== PID rounding")
    b = src.index("//=== Restore PID internal state")
    return src[a:b]


# (value, decimals, python round(value, decimals)); generated with Python 3.12
TIES = [
    (45.25, 1, 45.2),       # exact tie (45.25 is a dyadic), even neighbour is 45.2
    (-45.25, 1, -45.2),
    (45.35, 1, 45.4),       # 45.35 is 45.35000000000000142, so up
    (2.675, 2, 2.67),       # 2.675 is 2.67499999999999982236431605997495353221893310546875, so down
    (1.005, 2, 1.0),        # 1.00499999999999989, down
    (0.125, 2, 0.12),       # exact tie, even neighbour 0.12
    (-0.125, 2, -0.12),
    (0.375, 2, 0.38),       # exact tie, even neighbour 0.38
    (0.0625, 3, 0.062),     # exact tie
    (0.5, 0, 0.0),
    (1.5, 0, 2.0),
    (2.5, 0, 2.0),
    (-2.5, 0, -2.0),
    (-0.5, 0, 0.0),         # python gives -0.0, equal to 0.0
    (0.0005, 3, 0.001),     # 0.0005 is 0.0005000000000000000104..., so up
    (22.0, 1, 22.0),
    (40.04999923706055, 1, 40.0),   # float32 40.05f widened to double
    (40.2599983215332, 1, 40.3),    # float32 40.26f
]


class RoundHalfEvenDecimals(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        src = (FW / "SATpid.ino").read_text(encoding="utf-8").replace("\r\n", "\n")
        code = rounding_code(src)
        assert "satRoundHalfEvenDecimals" in code, "SATpid.ino has no satRoundHalfEvenDecimals() in its rounding block"
        # satPidRound3 may call the helper; give the block the one stub it needs and nothing else
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        cpp = d / "r.cpp"
        cpp.write_text(MAIN.replace("@CODE@", code), encoding="utf-8")
        from test_sat_pwm_override import compile_cpp
        cls.exe = compile_cpp(cpp, d / "r.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def run_pairs(self, pairs):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        text = "\n".join("%s %d" % (repr(v), n) for v, n in pairs) + "\n"
        out = subprocess.run([str(self.exe)], input=text, capture_output=True, text=True, check=True).stdout.split()
        self.assertEqual(len(out), len(pairs))
        return [float(x) for x in out]

    def test_named_ties_like_python_round(self):
        got = self.run_pairs([(v, n) for v, n, _ in TIES])
        for (v, n, want), g in zip(TIES, got):
            with self.subTest(v=v, n=n):
                self.assertEqual(want, round(v, n), "the pasted expectation is what python gives")
                self.assertEqual(g, want, "round(%r, %d)" % (v, n))

    def test_random_sweep_equals_python_round(self):
        rnd = random.Random(42)
        pairs = []
        for n in (1, 3, 6):
            scale = 10.0 ** n
            for _ in range(5000):
                pairs.append((rnd.uniform(-100.0, 100.0), n))
            for _ in range(5000):                      # near a decimal tie: (k + 0.5) / 10^n and its neighbours
                k = rnd.randint(-100000, 100000)
                v = (k + 0.5) / scale
                pairs.append((v, n))
            for _ in range(5000):                      # one ulp each side of that value
                k = rnd.randint(-100000, 100000)
                v = (k + 0.5) / scale
                pairs.append((v + abs(v) * 2.3e-16, n))
                pairs.append((v - abs(v) * 2.3e-16, n))
            for _ in range(2500):                      # exact dyadic ties: m / 2^j with the tie at the n-th decimal
                j = rnd.randint(1, 12)
                m = rnd.randint(-5000, 5000)
                pairs.append((m / float(1 << j), n))
        got = self.run_pairs(pairs)
        bad = [(p, g, round(p[0], p[1])) for p, g in zip(pairs, got) if g != round(p[0], p[1])]
        self.assertEqual(bad[:10], [], "%d of %d differ from python round()" % (len(bad), len(pairs)))

    def test_non_finite_and_huge_pass_through(self):
        inf, nan = float("inf"), float("nan")
        got = self.run_pairs([(inf, 3), (-inf, 3), (1e300, 3), (2.0 ** 60, 1)])
        self.assertEqual(got, [inf, -inf, 1e300, 2.0 ** 60])
        g = self.run_pairs([(nan, 3)])[0]
        self.assertTrue(g != g)


class SourceShape(unittest.TestCase):
    def test_no_roundf_left_in_the_pid_rounding_block(self):
        src = (FW / "SATpid.ino").read_text(encoding="utf-8").replace("\r\n", "\n")
        code = re.sub(r"//.*", "", rounding_code(src))
        self.assertNotIn("roundf", code, "satPidRound3() must use the half-to-even helper")


if __name__ == "__main__":
    unittest.main(verbosity=2)
