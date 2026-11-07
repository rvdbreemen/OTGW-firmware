#!/usr/bin/env python3
"""TASK-1686: the sensor, S0 and output pin settings accept every ESP32-S3 GPIO (0-21, 26-48).

The 0-16 limit came from the ESP8266. On an OTGW Classic with a LOLIN S3 Mini Pro the free holes
D6/D7/D8 are GPIO 39/38/37, so a Pro user could not put the 1-Wire bus on a free hole.

Not re-implemented here: the test cuts the REAL setter branches for GPIOSENSORSpin, S0COUNTERpin
and GPIOOUTPUTSpin out of settingStuff.ino (brace matching), the REAL settingGpioInRange() block
([setting-gpio-range:begin/end]) and the REAL boardPinReserved() block from OTGW-firmware.h, and
compiles them with the real Platform boards.h for the combo build. Only the logging, the conflict
check and the settings struct are stubbed. It then calls the setters the way a REST POST does.

OTGW_SETTINGSTUFF_INO / OTGW_FIRMWARE_H point at other copies (old-vs-fix runs). On the old
sources the setters refuse GPIO 37-39, so the Pro case goes red. Without a host C++ compiler the
compile cases are skipped, they do not pass silently.

Run: python tests/test_setting_gpio_range.py
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
INO = Path(os.environ.get("OTGW_SETTINGSTUFF_INO", SRC / "settingStuff.ino"))
HDR = Path(os.environ.get("OTGW_FIRMWARE_H", SRC / "OTGW-firmware.h"))
REST = SRC / "restAPI.ino"
BOARDS = REPO / "src" / "libraries" / "Platform" / "src" / "boards.h"
FIELDS = ("GPIOSENSORSpin", "S0COUNTERpin", "GPIOOUTPUTSpin")


def block(text, begin, end):
    i, j = text.find(begin), text.find(end)
    return text[i:j] if i >= 0 and j > i else ""


def setter_branch(text, field):
    """The `else if (strcasecmp_P(field, PSTR("<field>")) == 0) { ... }` branch, braces matched."""
    m = re.search(r'else if \(strcasecmp_P\(field, PSTR\("' + field + r'"\)\) == 0\)\s*\{', text)
    if not m:
        raise AssertionError(f"setter branch for {field} not found in {INO.name}")
    i, depth = m.end() - 1, 0
    for j in range(i, len(text)):
        if text[j] == "{":
            depth += 1
        elif text[j] == "}":
            depth -= 1
            if depth == 0:
                return text[m.start() + len("else "):j + 1]
    raise AssertionError(f"unbalanced braces in the {field} branch")


PRELUDE = r"""
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cctype>
#define BOARD_NODOSHOP_ESP32_COMBO
#include "BOARDS_H"
enum ComboPinMap : unsigned char { COMBO_MAP_OTGW32 = 0, COMBO_MAP_CLASSIC = 1, COMBO_MAP_CLASSIC_PRO = 2 };
#define PSTR(s) (s)
// strcasecmp_P on the device; a portable case-insensitive compare here (MSVC has no strings.h)
static int strcasecmp_P(const char* a, const char* b) {
  while (*a && *b && std::tolower((unsigned char)*a) == std::tolower((unsigned char)*b)) { a++; b++; }
  return std::tolower((unsigned char)*a) - std::tolower((unsigned char)*b);
}
#define DebugTf(...) do {} while (0)
#define Debugln(...) do {} while (0)
enum class GPIOConflictCaller { Sensor, S0, Output };
static bool checkGPIOConflict(int, GPIOConflictCaller) { return false; }
struct { struct { int iPin = 10; } sensors; struct { int iPin = 12; } s0; struct { int iPin = 16; } outputs; } settings;
"""


def build_and_run(cases):
    ino, hdr = INO.read_text(encoding="utf-8"), HDR.read_text(encoding="utf-8")
    rng = block(hdr, "// [setting-gpio-range:begin]", "// [setting-gpio-range:end]")
    rsv = block(hdr, "// [board-pin-reserved:begin]", "// [board-pin-reserved:end]")
    if not rsv:
        raise AssertionError("boardPinReserved() block not found")
    body = "static void apply(const char* field, const char* newValue) {\n  if (false) {}\n"
    for f in FIELDS:
        body += "  else " + setter_branch(ino, f) + "\n"
    body += "}\n"
    main = "int main(){int bad=0;\n"
    for field, member, value, expect, map_, reserved in cases:
        main += (f'  settings.{member}.iPin = -1; apply("{field}", "{value}");\n'
                 f'  if (settings.{member}.iPin != {expect}) {{ std::printf("FAIL {field}={value}: pin %d, expected {expect}\\n", settings.{member}.iPin); bad++; }}\n')
        if map_:
            main += (f'  if (boardPinReserved({value}, {map_}) != {str(reserved).lower()}) '
                     f'{{ std::printf("FAIL GPIO{value} reserved on {map_} != {reserved}\\n"); bad++; }}\n')
    main += "  return bad;}\n"
    src = PRELUDE.replace("BOARDS_H", BOARDS.as_posix()) + rng + "\n" + rsv.replace("inline bool", "static bool", 1) + "\n" + body + main
    with tempfile.TemporaryDirectory() as td:
        cpp = Path(td) / "gpio_range.cpp"
        cpp.write_text(src, encoding="utf-8")
        exe = base.compile_cpp(cpp, Path(td) / "gpio_range.exe")
        if exe is None:
            return None
        r = subprocess.run([str(exe)], capture_output=True, text=True)
        return r.returncode, r.stdout


# (field, settings member, value, expected stored pin (-1 = refused), map for the reserved check, reserved?)
PRO_FREE_HOLES = [
    ("GPIOSENSORSpin", "sensors", 39, 39, "COMBO_MAP_CLASSIC_PRO", False),   # D6
    ("GPIOSENSORSpin", "sensors", 38, 38, "COMBO_MAP_CLASSIC_PRO", False),   # D7
    ("GPIOSENSORSpin", "sensors", 37, 37, "COMBO_MAP_CLASSIC_PRO", False),   # D8
    ("S0COUNTERpin", "s0", 38, 38, None, None),
    ("GPIOOUTPUTSpin", "outputs", 37, 37, None, None),
]
REFUSED = [
    ("GPIOSENSORSpin", "sensors", 22, -1, None, None),   # 22-25 do not exist on the ESP32-S3
    ("GPIOSENSORSpin", "sensors", 25, -1, None, None),
    ("S0COUNTERpin", "s0", 49, -1, None, None),          # above GPIO 48
    ("GPIOOUTPUTSpin", "outputs", -1, -1, None, None),
]
UNCHANGED = [
    ("GPIOSENSORSpin", "sensors", 10, 10, "COMBO_MAP_CLASSIC", False),      # the Dallas default stays fine
    ("GPIOOUTPUTSpin", "outputs", 0, 0, None, None),
    ("GPIOSENSORSpin", "sensors", 16, 16, None, None),
]


class SettingGpioRange(unittest.TestCase):
    def run_cases(self, cases):
        res = build_and_run(cases)
        if res is None:
            self.skipTest("no host C++ compiler")
        rc, out = res
        self.assertEqual(rc, 0, out)

    def test_pro_user_can_pick_a_free_hole(self):
        self.run_cases(PRO_FREE_HOLES)

    def test_numbers_that_are_no_s3_gpio_are_refused(self):
        self.run_cases(REFUSED)

    def test_old_range_still_works(self):
        self.run_cases(UNCHANGED)


class SourceChecks(unittest.TestCase):
    def test_rest_metadata_announces_the_wider_range(self):
        rest = REST.read_text(encoding="utf-8")
        for key in ("gpiosensorspin", "s0counterpin", "gpiooutputspin"):
            m = re.search(r'addInt\(F\("' + key + r'"\), [^;]*?, (\w+)\);', rest)
            self.assertIsNotNone(m, key)
            self.assertEqual(m.group(1), "SETTING_GPIO_MAX", f"{key} still announces max {m.group(1)}")

    def test_board_pins_stay_refused_where_they_are_used(self):
        for f, call in (("sensors_ext.ino", "isBoardReservedPin(settings.sensors.iPin)"),
                        ("s0PulseCount.ino", "isBoardReservedPin(settings.s0.iPin)"),
                        ("outputs_ext.ino", "isBoardReservedPin(settings.outputs.iPin)")):
            self.assertIn(call, (SRC / f).read_text(encoding="utf-8"), f)


if __name__ == "__main__":
    unittest.main(verbosity=2)
