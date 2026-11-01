#!/usr/bin/env python3
"""TASK-1552: the S0, relay-output and Dallas pins never land on a pin the board drives.

The settings defaults came over from 1.x (S0 GPIO 12, output GPIO 16). On the OTGW Classic
with a LOLIN S3 Mini, 12 is the PIC reset and 16 is LED1; with an S3 Mini Pro, 12 is the I2C
SDA of the 0x26 watchdog and the OLED. initS0Count(), validateGPIOOutputsConfig() and
initSensors() now refuse such a pin via isBoardReservedPin().

Not re-implemented here: the test cuts the real `boardPinReserved()` block (between the
`[board-pin-reserved:begin]` / `:end` markers in OTGW-firmware.h) and compiles it with the real
src/libraries/Platform/src/boards.h for the combo build (BOARD_NODOSHOP_ESP32_COMBO), so the
pin numbers come from the pin tables the firmware uses. The MutantGate case drops the Pro I2C
SDA from the block and asserts the same check goes red. A source check asserts that the three
init paths call the guard before touching the pin.

OTGW_FIRMWARE_H points at another copy of the header (old-vs-fix runs). Without a host C++
compiler the compile cases are skipped, they do not pass silently.

Run: python tests/test_board_pin_reserved.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_sat_pwm_override as base  # noqa: E402  (compile_cpp: g++, clang++ or MSVC)

REPO = Path(__file__).resolve().parent.parent
SRC = REPO / "src" / "OTGW-firmware"
HDR = Path(os.environ.get("OTGW_FIRMWARE_H", SRC / "OTGW-firmware.h"))
BOARDS = REPO / "src" / "libraries" / "Platform" / "src" / "boards.h"
BEGIN = "// [board-pin-reserved:begin]"
END = "// [board-pin-reserved:end]"

# (map, pin, expected reserved)
CASES = [
    ("COMBO_MAP_CLASSIC", 12, True),       # S0 default = PIC reset on the S3 Mini
    ("COMBO_MAP_CLASSIC", 16, True),       # output default = LED1 on the S3 Mini
    ("COMBO_MAP_CLASSIC", 10, False),      # Dallas default stays usable
    ("COMBO_MAP_CLASSIC", 44, True),       # PIC UART RX
    ("COMBO_MAP_CLASSIC", 43, True),       # PIC UART TX
    ("COMBO_MAP_CLASSIC", 35, True),       # I2C SDA (watchdog, OLED)
    ("COMBO_MAP_CLASSIC", 47, True),       # S3 Mini status pixel
    ("COMBO_MAP_CLASSIC", 5, False),       # a free GPIO
    ("COMBO_MAP_CLASSIC_PRO", 12, True),   # S0 default = I2C SDA on the S3 Mini Pro
    ("COMBO_MAP_CLASSIC_PRO", 16, False),  # free on the Pro
    ("COMBO_MAP_CLASSIC_PRO", 40, True),   # Pro PIC reset
    ("COMBO_MAP_CLASSIC_PRO", 7, True),    # Pro pixel power
    ("COMBO_MAP_CLASSIC_PRO", 10, False),  # Dallas default stays usable
    ("COMBO_MAP_OTGW32", 4, False),        # the OT-Direct board's own 1-Wire pin stays usable
    ("COMBO_MAP_OTGW32", 21, True),        # OT master in
    ("COMBO_MAP_CLASSIC", 19, True),       # native USB D-
    ("COMBO_MAP_CLASSIC_PRO", 28, True),   # SPI flash / PSRAM
]

PRELUDE = r"""
#include <cstdio>
#include <cstdint>
#define BOARD_NODOSHOP_ESP32_COMBO
#include "boards.h"
enum ComboPinMap : unsigned char { COMBO_MAP_OTGW32 = 0, COMBO_MAP_CLASSIC = 1, COMBO_MAP_CLASSIC_PRO = 2 };
"""


def extract(text: str) -> str:
    i, j = text.find(BEGIN), text.find(END)
    if i < 0 or j < i:
        raise AssertionError("boardPinReserved() block not found in OTGW-firmware.h: the pin guard is missing")
    return text[i:j]


def build_and_run(block: str):
    main = "int main(){int bad=0;\n"
    for m, pin, exp in CASES:
        main += (f'  if (boardPinReserved({pin}, {m}) != {str(exp).lower()}) '
                 f'{{ std::printf("FAIL {m} GPIO{pin} expected {exp}\\n"); bad++; }}\n')
    main += "  return bad;}\n"
    with tempfile.TemporaryDirectory() as td:
        td = Path(td)
        cpp = td / "pin_reserved.cpp"
        cpp.write_text(PRELUDE + block.replace("inline bool", "static bool", 1) + "\n" + main, encoding="utf-8")
        # boards.h is found through the include path
        env_inc = str(BOARDS.parent)
        cpp.write_text(cpp.read_text(encoding="utf-8").replace('#include "boards.h"', f'#include "{(Path(env_inc) / "boards.h").as_posix()}"'), encoding="utf-8")
        exe = base.compile_cpp(cpp, td / "pin_reserved.exe")
        if exe is None:
            return None
        r = subprocess.run([str(exe)], capture_output=True, text=True)
        return r.returncode, r.stdout


class BoardPinReserved(unittest.TestCase):
    def test_board_pins_refused_free_pins_allowed(self):
        res = build_and_run(extract(HDR.read_text(encoding="utf-8")))
        if res is None:
            self.skipTest("no host C++ compiler")
        rc, out = res
        self.assertEqual(rc, 0, out)


class MutantGate(unittest.TestCase):
    def test_mutant_without_pro_sda_goes_red(self):
        block = extract(HDR.read_text(encoding="utf-8"))
        mutant = block.replace("pin == PIN_CLASSIC_PRO_I2C_SDA ||", "", 1)
        self.assertNotEqual(mutant, block, "mutation point not found")
        res = build_and_run(mutant)
        if res is None:
            self.skipTest("no host C++ compiler")
        rc, out = res
        self.assertNotEqual(rc, 0, "the check did not catch a guard without the Pro I2C SDA")
        self.assertIn("COMBO_MAP_CLASSIC_PRO GPIO12", out)


class CallersUseGuard(unittest.TestCase):
    def _body(self, path: Path, signature: str, upto: str) -> str:
        text = path.read_text(encoding="utf-8")
        i = text.find(signature)
        self.assertGreaterEqual(i, 0, f"{signature} not found in {path.name}")
        j = text.find(upto, i)
        self.assertGreater(j, i, f"{upto} not found after {signature} in {path.name}")
        return text[i:j]

    def test_s0_checks_before_pinmode(self):
        body = self._body(SRC / "s0PulseCount.ino", "void initS0Count()", "pinMode(settings.s0.iPin")
        self.assertIn("isBoardReservedPin(settings.s0.iPin)", body)

    def test_outputs_validation_checks_pin(self):
        body = self._body(SRC / "outputs_ext.ino", "static bool validateGPIOOutputsConfig()", "return true;")
        self.assertIn("isBoardReservedPin(settings.outputs.iPin)", body)

    def test_dallas_checks_before_onewire_begin(self):
        body = self._body(SRC / "sensors_ext.ino", "bSensorsDetected = false;", "oneWire.begin(settings.sensors.iPin)")
        self.assertIn("isBoardReservedPin(settings.sensors.iPin)", body)


if __name__ == "__main__":
    unittest.main(verbosity=2)
