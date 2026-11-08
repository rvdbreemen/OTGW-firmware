#!/usr/bin/env python3
"""TASK-1698: a stored 1.x S0/output default (12/16) moves to a free hole on a Classic S3 board.

12 and 16 are board pins on both Classic S3 modules (PIC reset / LED1 on the S3 Mini, I2C SDA on the
S3 Mini Pro), so initS0Count() and validateGPIOOutputsConfig() refused them (TASK-1552) and enabling
S0 or the output did nothing until the user changed the pin.

Not re-implemented here: the test cuts the REAL legacyPinMigrated() block ([legacy-pin-defaults])
and the REAL boardPinReserved() block from OTGW-firmware.h, and the REAL applyLegacyPinDefaults()
([apply-legacy-pin-defaults]) from settingStuff.ino, and compiles them with the real Platform
boards.h for the combo build. Stubbed: the settings struct, comboActivePinMap(), writeSettings()
(counted) and the logging. A source check pins the call in setup() before initOutputs() and
initS0Count().

OTGW_SETTINGSTUFF_INO / OTGW_FIRMWARE_H / OTGW_FIRMWARE_INO point at other copies (old-vs-fix). The
old sources have no migration, so every case goes red. Without a host C++ compiler the compile cases
are skipped, they do not pass silently.

Run: python tests/test_legacy_pin_defaults.py
"""
import os
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
MAIN = Path(os.environ.get("OTGW_FIRMWARE_INO", SRC / "OTGW-firmware.ino"))
BOARDS = REPO / "src" / "libraries" / "Platform" / "src" / "boards.h"


def block(text, begin, end):
    i, j = text.find(begin), text.find(end)
    return text[i:j] if i >= 0 and j > i else ""


PRELUDE = r"""
#include <cstdio>
#include <cstdint>
#define BOARD_NODOSHOP_ESP32_COMBO
#include "BOARDS_H"
enum ComboPinMap : unsigned char { COMBO_MAP_OTGW32 = 0, COMBO_MAP_CLASSIC = 1, COMBO_MAP_CLASSIC_PRO = 2 };
#define PSTR(s) (s)
#define DebugTf(...) do {} while (0)
static ComboPinMap g_map = COMBO_MAP_CLASSIC;
static ComboPinMap comboActivePinMap() { return g_map; }
static int g_writes = 0;
static bool writeSettings(bool) { g_writes++; return true; }
static void settingsMarkDirty() {}
struct { struct { int8_t iPin = 10; } sensors; struct { uint8_t iPin = 12; } s0; struct { int8_t iPin = 16; } outputs; } settings;
"""

# (map, sensors, s0, output) -> (s0, output, writes)
CASES = [
    ("COMBO_MAP_CLASSIC",     10, 12, 16, 13, 11, 1),   # S3 Mini: D6 / D7 holes
    ("COMBO_MAP_CLASSIC_PRO", 10, 12, 16, 39, 37, 1),   # S3 Mini Pro: D6 / D8 holes
    ("COMBO_MAP_OTGW32",      4,  12, 16, 12, 16, 0),   # OTGW32 keeps 12/16 (Robert 2026-11-07)
    ("COMBO_MAP_CLASSIC",     10, 5,  6,  5,  6,  0),   # the user's own pins stay
    ("COMBO_MAP_CLASSIC",     10, 12, 6,  13, 6,  1),   # only the legacy one moves
    ("COMBO_MAP_CLASSIC",     13, 12, 16, 12, 11, 1),   # D6 held by the sensors: S0 stays
    ("COMBO_MAP_CLASSIC",     11, 12, 16, 13, 16, 1),   # D7 held by the sensors: output stays
    ("COMBO_MAP_CLASSIC_PRO", 37, 12, 16, 39, 16, 1),   # Pro D8 held by the sensors
    ("COMBO_MAP_CLASSIC",     -1, 12, 16, 13, 11, 1),   # sensors disabled (-1)
    ("COMBO_MAP_CLASSIC",     10, 12, -1, 13, -1, 1),   # output disabled (-1) stays disabled
    ("COMBO_MAP_CLASSIC",     10, 12, 13, 12, 13, 0),   # D6 held by the user's own output pin: S0 stays
    ("COMBO_MAP_CLASSIC",     10, 11, 16, 11, 16, 0),   # D7 held by the user's own S0 pin: output stays
]


def build_and_run():
    hdr, ino = HDR.read_text(encoding="utf-8"), INO.read_text(encoding="utf-8")
    mig = block(hdr, "// [legacy-pin-defaults:begin]", "// [legacy-pin-defaults:end]")
    rsv = block(hdr, "// [board-pin-reserved:begin]", "// [board-pin-reserved:end]")
    app = block(ino, "// [apply-legacy-pin-defaults:begin]", "// [apply-legacy-pin-defaults:end]")
    missing = [n for n, b in (("legacy-pin-defaults", mig), ("board-pin-reserved", rsv),
                              ("apply-legacy-pin-defaults", app)) if not b]
    if missing:
        raise AssertionError("blocks not found: " + ", ".join(missing) + " (no board-specific pin defaults)")
    main = "int main(){int bad=0;\n"
    for i, (m, sen, s0, out, es0, eout, ew) in enumerate(CASES):
        main += (f"  g_map={m}; settings.sensors.iPin={sen}; settings.s0.iPin={s0}; settings.outputs.iPin={out}; g_writes=0;\n"
                 f"  applyLegacyPinDefaults();\n"
                 f"  if (settings.s0.iPin!={es0} || settings.outputs.iPin!={eout} || g_writes!={ew}) {{"
                 f" std::printf(\"FAIL case {i} {m}: s0 %d out %d writes %d, expected {es0}/{eout}/{ew}\\n\","
                 f" (int)settings.s0.iPin, (int)settings.outputs.iPin, g_writes); bad++; }}\n"
                 f"  g_writes=0; applyLegacyPinDefaults();\n"
                 f"  if (g_writes!=0) {{ std::printf(\"FAIL case {i}: second boot wrote the settings again\\n\"); bad++; }}\n")
        if m != "COMBO_MAP_OTGW32" and ew:
            # only the pins that moved: a legacy pin kept because its new hole is taken stays a board pin
            if es0 != s0:
                main += (f"  if (boardPinReserved({es0},{m})) {{ std::printf(\"FAIL case {i}: new S0 pin {es0} is a board pin\\n\"); bad++; }}\n")
            if eout != out:
                main += (f"  if (boardPinReserved({eout},{m})) {{ std::printf(\"FAIL case {i}: new output pin {eout} is a board pin\\n\"); bad++; }}\n")
    for m in ("COMBO_MAP_CLASSIC", "COMBO_MAP_CLASSIC_PRO"):
        main += (f"  if (!boardPinReserved(12,{m}) || (!boardPinReserved(16,{m}) && {m}==COMBO_MAP_CLASSIC))"
                 f" {{ std::printf(\"FAIL legacy pins not reserved on {m}\\n\"); bad++; }}\n")
    main += "  return bad;}\n"
    src = (PRELUDE.replace("BOARDS_H", BOARDS.as_posix()) + rsv.replace("inline bool", "static bool", 1)
           + "\n" + mig + "\n" + app + "\n" + main)
    with tempfile.TemporaryDirectory() as td:
        cpp = Path(td) / "legacy_pins.cpp"
        cpp.write_text(src, encoding="utf-8")
        exe = base.compile_cpp(cpp, Path(td) / "legacy_pins.exe")
        if exe is None:
            return None
        r = subprocess.run([str(exe)], capture_output=True, text=True)
        return r.returncode, r.stdout


class LegacyPinDefaults(unittest.TestCase):
    def test_legacy_defaults_move_to_a_free_hole_on_classic_only(self):
        res = build_and_run()
        if res is None:
            self.skipTest("no host C++ compiler")
        rc, out = res
        self.assertEqual(rc, 0, out)


OUTPUTS = SRC / "outputs_ext.ino"
OUTPUTS = Path(os.environ.get("OTGW_OUTPUTS_INO", OUTPUTS))


def function_body(text, signature):
    i = text.find(signature)
    if i < 0:
        raise AssertionError(f"{signature} not found")
    j, depth = text.index("{", i), 0
    for k in range(j, len(text)):
        depth += text[k] == "{"
        depth -= text[k] == "}"
        if depth == 0:
            return text[i:k + 1]
    raise AssertionError("unbalanced braces")


class OutputValidatorAcceptsTheNewPins(unittest.TestCase):
    """The REAL validateGPIOOutputsConfig() (outputs_ext.ino) must accept the migrated output pins.
    Review finding: it still capped the pin at 16 (ESP8266 range), so the S3 Mini Pro default 37
    (and every free Pro hole 37-39, TASK-1686) was refused as 'invalid pin'."""

    def test_validator_accepts_the_new_defaults_and_refuses_board_pins(self):
        hdr = HDR.read_text(encoding="utf-8")
        rng = block(hdr, "// [setting-gpio-range:begin]", "// [setting-gpio-range:end]")
        rsv = block(hdr, "// [board-pin-reserved:begin]", "// [board-pin-reserved:end]")
        val = function_body(OUTPUTS.read_text(encoding="utf-8"), "static bool validateGPIOOutputsConfig()")
        prelude = PRELUDE.replace("BOARDS_H", BOARDS.as_posix()).replace(
            "struct { struct { int8_t iPin = 10; } sensors; struct { uint8_t iPin = 12; } s0; struct { int8_t iPin = 16; } outputs; } settings;",
            "struct { struct { int8_t iPin = 16; int8_t iTriggerBit = 0; } outputs; } settings;")
        prelude += "static ComboPinMap g_m; static bool isBoardReservedPin(int p);\n"
        cases = [("COMBO_MAP_CLASSIC_PRO", 37, 1), ("COMBO_MAP_CLASSIC_PRO", 38, 1), ("COMBO_MAP_CLASSIC", 11, 1),
                 ("COMBO_MAP_CLASSIC", 16, 0), ("COMBO_MAP_CLASSIC_PRO", 14, 0), ("COMBO_MAP_CLASSIC", 23, 0),
                 ("COMBO_MAP_CLASSIC", -1, 0)]
        main = "int main(){int bad=0;\n"
        for m, pin, ok in cases:
            main += (f"  g_m={m}; settings.outputs.iPin={pin};"
                     f" if ((int)validateGPIOOutputsConfig()!={ok}) {{ std::printf(\"FAIL {m} pin {pin}: expected {ok}\\n\"); bad++; }}\n")
        main += "  return bad;}\n"
        src = (prelude + rng + "\n" + rsv.replace("inline bool", "static bool", 1)
               + "\nstatic bool isBoardReservedPin(int p) { return boardPinReserved(p, g_m); }\n" + val + "\n" + main)
        with tempfile.TemporaryDirectory() as td:
            cpp = Path(td) / "outputs_validate.cpp"
            cpp.write_text(src, encoding="utf-8")
            exe = base.compile_cpp(cpp, Path(td) / "outputs_validate.exe")
            if exe is None:
                self.skipTest("no host C++ compiler")
            r = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stdout)


class SourceChecks(unittest.TestCase):
    def test_setup_applies_the_defaults_before_the_outputs_and_s0_start(self):
        text = MAIN.read_text(encoding="utf-8")
        i = text.find("applyLegacyPinDefaults();")
        self.assertGreaterEqual(i, 0, "setup() does not call applyLegacyPinDefaults()")
        for before in ("readSettings(true);", "applyResolvedComboPins();"):
            j = text.find(before)
            self.assertGreaterEqual(j, 0, before)
            self.assertLess(j, i, f"applyLegacyPinDefaults() runs before {before} (settings or board map not final)")
        for call in ("initOutputs();", "initS0Count();"):
            j = text.find(call)
            self.assertGreater(j, i, f"{call} runs before applyLegacyPinDefaults()")


if __name__ == "__main__":
    unittest.main(verbosity=2)
