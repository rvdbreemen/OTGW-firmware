#!/usr/bin/env python3
"""TASK-1338: host test for SATcycles.ino _tailPercentile() with more than 127 samples.
TASK-1586 (entry 248): the percentile is interpolated between the two neighbouring sorted values; the expected
values come from the REAL thermo-nova helpers.percentile_interpolated (other-projects, pinned 7beaaf5, loaded through
test_sat_classifier_parity.load_upstream()), not from a re-implementation.

The real function and the real tail ring globals are cut out of SATcycles.ino with a regex
and compiled with a host compiler (g++, clang++ or MSVC cl via vswhere). Nothing is
re-implemented. SAT_TAIL_SAMPLE_SIZE is 180 on ESP32 (boards.h).

Before the fix the insertion-sort index was an int8_t: from the 129th element on it wraps
negative/positive, the sort writes outside `sorted[]` or never terminates. The test fails
that in three observable ways: a timeout (hang), a wrong percentile compared with a reference
computed from the same data, and, when the compiler supports it, an AddressSanitizer abort.

OTGW_SATCYCLES_INO points at another copy (e.g. `git show HEAD:src/OTGW-firmware/SATcycles.ino`)
for old-vs-fix runs. Without a compiler the tests are skipped.

Run: python tests/test_sat_tail_percentile.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import _vcvars  # noqa: E402  (shared MSVC discovery)
import shutil  # noqa: E402

REPO = Path(__file__).resolve().parent.parent
CYCLES_INO = Path(os.environ.get("OTGW_SATCYCLES_INO", REPO / "src" / "OTGW-firmware" / "SATcycles.ino"))
SIZE = 180

HARNESS_HEAD = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define SAT_TAIL_SAMPLE_SIZE 180
"""

HARNESS_MAIN = r"""
int main(int argc, char** argv) {   // argv: head count v0 v1 ... (count values, oldest first)
  int head = atoi(argv[1]), count = atoi(argv[2]);
  _tail_sampleHead = (uint8_t)head; _tail_sampleCount = (uint8_t)count;
  for (int i = 0; i < count; i++) {
    int slot = (head + SAT_TAIL_SAMPLE_SIZE - count + i) % SAT_TAIL_SAMPLE_SIZE;
    _tail_samples[slot] = (float)atof(argv[3 + i]);
  }
  printf("%.3f %.3f %.3f\n", _tailPercentile(10), _tailPercentile(50), _tailPercentile(90));
  return 0;
}
"""


def extract(src: str) -> str:
    g = re.search(r"^static float\s+_tail_samples\[[^\n]*\n^static uint8_t\s+_tail_sampleHead[^\n]*\n^static uint8_t\s+_tail_sampleCount[^\n]*\n",
                  src, re.M)
    h = re.search(r"^static void _percentilePos\(.*?^\}\s*$", src, re.S | re.M)
    f = re.search(r"^static float _tailPercentile\(uint8_t pct\)\s*\{.*?^\}\s*$", src, re.S | re.M)
    if not g or not f:
        raise AssertionError("tail ring globals or _tailPercentile() not found in SATcycles.ino")
    # _percentilePos() (TASK-1586) is part of the unit under test when the copy has it; the old copy has none
    return g.group(0) + "\n" + (h.group(0) + "\n" if h else "") + f.group(0) + "\n"


def compile_cpp(cpp: Path, exe: Path, asan: bool):
    for cc in ("g++", "clang++"):
        if shutil.which(cc):
            cmd = [cc, "-std=c++17"] + (["-fsanitize=address", "-g"] if asan else []) + ["-o", str(exe), str(cpp)]
            r = subprocess.run(cmd, capture_output=True, text=True)
            return exe if r.returncode == 0 else None
    bat = _vcvars()
    if bat:
        flags = "/fsanitize=address /Zi " if asan else ""
        cmd = f'call "{bat}" >nul && cl /nologo /std:c++17 /EHsc {flags}/Fe:"{exe}" /Fo:"{cpp.parent}\\\\" "{cpp}"'
        r = subprocess.run(cmd, shell=True, cwd=cpp.parent, capture_output=True, text=True)
        return exe if r.returncode == 0 else None
    return None


def reference(vals, pct):
    """The real thermo-nova percentile_interpolated (helpers.py:185-202) on the same samples."""
    import test_sat_classifier_parity as par
    return par.load_upstream().helpers.percentile_interpolated(vals, pct / 100.0)


class TailPercentile(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        wd = Path(cls.tmp.name)
        cpp = wd / "t.cpp"
        cpp.write_text(HARNESS_HEAD + extract(CYCLES_INO.read_text(encoding="utf-8")) + HARNESS_MAIN, encoding="utf-8")
        cls.exe = compile_cpp(cpp, wd / "t.exe", asan=False)
        asan_cpp = wd / "a" / "t.cpp"
        asan_cpp.parent.mkdir()
        asan_cpp.write_text(cpp.read_text(encoding="utf-8"), encoding="utf-8")
        cls.asan_exe = compile_cpp(asan_cpp, asan_cpp.parent / "t.exe", asan=True)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_exe(self, exe, head, vals):
        try:
            r = subprocess.run([str(exe), str(head), str(len(vals))] + [repr(v) for v in vals],
                               capture_output=True, text=True, timeout=10)
        except subprocess.TimeoutExpired:
            self.fail("_tailPercentile() did not terminate (sort index wrapped)")
        self.assertEqual(r.returncode, 0, f"abnormal exit {r.returncode}: {r.stderr[-400:]}")
        return [float(x) for x in r.stdout.split()]

    def check(self, vals, head=0, exe=None):
        got = self.run_exe(exe or self.exe, head, vals)
        want = [float(reference(vals, p)) for p in (10, 50, 90)]
        self.assertEqual(len(got), 3)
        for g_, w_, p in zip(got, want, (10, 50, 90)):
            self.assertAlmostEqual(g_, w_, places=2, msg=f"p{p} n={len(vals)} head={head}")   # printed with %.3f; float32 lerp

    def setUp(self):
        if self.exe is None:
            self.skipTest("no host C++ compiler (g++, clang++ or MSVC)")
        import test_sat_classifier_parity as par
        try:
            par.load_upstream()
        except par.UpstreamUnavailable as e:
            self.skipTest("thermo-nova upstream not runnable: %s" % e)

    def test_descending_full_ring(self):          # worst case for insertion sort
        self.check([float(SIZE - i) for i in range(SIZE)])

    def test_descending_just_over_127(self):
        self.check([float(129 - i) for i in range(129)])

    def test_ascending_full_ring(self):
        self.check([float(i) for i in range(SIZE)])

    def test_wrapped_ring_pseudo_random(self):
        vals = [float((i * 37) % 101) + 0.5 for i in range(SIZE)]
        self.check(vals, head=77)

    def test_interpolates_between_the_two_neighbours(self):
        # 180 values 0..179: p10 position 17.9 -> 17.9, p50 89.5, p90 161.1 (floor would give 17, 89, 161)
        got = self.run_exe(self.exe, 0, [float(i) for i in range(SIZE)])
        self.assertEqual([round(x, 2) for x in got], [17.9, 89.5, 161.1])

    def test_single_and_two_samples(self):
        self.assertEqual(self.run_exe(self.exe, 1, [7.5]), [7.5, 7.5, 7.5])          # n == 1 returns the value
        got = self.run_exe(self.exe, 2, [10.0, 20.0])                                 # n == 2: lerp over the pair
        self.assertEqual([round(x, 3) for x in got], [11.0, 15.0, 19.0])

    def test_equal_samples_stay_exact(self):
        self.assertEqual(self.run_exe(self.exe, 0, [3.25] * 50), [3.25, 3.25, 3.25])

    def test_small_counts_match_the_reference(self):
        for n in (10, 64, 127):
            self.check([float(n - i) for i in range(n)], head=5)

    def test_addresssanitizer_clean(self):
        if self.asan_exe is None:
            self.skipTest("compiler has no AddressSanitizer support")
        probe = subprocess.run([str(self.asan_exe), "0", "1", "1.0"], capture_output=True, text=True, timeout=10)
        if probe.returncode != 0:
            self.skipTest(f"AddressSanitizer binary does not start on this host (exit {probe.returncode})")
        self.check([float(SIZE - i) for i in range(SIZE)], exe=self.asan_exe)


if __name__ == "__main__":
    unittest.main(verbosity=2)
