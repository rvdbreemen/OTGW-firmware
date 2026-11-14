#!/usr/bin/env python3
"""TASK-1839 AC2/AC3: a real switch between gateway and monitor keeps the command state, as on the PIC.

Robert, DM 1570230662743592961 (answer A to 1570229987825418384): like the PIC, a real mode change (monitor <-> gateway)
keeps everything; only the queue is flushed (ADR-223); a CS= stays valid until it expires. The PIC's SetGatewayMode only
switches the comparators (gateway.asm 6.8 :4729-4774). The PIC has no master, loopback or bypass mode: a switch into or
out of one of those keeps the full reset (outside the question asked).

The real otdFlushCmdQueue(), resetTransientState(), applyOTDirectMode() and setOTDirectMode() are cut from
src/OTGW-firmware/OTDirect.ino and compiled with a host C++ compiler, on stubbed globals of the real types. The harness
sets HW=0 (otDHWOverride 0, master status bit 1 clear), an SR= entry (60 = 258), a CS= override (40.0) with its write
cache, then runs GW= switches or GW=R. OTGW_OTD_INO points at another copy for old-vs-fix.
Run: python tests/test_otd_mode_change_keeps_state.py
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
SRC = OTD_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
TYPES = (REPO / "src" / "OTGW-firmware" / "OTDirecttypes.h").read_text(encoding="utf-8").replace("\r\n", "\n")


def func(src, header):
    i = src.find(header)
    if i < 0:
        return ""
    return src[i:src.find("\n}\n", i) + 3]


def struct(src, name):
    m = re.search(r"struct %s \{.*?\n\};\n" % name, src, re.S)
    return m.group(0) if m else ""


PRE = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define F(s) (s)
#define PSTR(s) (s)
#define HAS_BYPASS_RELAY 0
#define OTDDebugTln(s) ((void)0)
#define snprintf_P snprintf
#if defined(_MSC_VER)   // the GCC atomic builtins the firmware uses; single-threaded here
#define __ATOMIC_RELAXED 0
#define __ATOMIC_RELEASE 3
template <class T, class V> static void __atomic_store_n(T* p, V v, int) { *p = (T)v; }
#endif
enum OTDirectMode { OTD_MODE_BYPASS = 0, OTD_MODE_GATEWAY = 1, OTD_MODE_MONITOR = 2, OTD_MODE_MASTER = 3, OTD_MODE_LOOPBACK = 4 };
enum OTRemoteOverrideMode { OT_OVERRIDE_NONE = 0, OT_OVERRIDE_TEMPORARY = 1, OT_OVERRIDE_CONSTANT = 2 };
@REMOTE@
static OTDirectMode otCurrentMode = OTD_MODE_GATEWAY;
static uint8_t otCmdHead = 7, otCmdFlushHead = 0; static bool otCmdFlushPending = false;
static uint8_t otOperModeDHW = 0, otOperModeCH1 = 0, otOperModeCH2 = 0;
static bool otThermostatSeen = true; static uint32_t otLastThermostatMs = 1234;
static bool otDHWBlocking = false, otCoolingEnable = true, otCH2Enable = true, otChModeOff = false;
static uint8_t otMasterStatusFlags = 0x03, otDHWOverride = 0xFF;
static uint8_t otUnknownCounters[32] = {0}; static uint8_t otUnknownIdCount = 0;
struct OTScheduleEntry { uint8_t msgId; uint32_t intervalMs; uint32_t lastSentMs; bool disabled; bool isWrite; bool valueSet; uint16_t cachedValue; };
static OTScheduleEntry otSchedule[] = { { 0, 1000, 5, false, false, false, 0 }, { 1, 1000, 5, false, true, false, 0 } };
static constexpr uint8_t OT_SCHEDULE_SIZE = sizeof(otSchedule) / sizeof(otSchedule[0]);
struct OTResponseOverride { uint8_t msgId; bool active; uint16_t value; };
static constexpr uint8_t OT_RESPONSE_OVERRIDE_MAX = 16;
static OTResponseOverride otResponseOverrides[OT_RESPONSE_OVERRIDE_MAX];
struct OTFrameOverride { uint8_t msgId; bool active; uint16_t overrideValue; };
static OTFrameOverride otOverrides[] = { { 1, false, 0 }, { 7, false, 0 } };
static constexpr uint8_t OT_OVERRIDE_COUNT = sizeof(otOverrides) / sizeof(otOverrides[0]);
static OTRemoteOverrideState otRemoteOverride = {};
static bool otRemoteOverrideClearPending = false;
static bool otSetbackEngaged = false; static float otFakeRoomSetpoint = 0.0f;
struct OtdState { int eOverrideMode; uint16_t iOverrideF88; bool bSetbackActive; };
struct State { OtdState otd; }; static State state = {};
struct OtdSettings { bool bEnableSlave; bool bSummerMode; };
struct Settings { OtdSettings otd; }; static Settings settings = { { true, false } };
static void otdPushEnd() {}
static void clearAllResponseModifiers() {}
static void sendWebSocketJSON(const char*) {}
static bool updateSetting(const char*, const char*) { return true; }
struct MockOT { void begin(void (*)(), void (*)(unsigned long, int)) {} void end() {} };
static MockOT otSlave;
static void slaveISR() {}
static void handleSlaveRequest(unsigned long, int) {}
"""

MAIN = r"""
int main(int argc, char** argv) {
  // HW=0, SR=60:1,2, CS=40 (override + write cache) in gateway mode
  otDHWOverride = 0; otMasterStatusFlags &= ~0x02;
  otResponseOverrides[0] = { 60, true, 258 };
  otOverrides[0].active = true; otOverrides[0].overrideValue = 40 * 256;
  otSchedule[1].valueSet = true; otSchedule[1].cachedValue = 40 * 256;
  for (int i = 1; i < argc; i++) {
    if (argv[i][0] == 'R') resetTransientState();               // GW=R
    else setOTDirectMode((OTDirectMode)atoi(argv[i]));
  }
  printf("hw=%d bit1=%d sr=%d cs=%d cache=%d flush=%d mode=%d\n", (int)otDHWOverride, (otMasterStatusFlags >> 1) & 1,
         (int)otResponseOverrides[0].active, (int)otOverrides[0].active, (int)otSchedule[1].valueSet,
         (int)otCmdFlushPending, (int)otCurrentMode);
  return 0;
}
"""


class ModeChangeKeepsState(unittest.TestCase):
    exe = None
    err = None
    tmp = None

    @classmethod
    def setUpClass(cls):
        names = ("static void otdFlushCmdQueue() {", "static void resetTransientState() {",
                 "static void applyOTDirectMode(OTDirectMode newMode) {", "static void setOTDirectMode(OTDirectMode newMode) {")
        parts = [func(SRC, h) for h in names]
        if not all(parts[1:]):
            cls.err = "resetTransientState / applyOTDirectMode / setOTDirectMode not found"
            return
        remote = struct(TYPES, "OTRemoteOverrideState")
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text(PRE.replace("@REMOTE@", remote) + "\n".join(p for p in parts if p) + MAIN, encoding="utf-8")
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

    def run_seq(self, *steps):
        out = subprocess.run([str(self.exe)] + [str(s) for s in steps], capture_output=True, text=True, check=True).stdout
        return dict(kv.split("=") for kv in out.split())

    def test_gw0_then_gw1_keeps_hw0_sr_and_cs(self):
        # AC3: gateway -> monitor -> gateway. Old code: everything cleared (hw back to auto 255, SR and CS gone).
        r = self.run_seq(2, 1)
        self.assertEqual((r["hw"], r["bit1"]), ("0", "0"), r)          # HW=0 stays
        self.assertEqual(r["sr"], "1", r)                               # SR=60 stays
        self.assertEqual((r["cs"], r["cache"]), ("1", "1"), r)          # CS=40 stays (until its own expiry)
        self.assertEqual(r["flush"], "1", "the queue is flushed at the switch (ADR-223)")
        self.assertEqual(r["mode"], "1")

    def test_gw0_alone_keeps_the_state_in_monitor_mode(self):
        r = self.run_seq(2)
        self.assertEqual((r["hw"], r["sr"], r["cs"], r["flush"], r["mode"]), ("0", "1", "1", "1", "2"), r)

    def test_a_switch_into_master_still_clears(self):
        # outside the question asked: the PIC has no master mode
        r = self.run_seq(3)
        self.assertEqual((r["hw"], r["sr"], r["cs"], r["cache"]), ("255", "0", "0", "0"), r)
        self.assertEqual(r["flush"], "1", r)

    def test_gw_r_still_clears_and_flushes(self):
        r = self.run_seq("R")
        self.assertEqual((r["hw"], r["sr"], r["cs"], r["cache"], r["flush"]), ("255", "0", "0", "0", "1"), r)

    # --- source: the kept state is not applied in monitor mode, and the CS expiry clock runs in every mode ---
    def test_monitor_mode_relays_raw_and_the_reply_modifiers_are_gateway_only(self):
        loop = func(SRC, "void loopOTDirect() {")
        mon = loop[loop.index("if (IS_MONITOR_MODE()) {"):]
        mon = mon[:mon.index("else {")]
        self.assertIn("sendMasterRequestAsync(otSlaveFrame, OT_DIRECT_ORIGIN_THERMOSTAT)", mon)
        self.assertIn("if (DUE(timerOTSchedule) && !IS_MONITOR_MODE()) {", loop)   # no own frames in monitor mode
        reply = func(SRC, "static void replyToThermostat(OTDirectRequestOrigin origin, unsigned long sentReq,")
        self.assertIn("} else if (otCurrentMode == OTD_MODE_GATEWAY) {\n    reply = applyResponseModifiers(boilerResp);", reply)

    def test_cs_expiry_runs_outside_the_monitor_gate(self):
        loop = func(SRC, "void loopOTDirect() {")
        i_exp = loop.index("OTD: CS heartbeat expired")
        i_sec = loop.rindex("if (DUE(timerOTDStatus)) {", 0, i_exp)
        self.assertNotIn("IS_MONITOR_MODE", loop[i_sec:i_exp])


if __name__ == "__main__":
    unittest.main(verbosity=2)
