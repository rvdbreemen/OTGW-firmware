#!/usr/bin/env python3
"""TASK-1806: on thermostat loss OT-Direct writes no setback flow setpoint and does not force CH on, like the PIC.

Robert (2026-11-11): "Like the PIC". Before: after iSetbackTimeout without thermostat frames and with FS set,
checkThermostatTimeout() put the 16 C setback temperature in MsgID 1 (setOverride + updateWriteCache), and the
gateway's own MsgID 0 carried the board default CH enable. The PIC, on an open line, clears the thermostat state
(Disconnect, gateway.asm 6.6 :1770-1789) and its standalone frames carry CH enable only with a CS= in force (readstatus
:2712-2742) and TSet 0 unless a CS= is set (MessageID1 :2796-2820).

REAL code, cut from src/OTGW-firmware/OTDirect.ino and compiled on the host: checkThermostatTimeout(),
otdEffectiveMasterFlags() and buildStatusRequest(). Recorded: setOverride/updateWriteCache on MsgID 1, the state flags,
the MsgID 0 master status byte. OTGW_OTD_INO points at another copy for old-vs-fix.
Run: python tests/test_otd_no_setback.py
"""
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402

OTD_INO = Path(os.environ.get("OTGW_OTD_INO", REPO / "src" / "OTGW-firmware" / "OTDirect.ino"))
SRC = OTD_INO.read_text(encoding="utf-8").replace("\r\n", "\n")


def cut_braced(signature):
    at = SRC.find(signature)
    if at < 0:
        raise AssertionError(signature + " not found")
    start = SRC.find("{", at)
    depth = 0
    for i in range(start, len(SRC)):
        depth += {"{": 1, "}": -1}.get(SRC[i], 0)
        if depth == 0:
            return SRC[at:i + 1]
    raise AssertionError("unbalanced braces in " + signature)


PRE = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define F(s) (s)
#define PSTR(s) (s)
#define OTDDebugTln(s) ((void)0)
#define OTDDebugTf(...) ((void)0)
// TASK-1850 (ba): the PIC's thermostat lines go to port 25238 and processOT()
typedef const char* PGM_P;
static void strlcpy_P(char* d, const char* s, size_t n) { strncpy(d, s, n - 1); d[n - 1] = '\0'; }
static void otDirectBridgeWriteLine(const char*, size_t) {}
static void processOT(const char*, int) {}
enum OTDirectMode { OTD_MODE_BYPASS = 0, OTD_MODE_GATEWAY = 1, OTD_MODE_MONITOR = 2, OTD_MODE_MASTER = 3, OTD_MODE_LOOPBACK = 4 };
static OTDirectMode otCurrentMode = OTD_MODE_GATEWAY;
#define IS_MASTER_MODE() (otCurrentMode == OTD_MODE_MASTER)
enum class OpenThermMessageType : uint8_t { READ_DATA = 0, WRITE_DATA = 1 };
enum class OpenThermMessageID : uint8_t { Status = 0 };
struct OpenTherm { static unsigned long buildRequest(OpenThermMessageType t, OpenThermMessageID id, unsigned int v) {
  return ((unsigned long)t << 28) | ((unsigned long)id << 16) | v; } };
static uint32_t g_now = 0;
static uint32_t millis() { return g_now; }
static bool otThermostatSeen = true;
static uint32_t otLastThermostatMs = 0;
static bool otFailSafeEnabled = true, otSetbackEngaged = false, otChModeOff = false, otPushSpoof = false;
static uint8_t otMasterStatusFlags = 0x01;   // board default: CH enable
struct { struct { uint8_t iSetbackTimeout = 30; float fSetbackTemp = 16.0f; } otd; } settings;
struct { struct { bool bThermostatConnected = true; bool bSetbackActive = false; } otd; } state;
static int g_forget = 0, g_ovr1 = 0, g_cache1 = 0, g_clear1 = 0;
static uint16_t g_ovr1Val = 0;
static void otdForgetThermostatWrites() { g_forget++; }
static int g_wsConnected = 0;
static void sendWebSocketJSON(const char* m) { if (strstr(m, "OTDirect: connected")) g_wsConnected++; }
static uint16_t floatToF88(float c) { return (uint16_t)(int16_t)(c * 256.0f); }
static void setOverride(uint8_t id, uint16_t v) { if (id == 1) { g_ovr1++; g_ovr1Val = v; } }
static void updateWriteCache(uint8_t id, uint16_t) { if (id == 1) g_cache1++; }
static void clearWriteOverride(uint8_t id) { if (id == 1) g_clear1++; }
static char* dtostrf(double v, int, int, char* b) { sprintf(b, "%.1f", v); return b; }
static bool g_cs = false, g_sat = false;
static bool otdCsOverrideActive() { return g_cs; }
static bool satOwnsControlSetpoint() { return g_sat; }
"""

MAIN = r"""
// argv[1]: mode (1 gateway, 3 master); argv[2]: 'c' = a CS= is in force
int main(int argc, char** argv) {
  otCurrentMode = (OTDirectMode)atoi(argv[1]);
  g_cs = argc > 2 && argv[2][0] == 'c';
  otLastThermostatMs = 1000;
  for (g_now = 1000; g_now <= 40000; g_now += 1000) checkThermostatTimeout();   // 39 s of silence
  const unsigned st = (unsigned)((buildStatusRequest() >> 8) & 0xFF);
  const int connectedAfterLoss = (int)state.otd.bThermostatConnected;
  otLastThermostatMs = g_now; checkThermostatTimeout();                          // the thermostat is back
  printf("wsconn=%d ovr1=%d cache1=%d val=%04X setback=%d forget=%d lost=%d back=%d chbit=%u\n", g_wsConnected, g_ovr1, g_cache1, g_ovr1Val,
         (int)state.otd.bSetbackActive, g_forget, connectedAfterLoss ? 0 : 1, (int)state.otd.bThermostatConnected,
         st & 1u);
  return 0;
}
"""


class NoSetback(unittest.TestCase):
    tmp = None
    exe = None

    @classmethod
    def setUpClass(cls):
        body = "\n".join([cut_braced("static void checkThermostatTimeout()"),
                          cut_braced("static uint8_t otdEffectiveMasterFlags(uint8_t boardFlags, bool chModeOff, bool csOverrideActive)"),
                          cut_braced("static unsigned long buildStatusRequest()")])
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text(PRE + body + MAIN, encoding="utf-8")
        cls.exe = compile_cpp(cpp, Path(cls.tmp.name) / ("t.exe" if os.name == "nt" else "t"))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_case(self, *args):
        if not self.exe:
            self.skipTest("no host compiler")
        out = subprocess.run([str(self.exe), *args], capture_output=True, text=True, check=True).stdout
        print(args, out.strip())
        return dict(kv.split("=") for kv in out.split())

    def test_thermostat_loss_writes_no_setback_flow_setpoint(self):
        r = self.run_case("1")
        self.assertEqual((r["ovr1"], r["cache1"], r["setback"]), ("0", "0", "0"), "no 16 C TSet override or write")
        self.assertEqual((r["lost"], r["forget"], r["back"]), ("1", "1", "1"), "loss and return are still tracked")
        self.assertEqual(r["wsconn"], "1", "the return is still reported to the web UI once")

    def test_own_status_after_the_loss_has_ch_off_without_cs(self):
        self.assertEqual(self.run_case("1")["chbit"], "0", "like the PIC standalone status: CH enable only with a CS=")

    def test_a_cs_override_still_enables_ch(self):
        self.assertEqual(self.run_case("1", "c")["chbit"], "1")

    def test_master_mode_unchanged(self):
        self.assertEqual(self.run_case("3")["chbit"], "0")
        self.assertEqual(self.run_case("3", "c")["chbit"], "1")


if __name__ == "__main__":
    unittest.main(verbosity=2)
