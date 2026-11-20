#!/usr/bin/env python3
"""TASK-1850: OT-Direct prints the PIC's "Thermostat disconnected" / "Thermostat connected" lines.

    python tests/test_otd_thermostat_lines.py                       # the working tree
    OTGW_OTD_REV=<sha> python tests/test_otd_thermostat_lines.py     # another revision (old vs fix)

The PIC 6.8 prints "Thermostat disconnected" when the thermostat is lost and "Thermostat connected" when it comes back
(gateway.asm 6.8: strings TDisconnect/TConnect, Disconnect and the reconnect path), also when no thermostat is there
at boot; at boot a present thermostat gets no line (its own tests: standalone-1.stc, standalone-2.stc,
standalone-7.stc). The firmware turns those lines into events (OTGW-Core.ino handlePICStatusToken) and the PIC
sends them to port 25238. OT-Direct must do the same through otDirectBridgeWriteLine() + processOT().
TASK-1999: the boot-time line of a thermostat that was never seen goes to processOT() only, not to port 25238: the PIC
prints it tens of ms after its own reset, before any 25238 client can exist, while OT-Direct reached it ~19 s after boot,
inside pyotgw's connect handshake (IndexError in process_statusfields_v4). Every later transition goes to both.

checkThermostatTimeout() is sliced from the revision's src/OTGW-firmware/OTDirect.ino (from its comment block to
the closing brace) and compiled with stubs for what it calls; the stubs record every bridge line and processOT()
line. Each case is its own process, so the function's statics start fresh like a boot.

Run: python tests/test_otd_thermostat_lines.py
"""
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_published_curve import compile_cpp  # noqa: E402

REPO = Path(__file__).resolve().parent.parent
OTD = "src/OTGW-firmware/OTDirect.ino"
REV = os.environ.get("OTGW_OTD_REV")


def read():
    if REV is None:
        return (REPO / OTD).read_text(encoding="utf-8").replace("\r\n", "\n")
    return subprocess.run(["git", "-C", str(REPO), "show", f"{REV}:{OTD}"], capture_output=True, text=True,
                          encoding="utf-8", check=True).stdout.replace("\r\n", "\n")


HARNESS = r"""
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#define F(s) (s)
#define PSTR(s) (s)
typedef const char *PGM_P;
static size_t strlcpy_P(char *d, const char *s, size_t n) { size_t l = strlen(s); if (n) { size_t c = l < n - 1 ? l : n - 1; memcpy(d, s, c); d[c] = 0; } return l; }
#define OTDDebugTf(...) ((void)0)
#define OTDDebugTln(...) ((void)0)
enum { OTD_MODE_MASTER = 1, OTD_MODE_GATEWAY = 2 };
struct { struct { uint32_t iSetbackTimeout; float fSetbackTemp; } otd; } settings = {{30, 16.0f}};
struct { struct { bool bThermostatConnected; bool bSetbackActive; } otd; } state = {{false, false}};
static uint32_t g_now = 0;
static uint32_t millis() { return g_now; }
static bool     otThermostatSeen = false;
static uint32_t otLastThermostatMs = 0;
static bool     otFailSafeEnabled = false;
static int      otCurrentMode = OTD_MODE_GATEWAY;
static bool     otSetbackEngaged = false;
static std::vector<std::string> g_lines;      // what reached port 25238
static std::vector<std::string> g_processed;  // what reached processOT()
static void otDirectBridgeWriteLine(const char *l, size_t n) { g_lines.push_back(std::string(l, n)); }
static void processOT(const char *l, int n) { g_processed.push_back(std::string(l, (size_t)n)); }
static void sendWebSocketJSON(const char *) {}
static void otdForgetThermostatWrites() {}
// TASK-1804 A3: the loss forgets a TT=/TC= override (clearRemoteOverride), recorded here
enum { OT_OVERRIDE_NONE = 0, OT_OVERRIDE_TEMPORARY = 1 };
static struct { int mode; } otRemoteOverride = { OT_OVERRIDE_NONE };
static int g_overrideClears = 0;
static void clearRemoteOverride() { g_overrideClears++; otRemoteOverride.mode = OT_OVERRIDE_NONE; }
static uint16_t floatToF88(float f) { return (uint16_t)(f * 256.0f); }
static void setOverride(int, uint16_t) {}
static void updateWriteCache(int, uint16_t) {}
static void clearWriteOverride(int) {}
static char *dtostrf(double v, int, int, char *b) { snprintf(b, 8, "%.1f", v); return b; }

@@SLICE@@

// one second of loop: optionally a thermostat frame, then the 1 s check
static void tick(bool frame) {
  g_now += 1000;
  if (frame) { otThermostatSeen = true; otLastThermostatMs = g_now; }
  checkThermostatTimeout();
}

static void report(const char *c) {
  printf("%s|", c);
  for (size_t i = 0; i < g_lines.size(); i++) printf("%s%s", i ? ";" : "", g_lines[i].c_str());
  printf("|");
  for (size_t i = 0; i < g_processed.size(); i++) printf("%s%s", i ? ";" : "", g_processed[i].c_str());
  printf("\n");
}

int main(int argc, char **argv) {
  const std::string c = argc > 1 ? argv[1] : "";
  if (c == "present") {               // thermostat there from boot, never lost
    for (int i = 0; i < 120; i++) tick(true);
  } else if (c == "loss") {           // thermostat there, then lost for longer than the timeout
    for (int i = 0; i < 20; i++) tick(true);
    for (int i = 0; i < 90; i++) tick(false);
  } else if (c == "return") {         // lost, then back (no fail-safe, so no setback)
    for (int i = 0; i < 20; i++) tick(true);
    for (int i = 0; i < 60; i++) tick(false);
    for (int i = 0; i < 20; i++) tick(true);
  } else if (c == "return_setback") { // the same with fail-safe on in gateway mode (setback engaged)
    otFailSafeEnabled = true;
    for (int i = 0; i < 20; i++) tick(true);
    for (int i = 0; i < 60; i++) tick(false);
    for (int i = 0; i < 20; i++) tick(true);
  } else if (c == "never") {          // no thermostat at all since boot
    for (int i = 0; i < 120; i++) tick(false);
  } else if (c == "late") {           // TASK-1999: no thermostat at boot, plugged in later, then lost
    for (int i = 0; i < 30; i++) tick(false);
    for (int i = 0; i < 20; i++) tick(true);
    for (int i = 0; i < 10; i++) tick(false);
  }
  report(c.c_str());
  return 0;
}
"""

DIS, CON = "Thermostat disconnected", "Thermostat connected"


class OtdThermostatLines(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        src = read()
        a = src.find("// checkThermostatTimeout")
        a = src.rfind("// -----", 0, a)
        b = src.find("\n}\n", src.find("static void checkThermostatTimeout()", a))
        if a < 0 or b < 0:
            raise AssertionError("checkThermostatTimeout() not found in OTDirect.ino")
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text(HARNESS.replace("@@SLICE@@", src[a:b + 3]), encoding="utf-8")
        exe = compile_cpp(cpp, Path(cls.tmp.name) / "t.exe")
        if exe is None:
            raise unittest.SkipTest("no host C++ compiler")
        cls.rows = {}
        for case in ("present", "loss", "return", "return_setback", "never", "late"):
            out = subprocess.run([str(exe), case], capture_output=True, text=True, check=True).stdout.strip()
            name, lines, processed = out.split("|")
            cls.rows[name] = ([x for x in lines.split(";") if x], [x for x in processed.split(";") if x])
            print(out)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def assertBoth(self, case, want):
        lines, processed = self.rows[case]
        self.assertEqual(lines, want, f"{case}: port 25238 lines")
        self.assertEqual(processed, want, f"{case}: processOT() lines")

    def test_present_at_boot_prints_nothing(self):
        self.assertBoth("present", [])

    def test_loss_prints_disconnected_once(self):
        self.assertBoth("loss", [DIS])

    def test_return_prints_connected_without_setback(self):
        self.assertBoth("return", [DIS, CON])

    def test_return_prints_connected_with_setback(self):
        self.assertBoth("return_setback", [DIS, CON])

    def test_no_thermostat_since_boot_reports_disconnected_once_but_not_to_the_bridge(self):
        # TASK-1999: the event reaches processOT() once, port 25238 gets no unsolicited line at boot
        lines, processed = self.rows["never"]
        self.assertEqual(lines, [], "never: no boot-time line on port 25238 (it broke the pyotgw connect, TASK-1999)")
        self.assertEqual(processed, [DIS], "never: the event still reaches processOT() once, as on the PIC")

    def test_thermostat_plugged_in_later_reports_connected_and_a_later_loss_on_the_bridge(self):
        # TASK-1999: after the silent boot line, the real transitions reach both (TASK-1850 parity)
        lines, processed = self.rows["late"]
        self.assertEqual(lines, [CON, DIS], "late: the real connect and the real loss on port 25238")
        self.assertEqual(processed, [DIS, CON, DIS], "late: boot event, connect, loss in processOT()")


if __name__ == "__main__":
    unittest.main(verbosity=2)
