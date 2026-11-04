#!/usr/bin/env python3
"""TASK-1600 / ADR-208: host test for the CH override on the relayed thermostat MsgID 0.

The real functions setOTParityBit, otdEffectiveMasterFlags, the otdRelayedStatus block,
applyOverrides, buildOTResponse and buildOverriddenReply are cut from
src/OTGW-firmware/OTDirect.ino and compiled with a host C++ compiler; only the globals and
logging they touch are stubbed. OTGW_OTD_INO points at another copy (e.g. `git show HEAD:...`)
for old-vs-fix runs. Without a compiler the compiled cases are skipped.

Run: python tests/test_otd_status_relay.py
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


def func(header):
    i = SRC.find(header)
    if i < 0:
        return ""
    return SRC[i:SRC.find("\n}\n", i) + 3]


def block(name):
    m = re.search(r"// --- %s begin\n(.*?)// --- %s end" % (name, name), SRC, re.S)
    return m.group(1) if m else ""


PRE = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#define PSTR(x) x
#define OTDDebugTf(...) ((void)0)
enum OTDirectMode { OTD_MODE_MONITOR = 0, OTD_MODE_GATEWAY = 1, OTD_MODE_BYPASS = 2 };
static OTDirectMode otCurrentMode = OTD_MODE_GATEWAY;
static bool otChModeOff = false;
struct Ov { uint8_t msgId; bool active; uint16_t overrideValue; };
#define OT_OVERRIDE_COUNT 2
static Ov otOverrides[OT_OVERRIDE_COUNT] = { {1, false, 0}, {9, false, 0} };
static void onThermostatMsgID16(uint8_t, uint16_t) {}
static unsigned long buildOTResponse(uint8_t type, uint8_t msgId, uint16_t data);
static unsigned long otdRelayedStatusFrame(unsigned long frame);
static uint16_t otdStatusReplyData(uint16_t thermostatData, uint16_t boilerData);
static bool otdCsOverrideActive() { return otOverrides[0].active; }
"""

MAIN = r"""
static unsigned long mk(unsigned t, unsigned id, unsigned data) {
  unsigned long f = ((unsigned long)t << 28) | ((unsigned long)id << 16) | data;
  setOTParityBit(f);
  return f;
}
static int parityOk(unsigned long f) { int n = 0; for (int i = 0; i < 32; i++) n += (f >> i) & 1; return n % 2 == 0; }
int main(int argc, char** argv) {
  // argv: cs chOff mode msgId type data boilerData boilerType
  otOverrides[0].active = atoi(argv[1]) != 0;
  otChModeOff = atoi(argv[2]) != 0;
  otCurrentMode = (OTDirectMode)atoi(argv[3]);
  unsigned id = atoi(argv[4]), type = atoi(argv[5]);
  unsigned data = strtoul(argv[6], 0, 16);
  unsigned long req = mk(type, id, data);
  unsigned long boiler = mk(atoi(argv[8]), id, strtoul(argv[7], 0, 16));
  bool modified;
  unsigned long sent = applyOverrides(req, modified);
  unsigned long reply = buildOverriddenReply(req, sent, boiler);
  printf("%08lX %d %d %08lX %d %08lX\n", sent, modified ? 1 : 0, parityOk(sent), reply, parityOk(reply), boiler);
  return 0;
}
"""


class Compiled(unittest.TestCase):
    exe = None
    err = None
    tmp = None

    @classmethod
    def setUpClass(cls):
        parts = [func("static inline void setOTParityBit"), block("otdEffectiveMasterFlags"),
                 block("otdRelayedStatus"), func("static unsigned long applyOverrides"),
                 func("static unsigned long buildOTResponse(uint8_t type, uint8_t msgId, uint16_t data) {"),
                 func("static unsigned long buildOverriddenReply")]
        # HEAD (old code) has no otdRelayedStatus block: stub the two helpers so the old applyOverrides compiles
        if not parts[2]:
            parts[2] = ("static unsigned long otdRelayedStatusFrame(unsigned long f) { return f; }\n"
                        "static uint16_t otdStatusReplyData(uint16_t t, uint16_t b) { return b; }\n")
        if not all(parts):
            cls.err = "function or marker block missing: %s" % [i for i, p in enumerate(parts) if not p]
            return
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text(PRE + "\n".join(parts) + MAIN, encoding="utf-8")
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

    def run_case(self, cs, ch_off, mode, msg_id, mtype, data, boiler=0, boiler_type=4):
        out = subprocess.run([str(self.exe), str(int(cs)), str(int(ch_off)), str(mode), str(msg_id), str(mtype),
                              "%04X" % data, "%04X" % boiler, str(boiler_type)],
                             capture_output=True, text=True, check=True).stdout.split()
        return int(out[0], 16), int(out[1]), int(out[2]), int(out[3], 16), int(out[4]), int(out[5], 16)

    # (a) CS override held: the boiler gets the CH bit of the override, not the thermostat's
    def test_a_cs_held_ch0_clears_bit0_with_valid_parity(self):
        sent, modified, par, _, _, _ = self.run_case(True, True, 1, 0, 0, 0x0300)   # thermostat: CH + DHW
        self.assertEqual(sent & 0xFFFF, 0x0200)          # CH cleared, DHW bit and low byte kept
        self.assertEqual((sent >> 16) & 0x7FFF, 0x0000)  # READ_DATA, MsgID 0 (bit 31 is parity)
        self.assertEqual(modified, 1)
        self.assertEqual(par, 1)

    def test_a_cs_held_ch1_sets_bit0_when_thermostat_sent_ch0(self):
        sent, modified, par, _, _, _ = self.run_case(True, False, 1, 0, 0, 0x0200)
        self.assertEqual(sent & 0xFFFF, 0x0300)
        self.assertEqual((modified, par), (1, 1))

    # (b) the reply: boiler low byte, thermostat's original high byte
    def test_b_reply_has_boiler_low_byte_and_thermostat_high_byte(self):
        # the boiler saw high byte 0x02 and answers slave status 0x0A
        _, _, _, reply, rpar, _ = self.run_case(True, True, 1, 0, 0, 0x0300, 0x020A)
        self.assertEqual(reply & 0xFFFF, 0x030A)          # thermostat's 0x03 + boiler's 0x0A
        self.assertEqual((reply >> 28) & 7, 4)            # READ_ACK
        self.assertEqual((reply >> 16) & 0xFF, 0)
        self.assertEqual(rpar, 1)

    def test_b_non_ack_boiler_answer_passes_back_as_it_came(self):
        # boiler type 7 = UNKNOWN_DATA_ID: the thermostat gets that frame, not a fabricated ACK
        _, _, _, reply, _, boiler = self.run_case(True, True, 1, 0, 0, 0x0300, 0x0000, boiler_type=7)
        self.assertEqual(reply, boiler)

    # (c) no CS override: unchanged, either CH bit
    def test_c_no_cs_override_passes_unchanged(self):
        for ch_off in (False, True):
            for data in (0x0300, 0x0200, 0x0000, 0x0100):
                sent, modified, par, _, _, _ = self.run_case(False, ch_off, 1, 0, 0, data)
                self.assertEqual(sent & 0xFFFF, data)
                self.assertEqual(modified, 0)
                self.assertEqual(par, 1)

    def test_c_matching_bit0_not_marked_modified(self):
        sent, modified, _, _, _, _ = self.run_case(True, False, 1, 0, 0, 0x0300)
        self.assertEqual((sent & 0xFFFF, modified), (0x0300, 0))

    def test_c_monitor_mode_untouched(self):
        sent, modified, _, _, _, _ = self.run_case(True, True, 0, 0, 0, 0x0300)
        self.assertEqual((sent & 0xFFFF, modified), (0x0300, 0))

    # (d) other READ_DATA ids stay unchanged, with and without a CS override
    def test_d_other_read_ids_unchanged(self):
        for mid in (1, 5, 14, 17, 25, 26, 56):
            for cs in (False, True):
                sent, modified, par, _, _, _ = self.run_case(cs, True, 1, mid, 0, 0x0300)
                self.assertEqual(sent & 0xFFFF, 0x0300, "MsgID %d" % mid)
                self.assertEqual(modified, 0)

    def test_d_msgid0_write_data_not_touched(self):
        sent, modified, _, _, _, _ = self.run_case(True, True, 1, 0, 1, 0x0300)
        self.assertEqual((sent & 0xFFFF, modified), (0x0300, 0))


if __name__ == "__main__":
    unittest.main(verbosity=2)
