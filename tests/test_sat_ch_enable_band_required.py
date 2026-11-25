#!/usr/bin/env python3
"""TASK-3054: satChEnableDecide() has no default for bandOff, so a caller that forgets the tolerance-band flag fails to compile.

TASK-3034 found the diag input of satDiagEventsStep calling satChEnableDecide() with four arguments: bandOff silently became
false and a band pause counted as "CH asked". With the default gone the same mistake is a compile error.

Nothing is re-implemented: the test compiles the real SATchEnable.h twice, once with the five-argument call (must build and
run) and once with the four-argument call (must NOT build). On the code before the fix (default `bandOff = false`) the
four-argument call builds, so the "must not build" test goes red there.

Run: python tests/test_sat_ch_enable_band_required.py
"""
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_sat_pwm_override as base  # noqa: E402

CHENABLE = Path(base.REPO) / "src" / "OTGW-firmware" / "SATchEnable.h"
SRC_DIR = Path(base.REPO) / "src" / "OTGW-firmware"

MAIN = r"""
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "@CHENABLE@"
int main() {
  printf("%d\n", (int)satChEnableDecide(45.0f, 28.2f, false, false@ARGS@));
  return 0;
}
"""


def _build(workdir: Path, extra_args: str):
    cpp = workdir / "chen.cpp"
    cpp.write_text(MAIN.replace("@CHENABLE@", CHENABLE.as_posix()).replace("@ARGS@", extra_args), encoding="utf-8")
    return base.compile_cpp(cpp, workdir / "chen.exe")


class BandFlagRequired(unittest.TestCase):
    def test_signature_has_no_default_for_band_off(self):
        text = CHENABLE.read_text(encoding="utf-8")
        sig = text[text.index("static inline uint8_t satChEnableDecide("):]
        sig = sig[: sig.index(")")]
        self.assertNotIn("=", sig, "bandOff must have no default value:\n" + sig)

    def test_five_argument_call_builds_and_runs(self):
        with tempfile.TemporaryDirectory() as tmp:
            exe = _build(Path(tmp), ", false")
            if not exe:
                self.skipTest("no host C++ compiler (g++, clang++ or MSVC)")
            out = subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout.strip()
            self.assertEqual(out, "1")   # requested 45 above cold 28.2, PWM not driving, no band: CH on

    def test_four_argument_call_does_not_build(self):
        with tempfile.TemporaryDirectory() as tmp:
            try:
                exe = _build(Path(tmp), "")
            except (subprocess.CalledProcessError, AssertionError):
                return   # rejected at compile time: the wanted result
            if not exe:
                self.skipTest("no host C++ compiler (g++, clang++ or MSVC)")
            self.fail("a four-argument satChEnableDecide() call compiled: bandOff still has a default")

    def test_every_firmware_call_passes_five_arguments(self):
        calls = 0
        for path in sorted(SRC_DIR.glob("*.ino")):
            src = path.read_text(encoding="utf-8", errors="replace")
            for m in re.finditer(r"satChEnableDecide\(", src):
                depth, i, args, cur = 1, m.end(), 0, ""
                while depth and i < len(src):
                    c = src[i]
                    depth += (c == "(") - (c == ")")
                    if depth == 1 and c == ",":
                        args += 1
                    cur += c
                    i += 1
                self.assertEqual(args + 1, 5, f"{path.name}: satChEnableDecide({cur[:80]}... has {args + 1} arguments")
                calls += 1
        self.assertGreaterEqual(calls, 2)


if __name__ == "__main__":
    unittest.main()
