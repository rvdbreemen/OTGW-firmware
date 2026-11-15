#!/usr/bin/env python3
"""TASK-1971: the cloud recorder's backoff schedule and status classification (src/OTGW-firmware/CloudRecPolicy.h).

The real header is compiled into a small C++ program; nothing is re-implemented here.
  1. Without jitter the waits are 5, 10, 20, ... min, capped at 24 h, and stay at 24 h.
  2. With any random value a wait is 100 to 120 % of its base and never above 24 h; the first wait is 5 to 6 min.
  3. HTTP statuses map to OK / REFUSED / AUTH / DOWN.
"""
import os, shutil, subprocess, tempfile, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = r'''
#include <cstdio>
#include "@HDR@"
int main() {
  const unsigned rnds[] = {0u, 1u, 2000u, 2001u, 4294967295u, 123456789u};
  for (int s = 0; s < 16; s++) for (unsigned r : rnds) printf("B %d %u %u\n", s, r, crec::backoffMs((uint8_t)s, r));
  const int st[] = {0, 200, 400, 401, 403, 404, 429, 500, 502, 503};
  for (int x : st) printf("C %d %d\n", x, (int)crec::classify(x));
}
'''.replace("@HDR@", os.path.join(ROOT, "src", "OTGW-firmware", "CloudRecPolicy.h").replace("\\", "/"))

MIN = 60000
H24 = 24 * 60 * MIN


def run():
    cxx = shutil.which("g++") or shutil.which("c++") or shutil.which("clang++")
    if not cxx:
        return None
    d = tempfile.mkdtemp(prefix="crpol-")
    src, exe = os.path.join(d, "p.cpp"), os.path.join(d, "p")
    open(src, "w").write(SRC)
    subprocess.run([cxx, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-o", exe, src], check=True)
    return subprocess.run([exe], capture_output=True, text=True, check=True).stdout.split("\n")


OUT = run()


@unittest.skipIf(OUT is None, "no C++ compiler")
class CloudRecPolicyTest(unittest.TestCase):
    def rows(self, k):
        return [list(map(int, l.split()[1:])) for l in OUT if l.startswith(k + " ")]

    def test_schedule_without_jitter(self):
        got = [w for s, r, w in self.rows("B") if r == 0]
        want = [min(5 * MIN << s, H24) for s in range(16)]
        self.assertEqual(got, want)
        self.assertEqual(got[:10], [5 * MIN, 10 * MIN, 20 * MIN, 40 * MIN, 80 * MIN, 160 * MIN, 320 * MIN, 640 * MIN,
                                    1280 * MIN, H24])

    def test_jitter_bounds(self):
        for s, r, w in self.rows("B"):
            base = min(5 * MIN << s, H24)
            self.assertGreaterEqual(w, base)
            self.assertLessEqual(w, min(base * 12 // 10, H24))
        first = [w for s, r, w in self.rows("B") if s == 0]
        self.assertTrue(all(5 * MIN <= w <= 6 * MIN for w in first))
        self.assertEqual(max(w for s, r, w in self.rows("B") if s == 0), 6 * MIN)    # r % 2001 == 2000

    def test_classify(self):
        got = {s: o for s, o in self.rows("C")}
        self.assertEqual(got, {0: 3, 200: 0, 400: 1, 401: 2, 403: 2, 404: 3, 429: 3, 500: 3, 502: 3, 503: 3})


if __name__ == "__main__":
    unittest.main(verbosity=2)
