#!/usr/bin/env python3
"""TASK-1402: the forced OT-Direct (iBoardMode 2) recovery probe resets the PIC on the Pro's pin.

Mode 2 resolves the OTGW32 pin map, whose PIC reset is GPIO 12 (the I2C SDA on an S3 Mini Pro).
picRstPinFor() now returns GPIO 40 for mode 2 when the IMU probe found a Pro.

Not re-implemented: the test cuts the real block between the `[pic-rst-pin:begin]` / `:end`
markers in OTGW-firmware.h and compiles it with the real boards.h (combo build).
OTGW_FIRMWARE_H points at another header copy (old-vs-fix runs); with the old header the
block is absent and the test fails red. Without a host C++ compiler the compile case is skipped.

Run: python tests/test_pic_rst_pin.py
"""
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_sat_pwm_override as base  # noqa: E402

REPO = Path(__file__).resolve().parent.parent
SRC = REPO / "src" / "OTGW-firmware"
HDR = Path(os.environ.get("OTGW_FIRMWARE_H", SRC / "OTGW-firmware.h"))
BOARDS = REPO / "src" / "libraries" / "Platform" / "src" / "boards.h"
BEGIN = "// [pic-rst-pin:begin]"
END = "// [pic-rst-pin:end]"

# (label, map, boardMode, classicPro, expected macro)
CASES = [
    ("mode2 Pro",        "COMBO_MAP_OTGW32",      2, True,  "PIN_CLASSIC_PRO_PIC_RST"),  # the fix: 40
    ("mode2 Mini",       "COMBO_MAP_OTGW32",      2, False, "PIN_PIC_RST"),              # 12
    ("mode2 OTGW32",     "COMBO_MAP_OTGW32",      2, False, "PIN_PIC_RST"),
    ("auto Pro",         "COMBO_MAP_CLASSIC_PRO", 0, True,  "PIN_CLASSIC_PRO_PIC_RST"),
    ("auto Mini",        "COMBO_MAP_CLASSIC",     0, False, "PIN_PIC_RST"),
    ("mode1 Mini",       "COMBO_MAP_CLASSIC",     1, False, "PIN_PIC_RST"),
    ("mode1 Pro flag",   "COMBO_MAP_CLASSIC",     1, True,  "PIN_PIC_RST"),              # forced Mini unchanged
    ("mode3 Pro",        "COMBO_MAP_CLASSIC_PRO", 3, True,  "PIN_CLASSIC_PRO_PIC_RST"),
    ("auto OTDirect",    "COMBO_MAP_OTGW32",      0, False, "PIN_PIC_RST"),
]

PRELUDE = r"""
#include <cstdio>
#include <cstdint>
#define BOARD_NODOSHOP_ESP32_COMBO
#include "%s"
enum ComboPinMap : unsigned char { COMBO_MAP_OTGW32 = 0, COMBO_MAP_CLASSIC = 1, COMBO_MAP_CLASSIC_PRO = 2 };
"""


def extract(text: str) -> str:
    i, j = text.find(BEGIN), text.find(END)
    if i < 0 or j < i:
        raise AssertionError("picRstPinFor() block not found in OTGW-firmware.h: forced mode 2 still resolves GPIO 12 on a Pro")
    return text[i:j]


def run(block: str):
    main = "int main(){int bad=0;\n"
    for label, m, mode, pro, exp in CASES:
        main += (f'  {{int g = picRstPinFor({m}, {mode}, {str(pro).lower()}); '
                 f'std::printf("{label}: GPIO %d\\n", g); '
                 f'if (g != {exp}) {{ std::printf("FAIL {label} expected %d\\n", {exp}); bad++; }} }}\n')
    main += "  return bad;}\n"
    with tempfile.TemporaryDirectory() as td:
        td = Path(td)
        cpp = td / "pic_rst.cpp"
        cpp.write_text(PRELUDE % BOARDS.as_posix() + block + "\n" + main, encoding="utf-8")
        exe = base.compile_cpp(cpp, td / "pic_rst.exe")
        if exe is None:
            return None
        r = subprocess.run([str(exe)], capture_output=True, text=True)
        return r.returncode, r.stdout


class PicRstPin(unittest.TestCase):
    def test_pin_per_mode(self):
        res = run(extract(HDR.read_text(encoding="utf-8")))
        if res is None:
            self.skipTest("no host C++ compiler")
        rc, out = res
        print(out)
        self.assertEqual(rc, 0, out)

    def test_mutant_without_mode2_clause_goes_red(self):
        block = extract(HDR.read_text(encoding="utf-8"))
        mutant = block.replace(" || (boardMode == 2 && classicPro)", "", 1)
        self.assertNotEqual(mutant, block, "mutation point not found")
        res = run(mutant)
        if res is None:
            self.skipTest("no host C++ compiler")
        self.assertNotEqual(res[0], 0)
        self.assertIn("FAIL mode2 Pro", res[1])

    def test_activePicRst_and_recovery_use_it(self):
        text = HDR.read_text(encoding="utf-8")
        i = text.find("inline int activePicRst()")
        self.assertGreater(i, 0)
        self.assertIn("picRstPinFor(comboActivePinMap(), settings.iBoardMode, state.hw.bClassicPro)", text[i:i + 200])
        ino = (SRC / "OTGW-firmware.ino").read_text(encoding="utf-8")
        k = ino.find("if (settings.iBoardMode == 2) {")
        self.assertGreater(k, 0)
        self.assertIn("OTGWSerial.setResetPin(activePicRst())", ino[k:k + 2500])


if __name__ == "__main__":
    unittest.main()
