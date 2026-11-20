#!/usr/bin/env python3
"""TASK-2025 (fix option A, pending Robert's choice): on a board without PSRAM the BLE scanner never starts.

09 on .61 (no PSRAM, beta.370, 2026-11-18): with SATbleriskack already true, one POST SATbleenable=true started the
scanner; within about 2 minutes the free heap fell to 9 KB (max block 2 KB, minFree 520 B), web 503/000, MQTT drops,
then a software reset. SATbleenable=true was stored, so every boot started the scanner again: a restart loop with no
way out over REST. The gate was bleActive() = bBleEnable && (psramFound() || bBleRiskAck) (SATble.ino, ADR-222).

Checks, on the REAL code:
1. The REAL bleActive() from SATble.ino, compiled with stubbed settings and psramFound(): without PSRAM it is false
   for every combination of enable and risk-ack (the old gate returns true with both set: red).
2. The REAL bleEnableAllowed() block ([ble-nopsram-refuse] in settingStuff.ino), compiled: a request for BLE on is
   refused without PSRAM and kept with PSRAM; off is always off.
3. Wiring (source checks; a host build cannot boot LittleFS): the SATbleenable setter stores bleEnableAllowed(...)
   (so the boot parse of settings.ini, REST and MQTT all go through it), notes a refused request, and readSettings()
   resets that note per parse and persists the refusal through the deferred flush (settingsDirty), so a stored true
   on a no-PSRAM board is rewritten as false once.

OTGW_SATBLE_INO / OTGW_SETTINGSTUFF_INO point at other copies (old-vs-fix).

Run: python tests/test_ble_nopsram_refuse.py
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
FW = REPO / "src" / "OTGW-firmware"
SATBLE = Path(os.environ.get("OTGW_SATBLE_INO", FW / "SATble.ino"))
SETTINGS = Path(os.environ.get("OTGW_SETTINGSTUFF_INO", FW / "settingStuff.ino"))


def text(p):
    return p.read_text(encoding="utf-8").replace("\r\n", "\n")


def function_body(s, signature):
    i = s.find(signature)
    if i < 0:
        return ""
    j, depth = s.index("{", i), 0
    for k in range(j, len(s)):
        depth += s[k] == "{"
        depth -= s[k] == "}"
        if depth == 0:
            return s[i:k + 1]
    return ""


def run_cpp(src, name):
    with tempfile.TemporaryDirectory() as td:
        cpp = Path(td) / (name + ".cpp")
        cpp.write_text(src, encoding="utf-8")
        exe = base.compile_cpp(cpp, Path(td) / (name + ".exe"))
        if exe is None:
            raise unittest.SkipTest("no host C++ compiler")
        return subprocess.run([str(exe)], capture_output=True, text=True)


class Gate(unittest.TestCase):
    def test_no_psram_never_starts_the_scanner(self):
        gate = function_body(text(SATBLE), "static inline bool bleActive()")
        self.assertTrue(gate, "bleActive() not found in SATble.ino")
        src = ("#include <cstdio>\n"
               "static bool g_psram = false;\nstatic bool psramFound() { return g_psram; }\n"
               "struct { struct { bool bBleEnable = false, bBleRiskAck = false; } sat; } settings;\n" + gate +
               "\nint main(){int bad=0;\n"
               " for (int e=0;e<2;e++) for (int a=0;a<2;a++) {\n"
               "  settings.sat.bBleEnable=e; settings.sat.bBleRiskAck=a; g_psram=false;\n"
               "  if (bleActive()) { std::printf(\"FAIL: no PSRAM, enable=%d ack=%d starts BLE\\n\", e, a); bad++; }\n"
               "  g_psram=true;\n"
               "  if (bleActive() != (bool)e) { std::printf(\"FAIL: PSRAM, enable=%d ack=%d gives %d\\n\", e, a, (int)bleActive()); bad++; }\n"
               " }\n return bad;}\n")
        r = run_cpp(src, "ble_gate")
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)


class Setter(unittest.TestCase):
    def setUp(self):
        self.s = text(SETTINGS)

    def test_a_request_for_ble_on_is_refused_without_psram(self):
        blk = self.s[self.s.find("// [ble-nopsram-refuse:begin]"):self.s.find("// [ble-nopsram-refuse:end]")]
        self.assertTrue(blk.strip(), "[ble-nopsram-refuse] block not found in settingStuff.ino")
        src = ("#include <cstdio>\n" + blk +
               "\nint main(){int bad=0;\n"
               " if (bleEnableAllowed(false, true))  { std::printf(\"FAIL: no PSRAM keeps BLE on\\n\"); bad++; }\n"
               " if (!bleEnableAllowed(true, true))  { std::printf(\"FAIL: PSRAM board refused\\n\"); bad++; }\n"
               " if (bleEnableAllowed(false, false) || bleEnableAllowed(true, false)) { std::printf(\"FAIL: off became on\\n\"); bad++; }\n"
               " return bad;}\n")
        r = run_cpp(src, "ble_allowed")
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)

    def test_the_setter_stores_the_allowed_value_and_notes_a_refusal(self):
        m = re.search(r'PSTR\("SATbleenable"\)\) == 0\)\s*\{([^}]*)\}', self.s)
        self.assertIsNotNone(m, "SATbleenable setter not found")
        body = m.group(1)
        self.assertIn("bleEnableAllowed(psramFound(),", body, "the setter stores the requested value unchecked")
        self.assertIn("g_bleRefusedNoPsram = true", body, "a refused request is not noted")
        self.assertIn("g_sawBleEnableKey = true", body, "TASK-1693 sentinel lost")

    def test_a_stored_true_without_psram_is_rewritten_once(self):
        read = function_body(self.s, "void readSettings(bool show)")
        self.assertIn("g_bleRefusedNoPsram = false;", read, "the refusal note is not reset per parse")
        m = re.search(r"if \(g_bleRefusedNoPsram\) \{(.*?)\}", read, re.S)
        self.assertIsNotNone(m, "readSettings() does not act on a refused stored SATbleenable=true")
        self.assertIn("settingsDirty = true", m.group(1), "the refusal must be persisted (deferred flush)")


class RestAnswer(unittest.TestCase):
    def test_rest_post_refuses_ble_on_without_psram_before_the_setter(self):
        src = text(Path(os.environ.get("OTGW_RESTAPI_INO", FW / "restAPI.ino")))
        body = function_body(src, "void postSettings(")
        self.assertTrue(body, "postSettings() not found")
        m = re.search(r'strcasecmp_P\(field, PSTR\("satbleenable"\)\) == 0 && EVALBOOLEAN\(newValue\) && !psramFound\(\)\)\s*\{'
                      r'\s*webSend\(409,', body)
        self.assertIsNotNone(m, "POST satbleenable=true on a board without PSRAM is not answered with 409")
        self.assertLess(m.start(), body.find("updateSetting(field, newValue)"), "the refusal must come before the setter")


if __name__ == "__main__":
    unittest.main(verbosity=2)
