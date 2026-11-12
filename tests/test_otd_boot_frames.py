#!/usr/bin/env python3
"""TASK-1835 (PIC 6.8 parity audit N5): OT-Direct sends no frame of its own at boot in gateway or monitor mode.

The PIC creates frames of its own only without a thermostat (gateway.asm 6.8 StandAloneMsg :1797-1820,
HandleRequest :2388-2407) and never in monitor mode. OT-Direct sent, in every mode, a status request and the
WRITE 2 / READ 3 / WRITE 124 / WRITE 126 handshake from initOTDirect() before the thermostat's first frame.

The real initOTDirect() (and, on the fixed code, otdBootHandshake()) are cut from src/OTGW-firmware/OTDirect.ino
and compiled with a host C++ compiler; only what they touch is stubbed: the OpenTherm master/slave (the mock
master counts every frame it is asked to send and notes whether that happened before or after the auto-detect's
"no thermostat" decision), pins, delays, settings and state. OTGW_OTD_INO points at another copy for old-vs-fix.
Run: python tests/test_otd_boot_frames.py
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


def func(header):
    i = SRC.find(header)
    if i < 0:
        return ""
    return SRC[i:SRC.find("\n}\n", i) + 3]


PRE = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define F(s) (s)
#define PSTR(s) (s)
#define HAS_BYPASS_RELAY 0
#define DebugTln(s) ((void)0)
#define OTDDebugTln(s) ((void)0)
#define OTDDebugTf(...) ((void)0)
enum class OpenThermMessageType { READ_DATA = 0, WRITE_DATA = 1 };
enum class OpenThermMessageID { Status = 0, MConfigMMemberIDcode = 2, SConfigSMemberIDcode = 3,
                                OpenThermVersionMaster = 124, MasterVersion = 126 };
enum OTDirectMode { OTD_MODE_BYPASS = 0, OTD_MODE_GATEWAY = 1, OTD_MODE_MONITOR = 2, OTD_MODE_MASTER = 3 };
static OTDirectMode otCurrentMode = OTD_MODE_GATEWAY;
#define IS_BYPASS_MODE()   (otCurrentMode == OTD_MODE_BYPASS)
#define IS_MONITOR_MODE()  (otCurrentMode == OTD_MODE_MONITOR)
#define IS_MASTER_MODE()   (otCurrentMode == OTD_MODE_MASTER)
static uint32_t g_ms = 0;
static uint32_t millis() { return g_ms; }
static void delay(uint32_t ms) { g_ms += ms; }
static void feedWatchDog() {}
static bool g_thermostat = false, otSlaveFramePending = false, g_decided = false;
static int g_before = 0, g_after = 0;
struct MockOT {
  bool master;
  void begin(void (*)()) {}
  void begin(void (*)(), void (*)(unsigned long, int)) {}
  unsigned long sendRequest(unsigned long f) { if (g_decided) g_after++; else g_before++; return f | 0x40000000UL; }
  bool isValidResponse(unsigned long) { return true; }
  void process() { if (!master && g_thermostat) otSlaveFramePending = true; }
  static unsigned long buildRequest(OpenThermMessageType t, OpenThermMessageID id, unsigned int data) {
    return ((unsigned long)t << 28) | ((unsigned long)id << 16) | data;
  }
};
using OpenTherm = MockOT;
static MockOT otMaster{true}, otSlave{false};
static void masterISR() {}
static void slaveISR() {}
static void handleSlaveRequest(unsigned long, int) {}
struct OtdSettings { int iMode; float fSetbackTemp; bool bSummerMode, bFailSafe; unsigned iMsgInterval; unsigned iVentSetpoint;
                     bool bEnableSlave, bAutoDetect; };
struct Settings { OtdSettings otd; };
static Settings settings = { { 1, 16.0f, false, true, 100, 0, true, false } };
enum { HW_MODE_PIC = 1, HW_MODE_OT_DIRECT = 2 };
struct State { struct { int eMode; } hw; struct { bool bOnline; } otBus; struct { bool bThermostatConnected; } otd; };
static State state = {};
static bool isPICEnabled() { return false; }
static OTDirectMode otdModeFromSetting(int m) { return (OTDirectMode)m; }
static void initFlameRatio() {}
static void updateWriteCache(uint8_t, uint16_t) {}
static void setOverride(uint8_t, uint16_t) {}
static void bridgeFrameToParser(char, unsigned long) {}
static void sendWebSocketJSON(const char*) {}
static void enableStepUp() {}
static bool probeOTBus() { return true; }
static unsigned long buildStatusRequest() { return OpenTherm::buildRequest(OpenThermMessageType::READ_DATA, OpenThermMessageID::Status, 0x0100); }
static void applyOTDirectMode(OTDirectMode m) { g_decided = true; otCurrentMode = m; }
static float otSetbackTemp; static bool otSummerMode, otFailSafeEnabled; static unsigned otMinIntervalMs;
static uint8_t otMasterStatusFlags = 0x01;
static uint32_t otLastThermostatMs; static bool otThermostatSeen;
"""

MAIN = r"""
int main(int argc, char** argv) {
  // argv: mode autodetect thermostat
  settings.otd.iMode = atoi(argv[1]);
  settings.otd.bAutoDetect = atoi(argv[2]) != 0;
  g_thermostat = atoi(argv[3]) != 0;
  initOTDirect();
  printf("%d %d %d\n", g_before, g_after, (int)otCurrentMode);
  return 0;
}
"""


class BootFrames(unittest.TestCase):
    exe = None
    err = None
    tmp = None

    @classmethod
    def setUpClass(cls):
        init = func("void initOTDirect() {")
        if not init:
            cls.err = "initOTDirect() not found"
            return
        hs = func("static void otdBootHandshake() {")   # absent on the code before TASK-1835
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text(PRE + hs + "\n" + init + MAIN, encoding="utf-8")
        cls.exe = compile_cpp(cpp, Path(cls.tmp.name) / ("t.exe" if os.name == "nt" else "t"))
        if cls.exe is None:
            cls.err = "SKIP no host compiler"

    @classmethod
    def tearDownClass(cls):
        if cls.tmp:
            cls.tmp.cleanup()

    def setUp(self):
        if self.err and self.err.startswith("SKIP"):
            self.skipTest(self.err)
        if self.err:
            self.fail(self.err)

    def boot(self, mode, autodetect=False, thermostat=False):
        out = subprocess.run([str(self.exe), str(mode), str(int(autodetect)), str(int(thermostat))],
                             capture_output=True, text=True, check=True).stdout.split()
        return int(out[0]), int(out[1]), int(out[2])

    def test_gateway_mode_sends_nothing_at_boot(self):
        self.assertEqual(self.boot(1)[:2], (0, 0))

    def test_gateway_with_a_thermostat_found_by_auto_detect_sends_nothing(self):
        self.assertEqual(self.boot(1, autodetect=True, thermostat=True), (0, 0, 1))

    def test_monitor_mode_sends_nothing_at_boot(self):
        self.assertEqual(self.boot(2)[:2], (0, 0))

    def test_bypass_mode_sends_nothing_at_boot(self):
        self.assertEqual(self.boot(0)[:2], (0, 0))

    def test_no_thermostat_frames_only_after_the_decision(self):
        # auto-detect finds no thermostat and switches to master for this boot: the handshake follows the decision
        before, after, mode = self.boot(1, autodetect=True, thermostat=False)
        self.assertEqual((before, mode), (0, 3), "no gateway-made frame before the no-thermostat decision")
        self.assertEqual(after, 5, "status request + WRITE 2, READ 3, WRITE 124, WRITE 126 once master")

    def test_master_mode_keeps_the_boot_handshake(self):
        self.assertEqual(self.boot(3), (5, 0, 3))


if __name__ == "__main__":
    unittest.main(verbosity=2)
