#!/usr/bin/env python3
"""TASK-1701: OT-Direct gateway mode must not send a MsgID 0 of its own while a thermostat is on the bus.

Field report (tjfs, 2026-11-07): the thermostat sent master status 0x02 (CH off, DHW on) about once a
second, the gateway's scheduler sent its own MsgID 0 with 0x01 (CH on) every 800 ms, and the boiler
followed whichever status came last: it toggled between CH demand with flame and standby although the
room was above the setpoint. The PIC sends one boiler frame per thermostat frame and generates frames
of its own only without a thermostat (gateway.asm:2384-2402, 1770-1816).

The real scheduleMasterRequest(), buildStatusRequest(), the schedule table and the flag helpers are cut
from src/OTGW-firmware/OTDirect.ino and compiled with a host C++ compiler; only the bus, millis() and
the OpenTherm frame builder are stand-ins. A boiler model takes bit 0 of the last MsgID 0 it received
from either master as its CH demand. OTGW_OTD_INO points at another copy (e.g. `git show <rev>:...`)
for the old-vs-fix run; the old code has no otdOwnStatusRequestAllowed() and must fail G1.

Run: python tests/test_otd_gateway_own_status.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402

OTD_INO = Path(os.environ.get("OTGW_OTD_INO", REPO / "src" / "OTGW-firmware" / "OTDirect.ino"))
SRC = OTD_INO.read_text(encoding="utf-8")


def between(src, start_re, end_re, include_end=True):
    m = re.search(start_re, src, re.M)
    if not m:
        raise AssertionError(f"slice start not found: {start_re}")
    e = re.compile(end_re, re.M).search(src, m.start())
    if not e:
        raise AssertionError(f"slice end not found: {end_re}")
    return src[m.start():e.end() if include_end else e.start()]


def function(src, header_re):
    m = re.search(header_re, src, re.M)
    if not m:
        raise AssertionError(f"function not found: {header_re}")
    e = src.find("\n}\n", m.start())
    return src[m.start():e + 3]


def markers(src, name):
    m = re.search(rf"// --- {name} begin\r?\n(.*?)// --- {name} end", src, re.S)
    return m.group(1) if m else None


def slices(src):
    parts = [
        between(src, r"^static constexpr uint32_t OT_STATUS_INTERVAL_MS\b", r"^static bool otChModeOff = false;.*$"),
        between(src, r"^struct OTScheduleEntry \{", r"^static uint8_t otScheduleIdx = 0;.*$"),
        between(src, r"^static constexpr uint32_t OT_VENT_FAST_INTERVAL_MS\b", r";.*$"),
        markers(src, "otdEffectiveMasterFlags") or "",
    ]
    own = markers(src, "otdOwnStatusAllowed")   # absent before TASK-1701
    if own:
        parts.append(own)
    parts.append(function(src, r"^static unsigned long buildStatusRequest\(\)\s*\{"))
    parts.append(function(src, r"^static void scheduleMasterRequest\(\)\s*\{"))
    return "\n".join(parts)


PRE = r"""
#include <cstdint>
#include <cstdio>
#include <cstring>
enum OTDirectMode { OTD_MODE_GATEWAY = 0, OTD_MODE_MONITOR = 1, OTD_MODE_BYPASS = 2, OTD_MODE_MASTER = 3, OTD_MODE_LOOPBACK = 4 };
static OTDirectMode otCurrentMode = OTD_MODE_GATEWAY;
#define IS_MASTER_MODE()   (otCurrentMode == OTD_MODE_MASTER)
#define IS_MONITOR_MODE()  (otCurrentMode == OTD_MODE_MONITOR)
enum OTDirectRequestOrigin { OT_DIRECT_ORIGIN_GATEWAY = 0, OT_DIRECT_ORIGIN_THERMOSTAT = 1 };
enum class OpenThermMessageType : uint8_t { READ_DATA = 0, WRITE_DATA = 1 };
enum class OpenThermMessageID : uint8_t { Status = 0 };
struct OpenTherm {
  static unsigned long buildRequest(OpenThermMessageType t, OpenThermMessageID id, unsigned int data) {
    unsigned long f = ((unsigned long)t << 28) | ((unsigned long)(uint8_t)id << 16) | (data & 0xFFFF);
    int n = 0; for (int b = 0; b < 31; b++) n += (int)((f >> b) & 1UL);
    return (n & 1) ? (f | 0x80000000UL) : f;
  }
};
static struct { struct { bool bOnline; } otBus; struct { bool bThermostatConnected; } otd; } state;
static uint32_t g_now = 0;
static uint32_t millis() { return g_now; }
static bool otMasterRequestActive = false;
static uint32_t otMinIntervalMs = 100;
static uint32_t otLastAnySendMs = 0;
static constexpr uint8_t OT_CMD_QUEUE_SIZE = 12;
static uint32_t otCmdQueue[OT_CMD_QUEUE_SIZE];
static uint8_t otCmdHead = 0, otCmdTail = 0;
static bool otCmdQueueEmpty() { return otCmdHead == otCmdTail; }
static bool otIsVentSlave() { return false; }
static bool otdCsOverrideActive() { return false; }
static bool satOwnsControlSetpoint() { return false; }

// Bus stand-in: every frame the gateway sends reaches the boiler model.
static int g_bootCH = -1;           // boiler CH demand: bit 0 of the last MsgID 0 it received
static int g_chChanges = 0;
static int g_ownStatus = 0, g_ownOther = 0;
static unsigned g_lastOwnStatusHB = 0xFFFF;
static void boilerSeesStatus(unsigned hb) {
  int ch = (int)(hb & 1);
  if (g_bootCH >= 0 && ch != g_bootCH) g_chChanges++;
  g_bootCH = ch;
}
static bool sendMasterRequestAsync(unsigned long req, OTDirectRequestOrigin) {
  otLastAnySendMs = g_now;
  if (((req >> 16) & 0xFF) == 0) { g_ownStatus++; g_lastOwnStatusHB = (unsigned)((req >> 8) & 0xFF); boilerSeesStatus(g_lastOwnStatusHB); }
  else g_ownOther++;
  return true;
}
"""

MAIN = r"""
// Six minutes as in tjfs's log: the thermostat sends MsgID 0 with master status 0x02 every second,
// loopOTDirect() calls scheduleMasterRequest() every 100 ms.
static void run(OTDirectMode mode, bool thermostat, const char* name) {
  otCurrentMode = mode; state.otd.bThermostatConnected = thermostat; state.otBus.bOnline = true;
  g_bootCH = -1; g_chChanges = 0; g_ownStatus = 0; g_ownOther = 0; g_lastOwnStatusHB = 0xFFFF;
  otLastAnySendMs = 0; otScheduleIdx = 0;
  for (uint8_t i = 0; i < OT_SCHEDULE_SIZE; i++) otSchedule[i].lastSentMs = 0;
  for (g_now = 1000; g_now < 1000 + 360000; g_now += 100) {
    if (thermostat && g_now % 1000 == 0) boilerSeesStatus(0x02);
    scheduleMasterRequest();
  }
  printf("%s own_status=%d own_other=%d ch_changes=%d last_ch=%d own_hb=%u\n",
         name, g_ownStatus, g_ownOther, g_chChanges, g_bootCH, g_lastOwnStatusHB);
}
int main() {
  run(OTD_MODE_GATEWAY, true,  "G1");   // tjfs: gateway mode, thermostat CH off
  run(OTD_MODE_GATEWAY, false, "G2");   // gateway mode, no thermostat: the gateway is the only master
  run(OTD_MODE_MASTER,  false, "G3");   // master mode, SAT off: own MsgID 0 with CH off (TASK-1684)
  return 0;
}
"""


class Compiled(unittest.TestCase):
    rows = None
    err = None

    @classmethod
    def setUpClass(cls):
        with tempfile.TemporaryDirectory() as d:
            cpp = Path(d) / "t.cpp"
            cpp.write_text(PRE + slices(SRC) + MAIN, encoding="utf-8")
            exe = compile_cpp(cpp, Path(d) / ("t.exe" if os.name == "nt" else "t"))
            if exe is None:
                cls.err = "SKIP no host compiler"
                return
            out = subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout
        print(out, end="")
        cls.rows = {}
        for line in out.splitlines():
            name, *kv = line.split()
            cls.rows[name] = {k: int(v) for k, v in (x.split("=") for x in kv)}

    def setUp(self):
        if self.err:
            self.skipTest(self.err)

    def test_G1_gateway_with_thermostat_sends_no_own_status(self):
        r = self.rows["G1"]
        self.assertEqual(r["own_status"], 0, "gateway mode with a thermostat: the gateway sent MsgID 0 of its own")
        self.assertEqual(r["ch_changes"], 0, "the boiler's CH demand toggled between the two masters")
        self.assertEqual(r["last_ch"], 0, "the boiler must follow the thermostat's CH off")

    def test_G1_other_schedule_entries_still_run(self):
        # Hotfix scope (Robert 2026-11-07): only MsgID 0 stops; the PIC slot model is a follow-up task.
        self.assertGreater(self.rows["G1"]["own_other"], 0)

    def test_G2_gateway_without_thermostat_keeps_own_status(self):
        r = self.rows["G2"]
        self.assertGreater(r["own_status"], 300, "without a thermostat the gateway is the only master and must send MsgID 0")

    def test_G3_master_mode_status_has_ch_off(self):
        r = self.rows["G3"]
        self.assertGreater(r["own_status"], 300)
        self.assertEqual(r["own_hb"] & 1, 0, "TASK-1684: master mode without SAT or CS override sends CH off")


class SourcePatterns(unittest.TestCase):
    def test_monitor_mode_never_runs_the_scheduler(self):
        # PIC monitor mode transmits nothing of its own (gateway.asm:1488-1489, 1793).
        loop = function(SRC, r"^void loopOTDirect\(\)\s*\{")
        self.assertRegex(loop, r"DUE\(timerOTSchedule\)\s*&&\s*!IS_MONITOR_MODE\(\)\)\s*\{\s*\n\s*scheduleMasterRequest\(\);")

    def test_monitor_mode_forwards_thermostat_frame_unmodified(self):
        loop = function(SRC, r"^void loopOTDirect\(\)\s*\{")
        i = loop.find("if (IS_MONITOR_MODE())")
        self.assertGreaterEqual(i, 0)
        self.assertIn("sendMasterRequestAsync(otSlaveFrame, OT_DIRECT_ORIGIN_THERMOSTAT)", loop[i:i + 300])


if __name__ == "__main__":
    unittest.main(verbosity=2)
