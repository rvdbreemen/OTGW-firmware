#!/usr/bin/env python3
"""TASK-3120 (Robert Q32 A, 'S0 is belofte van mij'): board-specific pin defaults on the OTGW32.

Facts: the OTGW32 Dallas default 10 is PIN_STEPUP_ENABLE (a board pin), so sensors never started; the S0 default 12 is the
W5500 SCK and the output default 16 the W5500 RST. 1-Wire sits on GPIO 4 (boards.h PIN_1WIRE). S0 stays a feature of the
OTGW32 but has no confirmed pin yet (the DI pad, GPIO 5, is unconfirmed): it is NOT SET (-1) until then. The GPIO output has
no pad on the OTGW32 and is hidden there (boardHasOutputPin()).

Not re-implemented: the real legacyPinMigrated() block, the real boardPinReserved() block, the real boardHasOutputPin()
block and the real applyLegacyPinDefaults() are cut out of the sources and compiled with the real Platform boards.h (same
harness as TASK-1698, tests/test_legacy_pin_defaults.py). Without a host C++ compiler the compile cases skip.
Old-vs-fix: OTGW_SETTINGSTUFF_INO / OTGW_FIRMWARE_H point at other copies.

Cases per map: (map, sensors, s0, output) -> (sensors, s0, output, writes of settings.ini). A stored pin the user chose
never moves. Source checks (text, not a run): device/info has_output_pin, the S0 REST range allows -1, initS0Count() and the
S0 setter know -1, initOutputs() knows boardHasOutputPin().

Run: python tests/test_board_defaults_3120.py
"""
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_sat_pwm_override as base  # noqa: E402
import test_legacy_pin_defaults as old  # noqa: E402

SRC = old.SRC

# (map, sensors, s0, output, expected sensors, expected s0, expected output, writes)
CASES = [
    ("COMBO_MAP_OTGW32", 10, 12, 16, 4, 5, 16, 1),     # all legacy: Dallas to GPIO 4, S0 to 5 (the DI pad), output left alone
    ("COMBO_MAP_OTGW32", 4, 12, 16, 4, 5, 16, 1),      # Dallas already on 4
    ("COMBO_MAP_OTGW32", 10, 5, 16, 4, 5, 16, 1),      # the user's own S0 pin 5 stays
    ("COMBO_MAP_OTGW32", 15, 7, 40, 15, 7, 40, 0),     # nothing legacy: nothing moves, nothing written
    ("COMBO_MAP_OTGW32", 10, 12, -1, 4, 5, -1, 1),     # output disabled (-1) stays
    ("COMBO_MAP_OTGW32", -1, 12, 16, -1, 5, 16, 1),    # sensors disabled (-1) is not the legacy 10
    ("COMBO_MAP_OTGW32", 10, 4, 16, 10, 4, 16, 0),     # GPIO 4 held by the user's own S0 pin: Dallas stays
    ("COMBO_MAP_OTGW32", 11, 12, 16, 11, 5, 16, 1),    # a stored sensors pin 11 is never touched
    ("COMBO_MAP_OTGW32", 15, 12, 5, 15, 12, 5, 0),     # GPIO 5 already held by the output: S0 is not moved onto it
    ("COMBO_MAP_OTGW32", 15, -1, 16, 15, -1, 16, 0),   # S0 deliberately not set (-1) stays not set
    ("COMBO_MAP_CLASSIC", 10, 12, 16, 10, 13, 11, 1),  # Classic: Dallas default 10 is the valid D8 hole, unchanged (TASK-1698)
    ("COMBO_MAP_CLASSIC_PRO", 10, 12, 16, 10, 39, 37, 1),
]


def build_and_run():
    hdr = old.HDR.read_text(encoding="utf-8")
    ino = old.INO.read_text(encoding="utf-8")
    mig = old.block(hdr, "// [legacy-pin-defaults:begin]", "// [legacy-pin-defaults:end]")
    rsv = old.block(hdr, "// [board-pin-reserved:begin]", "// [board-pin-reserved:end]")
    has = old.block(hdr, "// [board-has-output:begin]", "// [board-has-output:end]")
    app = old.block(ino, "// [apply-legacy-pin-defaults:begin]", "// [apply-legacy-pin-defaults:end]")
    if not (mig and rsv and app):
        raise AssertionError("pin default blocks not found")
    if not has:
        raise AssertionError("no [board-has-output] block in OTGW-firmware.h")
    main = "int main(){int bad=0;\n"
    for i, (m, sen, s0, out, esen, es0, eout, ew) in enumerate(CASES):
        main += (f"  g_map={m}; settings.sensors.iPin={sen}; settings.s0.iPin={s0}; settings.outputs.iPin={out}; g_writes=0;\n"
                 f"  applyLegacyPinDefaults();\n"
                 f"  if (settings.sensors.iPin!={esen} || settings.s0.iPin!={es0} || settings.outputs.iPin!={eout} || g_writes!={ew}) {{"
                 f" std::printf(\"FAIL case {i} {m}: sen %d s0 %d out %d writes %d, expected {esen}/{es0}/{eout}/{ew}\\n\","
                 f" (int)settings.sensors.iPin, (int)settings.s0.iPin, (int)settings.outputs.iPin, g_writes); bad++; }}\n"
                 f"  g_writes=0; applyLegacyPinDefaults();\n"
                 f"  if (g_writes!=0) {{ std::printf(\"FAIL case {i}: second boot wrote the settings again\\n\"); bad++; }}\n")
        if esen != sen:   # a pin that moved must be a free pin of its map
            main += (f"  if (boardPinReserved({esen},{m})) {{ std::printf(\"FAIL case {i}: new Dallas pin {esen} is a board pin\\n\"); bad++; }}\n")
    main += ("  if (boardPinReserved(5,COMBO_MAP_OTGW32)) { std::printf(\"FAIL GPIO 5 (DI pad) is reserved on OTGW32\\n\"); bad++; }\n"
             "  if (OTGW32_S0_DEFAULT_PIN != 5) { std::printf(\"FAIL OTGW32 S0 default is not 5\\n\"); bad++; }\n"
             "  if (!boardPinReserved(10,COMBO_MAP_OTGW32)) { std::printf(\"FAIL legacy Dallas 10 is not reserved on OTGW32\\n\"); bad++; }\n"
             "  if (boardPinReserved(4,COMBO_MAP_OTGW32)) { std::printf(\"FAIL GPIO 4 is reserved on OTGW32\\n\"); bad++; }\n"
             "  if (!boardPinReserved(12,COMBO_MAP_OTGW32) || !boardPinReserved(16,COMBO_MAP_OTGW32)) { std::printf(\"FAIL legacy S0/output are not W5500 pins\\n\"); bad++; }\n"
             "  if (boardHasOutputPin(COMBO_MAP_OTGW32)) { std::printf(\"FAIL OTGW32 has an output pin\\n\"); bad++; }\n"
             "  if (!boardHasOutputPin(COMBO_MAP_CLASSIC) || !boardHasOutputPin(COMBO_MAP_CLASSIC_PRO)) { std::printf(\"FAIL Classic has no output pin\\n\"); bad++; }\n"
             "  return bad;}\n")
    src = (old.PRELUDE.replace("BOARDS_H", old.BOARDS.as_posix()) + rsv.replace("inline bool", "static bool", 1)
           + "\n" + has + "\n" + mig + "\n" + app + "\n" + main)
    with tempfile.TemporaryDirectory() as td:
        cpp = Path(td) / "board_defaults.cpp"
        cpp.write_text(src, encoding="utf-8")
        exe = base.compile_cpp(cpp, Path(td) / "board_defaults.exe")
        if exe is None:
            return None
        r = subprocess.run([str(exe)], capture_output=True, text=True)
        return r.returncode, r.stdout


class BoardDefaults(unittest.TestCase):
    def test_otgw32_defaults_and_classic_unchanged(self):
        res = build_and_run()
        if res is None:
            self.skipTest("no host C++ compiler")
        rc, out = res
        self.assertEqual(rc, 0, out)


def rd(p):
    return Path(p).read_text(encoding="utf-8").replace("\r\n", "\n")


class SourceChecks(unittest.TestCase):
    """Text checks (not a run)."""

    def test_surfaces(self):
        rest = rd(SRC / "restAPI.ino")
        self.assertIn('je.field(F("has_output_pin")', rest)
        self.assertRegex(rest, r'addInt\(F\("s0counterpin"\), settings\.s0\.iPin, "i", -1, SETTING_GPIO_MAX\)')
        self.assertRegex(rd(SRC / "S0types.h"), r"int8_t\s+iPin\s*=\s*12;")
        self.assertRegex(rd(SRC / "s0PulseCount.ino"), r"settings\.s0\.iPin\s*<\s*0")
        self.assertRegex(rd(SRC / "settingStuff.ino"), r"(?s)S0COUNTERpin.*?newPin\s*==\s*-1")
        self.assertIn("boardHasOutputPin()", rd(SRC / "outputs_ext.ino"))

    def test_constants_are_named(self):
        hdr = rd(SRC / "OTGW-firmware.h")
        self.assertIn("#define OTGW32_S0_DEFAULT_PIN", hdr)
        self.assertIn("#define OTGW32_DALLAS_PIN", hdr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
