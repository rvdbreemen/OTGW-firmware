#!/usr/bin/env python3
"""TASK-1805 / ADR-229: host test for bits 1, 2, 4, 5 and 6 of the relayed thermostat MsgID 0.
The PIC rewrites them by its readstatus rule (other-projects/otgw-6.6/gateway.asm:2712-2742): per bit a
mask (is the command in force) and a value. The real setOTParityBit, otdEffectiveMasterFlags and the
otdRelayedStatus block are cut from src/OTGW-firmware/OTDirect.ino and compiled with a host C++ compiler;
only the command state they read is stubbed, with the firmware's names. otdRelayedStatusFrame() is the
entry point applyOverrides() uses for a gateway-mode READ_DATA MsgID 0 (ADR-208 test:
tests/test_otd_status_relay.py). OTGW_OTD_INO points at another copy for old-vs-fix runs.
Run: python tests/test_otd_status_bits.py
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
OTD_TYPES = Path(os.environ.get("OTGW_OTD_TYPES", REPO / "src" / "OTGW-firmware" / "OTDirecttypes.h"))
TYPES = OTD_TYPES.read_text(encoding="utf-8").replace("\r\n", "\n") if OTD_TYPES.exists() else ""


def status_cmd_struct(relay_block):
    """ADR-229: OtdStatusCmd lives in OTDirecttypes.h (the Arduino-generated prototypes name it); an older
    OTDirect.ino has it inline in the relay block, and then nothing is added."""
    if not relay_block or "struct OtdStatusCmd" in relay_block:
        return ""
    m = re.search(r"// --- OtdStatusCmd begin\n(.*?)// --- OtdStatusCmd end", TYPES, re.S)
    return m.group(1) if m else ""


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
struct OTFrameOverride { uint8_t msgId; bool active; uint16_t overrideValue; };
static OTFrameOverride otOverrides[] = { {1, false, 0}, {7, false, 0}, {8, false, 0}, {14, false, 0} };
static constexpr uint8_t OT_OVERRIDE_COUNT = sizeof(otOverrides) / sizeof(otOverrides[0]);
static bool otdCsOverrideActive() {
  for (uint8_t i = 0; i < OT_OVERRIDE_COUNT; i++) if (otOverrides[i].msgId == 1) return otOverrides[i].active;
  return false;
}
enum DHWPushState { PUSH_IDLE, PUSH_PENDING, PUSH_ACTIVE };
static DHWPushState otDHWPushState = PUSH_IDLE;
static uint8_t otDHWOverride = 0xFF;          // HW=: 0, 1, 0xFF = auto
static uint8_t otMasterStatusFlags = 0x01;    // H2= sets bit 4 here
static bool otChModeOff = false;
static bool otSummerMode = false;
static bool otDHWBlocking = false;
static bool otCoolingEnable = false;
"""

MAIN = r"""
static int parityOk(unsigned long f) { int n = 0; for (int i = 0; i < 32; i++) n += (f >> i) & 1; return n % 2 == 0; }
int main(int argc, char** argv) {
  // argv: hb cs chOff hw push cl ce c2 h2 sm bw
  unsigned hb = strtoul(argv[1], 0, 16);
  otOverrides[0].active = atoi(argv[2]) != 0;
  otChModeOff = atoi(argv[3]) != 0;
  otDHWOverride = (uint8_t)atoi(argv[4]);
  otDHWPushState = atoi(argv[5]) ? PUSH_ACTIVE : PUSH_IDLE;
  otOverrides[1].active = atoi(argv[6]) != 0;
  otCoolingEnable = atoi(argv[7]) != 0;
  otOverrides[2].active = atoi(argv[8]) != 0;
  if (atoi(argv[9])) otMasterStatusFlags |= 0x10; else otMasterStatusFlags &= ~0x10;
  otSummerMode = atoi(argv[10]) != 0;
  otDHWBlocking = atoi(argv[11]) != 0;
  unsigned long req = ((unsigned long)hb << 8) | 0x00;   // READ_DATA, MsgID 0, low byte 0
  setOTParityBit(req);
  unsigned long sent = otdRelayedStatusFrame(req);
  printf("%02lX %d\n", (sent >> 8) & 0xFF, parityOk(sent));
  return 0;
}
"""

AUTO = 255


class StatusBits(unittest.TestCase):
    exe = None
    err = None
    tmp = None

    @classmethod
    def setUpClass(cls):
        parts = [func("static inline void setOTParityBit"), block("otdEffectiveMasterFlags"), block("otdRelayedStatus")]
        if not all(parts):
            cls.err = "function or marker block missing: %s" % [i for i, p in enumerate(parts) if not p]
            return
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text(PRE + status_cmd_struct(block("otdRelayedStatus")) + "\n".join(parts) + MAIN, encoding="utf-8")
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

    def hb(self, hb, cs=0, ch_off=0, hw=AUTO, push=0, cl=0, ce=0, c2=0, h2=0, sm=0, bw=0):
        out = subprocess.run([str(self.exe), "%02X" % hb] + [str(int(v)) for v in (cs, ch_off, hw, push, cl, ce, c2, h2, sm, bw)],
                             capture_output=True, text=True, check=True).stdout.split()
        self.assertEqual(out[1], "1", "parity")
        return int(out[0], 16)

    # No command in force: the thermostat's status passes byte for byte (all 256 high bytes).
    def test_no_command_passes_every_high_byte(self):
        for hb in range(256):
            self.assertEqual(self.hb(hb), hb)

    # Bit 1, HW=: HW=0 clears, HW=1 sets, HW=A leaves the thermostat's bit; a DHW push sets it.
    def test_bit1_hw(self):
        self.assertEqual(self.hb(0x03, hw=0), 0x01)
        self.assertEqual(self.hb(0x01, hw=1), 0x03)
        self.assertEqual(self.hb(0x03, hw=AUTO), 0x03)
        self.assertEqual(self.hb(0x01, hw=AUTO), 0x01)
        self.assertEqual(self.hb(0x01, hw=0, push=1), 0x03)

    # Bit 2, CE=: replaced only while a CL= cooling level is held (gateway.asm SysCoolLevel).
    def test_bit2_ce_needs_cl(self):
        self.assertEqual(self.hb(0x00, ce=1, cl=1), 0x04)
        self.assertEqual(self.hb(0x04, ce=0, cl=1), 0x00)
        self.assertEqual(self.hb(0x00, ce=1, cl=0), 0x00)
        self.assertEqual(self.hb(0x04, ce=0, cl=0), 0x04)

    # Bit 4, H2=: replaced only while a C2= CH2 setpoint is held (gateway.asm SysCH2Setpoint).
    def test_bit4_h2_needs_c2(self):
        self.assertEqual(self.hb(0x00, h2=1, c2=1), 0x10)
        self.assertEqual(self.hb(0x10, h2=0, c2=1), 0x00)
        self.assertEqual(self.hb(0x00, h2=1, c2=0), 0x00)
        self.assertEqual(self.hb(0x10, h2=0, c2=0), 0x10)

    # Bit 5, SM=: SM=1 forces 1, SM=0 leaves the thermostat's bit.
    def test_bit5_sm(self):
        self.assertEqual(self.hb(0x00, sm=1), 0x20)
        self.assertEqual(self.hb(0x20, sm=1), 0x20)
        self.assertEqual(self.hb(0x20, sm=0), 0x20)
        self.assertEqual(self.hb(0x00, sm=0), 0x00)

    # Bit 6, BW=: BW=1 forces 1, BW=0 leaves the thermostat's bit.
    def test_bit6_bw(self):
        self.assertEqual(self.hb(0x00, bw=1), 0x40)
        self.assertEqual(self.hb(0x40, bw=0), 0x40)
        self.assertEqual(self.hb(0x00, bw=0), 0x00)

    # Bit 0 stays the ADR-208 rule; bits 3 and 7 are never touched.
    def test_bit0_unchanged_and_bits_3_7_untouched(self):
        self.assertEqual(self.hb(0x01, cs=1, ch_off=1), 0x00)
        self.assertEqual(self.hb(0x00, cs=1, ch_off=0), 0x01)
        self.assertEqual(self.hb(0x01, cs=0, ch_off=1), 0x01)
        self.assertEqual(self.hb(0x88, hw=0, cl=1, ce=0, c2=1, h2=0, sm=1, bw=1), 0xE8)

    # All commands at once on a thermostat status with DHW, cooling and CH2 on.
    def test_all_bits_together(self):
        self.assertEqual(self.hb(0x17, cs=1, ch_off=1, hw=0, cl=1, ce=0, c2=1, h2=0, sm=1, bw=1), 0x60)


if __name__ == "__main__":
    unittest.main(verbosity=2)
