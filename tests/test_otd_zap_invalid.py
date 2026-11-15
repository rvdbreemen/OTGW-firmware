#!/usr/bin/env python3
"""TASK-1804 A2: a boiler Data-Invalid / Unknown-DataID answer to MsgID 1 or 56 drops the gateway's CS= / SW=, like the PIC.

Decision: Q-1804a = A (2026-11-15, under Robert's directive, docs/sat/open-questions.md). PIC ground truth
(other-projects/otgw-6.6/gateway.asm): ctrlsetptreturn :2822-2825 sets SysCtrlSetpoint on a Data-Invalid response to
MsgID 1 (byte1 bit 5 is set for Data-Invalid and Unknown-DataID), so the thermostat's setpoint goes to the boiler again;
InvalidHotWater :3141-3147 clears dhwsetpoint1/2 and the stored value, and does nothing when none was set. SH= (MsgID 57)
is not touched (entry 252).

REAL code: the otdZapInvalid block (markers in src/OTGW-firmware/OTDirect.ino) is compiled with stubs for the override
list, the write cache and the settings store; the two call sites in handleMasterResponse() are checked in the source.
Old-vs-fix: OTGW_OTD_INO=<older OTDirect.ino> python tests/test_otd_zap_invalid.py
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402

REPO = Path(__file__).resolve().parent.parent
OTD_INO = Path(os.environ.get("OTGW_OTD_INO", REPO / "src" / "OTGW-firmware" / "OTDirect.ino"))
SRC = OTD_INO.read_text(encoding="utf-8").replace("\r\n", "\n")

PRE = r"""
#include <cstdint>
#include <cstdio>
#include <cstring>
#define F(s) (s)
#define OTDDebugTln(s) ((void)0)
static bool g_csActive = false;
static int g_cleared[4]; static int g_nCleared = 0;
static uint32_t otCSLastCommandMs = 0;
static uint16_t otdDhwSetF88 = 0;
static bool otdInitSW = false;
static struct { struct { float fDhwSetpoint = 0.0f; } otd; } settings;
static int g_persists = 0;
static bool otdCsOverrideActive() { return g_csActive; }
static void clearWriteOverride(uint8_t id) { if (g_nCleared < 4) g_cleared[g_nCleared++] = id; if (id == 1) g_csActive = false; }
static void otdPersistFloat(float &field, float v) { if (field != v) { field = v; g_persists++; } }
"""

MAIN = r"""
int main(int argc, char** argv) {
  // argv: <msgId> <csActive 0/1> <dhwF88>
  const int id = atoi(argv[1]); g_csActive = atoi(argv[2]) != 0; otdDhwSetF88 = (uint16_t)atoi(argv[3]);
  otCSLastCommandMs = 12345; otdInitSW = otdDhwSetF88 != 0; settings.otd.fDhwSetpoint = otdDhwSetF88 / 256.0f;
  otdZapInvalid((uint8_t)id);
  printf("cleared=");
  for (int i = 0; i < g_nCleared; i++) printf("%d,", g_cleared[i]);
  printf("|cs=%d|csMs=%u|dhw=%u|initsw=%d|stored=%.2f|persists=%d\n", g_csActive ? 1 : 0, (unsigned)otCSLastCommandMs,
         (unsigned)otdDhwSetF88, otdInitSW ? 1 : 0, settings.otd.fDhwSetpoint, g_persists);
  return 0;
}
"""

_tmp = None
_exe = None


def setUpModule():
    global _tmp, _exe
    m = re.search(r"// --- otdZapInvalid begin\n(.*?)// --- otdZapInvalid end", SRC, re.S)
    if not m:
        raise AssertionError("otdZapInvalid block not found in OTDirect.ino (the code before TASK-1804 A2 has none)")
    _tmp = Path(tempfile.mkdtemp(prefix="otd_zap_"))
    cpp = _tmp / "t.cpp"
    cpp.write_text(PRE.replace("#include <cstring>", "#include <cstring>\n#include <cstdlib>") + m.group(1) + MAIN,
                   encoding="utf-8")
    _exe = compile_cpp(cpp, _tmp / ("t.exe" if os.name == "nt" else "t"))
    if _exe is None:
        raise unittest.SkipTest("no C++ compiler")


def tearDownModule():
    if _tmp:
        shutil.rmtree(_tmp, ignore_errors=True)


def run(msg_id, cs_active, dhw):
    out = subprocess.run([str(_exe), str(msg_id), str(int(cs_active)), str(dhw)], capture_output=True, text=True,
                         check=True).stdout.strip()
    head, *fields = out.split("|")
    row = dict(f.split("=", 1) for f in fields)
    row["cleared"] = head.split("=", 1)[1]
    return row


class ZapInvalid(unittest.TestCase):
    def test_cs_refused_drops_the_control_setpoint_override(self):
        r = run(1, True, 0)
        self.assertEqual((r["cleared"], r["cs"], r["csMs"]), ("1,", "0", "0"))

    def test_cs_without_an_override_changes_nothing(self):
        r = run(1, False, 0)
        self.assertEqual((r["cleared"], r["csMs"]), ("", "12345"))

    def test_sw_refused_clears_the_dhw_setpoint_and_its_stored_value(self):
        r = run(56, False, 50 * 256)
        self.assertEqual((r["cleared"], r["dhw"], r["initsw"], r["stored"], r["persists"]), ("56,", "0", "0", "0.00", "1"))

    def test_sw_without_a_value_changes_nothing(self):
        r = run(56, False, 0)
        self.assertEqual((r["cleared"], r["persists"]), ("", "0"))

    def test_sh_and_other_ids_are_left_alone(self):
        for mid in (57, 14, 0):
            r = run(mid, True, 50 * 256)
            self.assertEqual((r["cleared"], r["cs"], r["dhw"]), ("", "1", str(50 * 256)), mid)


class CallSites(unittest.TestCase):
    hm = SRC[SRC.index("static void handleMasterResponse() {"):]
    hm = hm[:hm.index("\n}\n")]

    def test_called_for_unknown_dataid_and_for_data_invalid(self):
        self.assertIn("if (respType == OpenThermMessageType::UNKNOWN_DATA_ID) otdZapInvalid(respMsgId);", self.hm)
        di = self.hm[self.hm.index("} else if (boilerDataInvalid) {"):]
        self.assertIn("otdZapInvalid((uint8_t)((response >> 16) & 0xFF));", di[:900])


if __name__ == "__main__":
    unittest.main(verbosity=2)
