#!/usr/bin/env python3
"""TASK-1803: OT-Direct gateway mode carries OT=, SW= and SH= on the thermostat's own frames, as the PIC 6.8.

With a thermostat whose every Data-ID the boiler supports there is no alternative slot, so the gateway's own WRITEs
never go out. The PIC (gateway.asm 6.8, repo 770aa45) rewrites the thermostat's frames instead:
  - MessageID27 / UserOutsideTemp :3071-3088: with an OT= value set, a thermostat READ 27 goes to the boiler as
    WRITE 27 with that value and the thermostat gets READ_ACK with the value;
  - MessageID56 / MessageID57 :3135-3148, :3171-3184: a thermostat READ 56/57 becomes one WRITE of the SW=/SH= value
    after the command (INITSW / INITSH), set only when MsgID 6 allows the write (HotWaterTempOK :4181, HeatingTempOK
    :4208: dhwupdate = ID6 LB bit 0, maxchupdate = bit 1).

The code under test is REAL, cut from src/OTGW-firmware/OTDirect.ino: applyOverrides() with the otOverrides table and
setOTParityBit(), buildOverriddenReply(), buildOTResponse(), the "// --- otdRelayRw" block, and the OT=, SW= and SH=
branches of handleOTDirectCommand() with the otdPicFloat block and otdPicInBounds(); the command doubles come from
tests/test_otd_pic_float.py.

Old-vs-fix: git show <old-rev>:src/OTGW-firmware/OTDirect.ino > $TMP/OTDirect.ino
            OTGW_OTD_INO=$TMP/OTDirect.ino python tests/test_otd_relay_rw.py
Run: python tests/test_otd_relay_rw.py
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
import test_otd_pic_float as pf  # noqa: E402  (shares OTGW_OTD_INO and the command doubles)

SRC = pf.SRC


def block(name):
    m = re.search(r"// --- %s begin\n(.*?)// --- %s end" % (name, name), SRC, re.S)
    return m.group(1) if m else None


EXTRA = r"""
// OTDirectMode and otCurrentMode (gateway) come from test_otd_pic_float.PRE (TASK-1751)
static unsigned long otdRelayedStatusFrame(unsigned long f) { return f; }
static uint16_t otdStatusReplyData(uint16_t t, uint16_t) { return t; }
static bool otdRelayRead99(unsigned long, unsigned long &) { return false; }   // TASK-1906, nothing pending here
static uint16_t otdOperModeData(bool) { return 0; }
static bool otPushManual = false;
static unsigned long buildOTResponse(uint8_t type, uint8_t msgId, uint16_t data);
"""

MAIN = r"""
#ifndef HAVE_RELAYRW
static bool otdRelayRewriteRead(unsigned long, unsigned long &) { return false; }
#endif
static unsigned long strip(unsigned long f) { return f & 0x7FFFFFFFUL; }
static bool cmd(const char* buf, const char* value) {
  const char cmd0 = buf[0], cmd1 = buf[1]; char rspBuf[16];
  if (false) {}
  @BRANCHES@
  return true;
}
static void relay(const char* tag, unsigned long thermostatReq, unsigned long boilerResp) {
  bool mod = false;
  unsigned long out = applyOverrides(thermostatReq, mod);
  unsigned long reply = mod ? buildOverriddenReply(thermostatReq, out, boilerResp) : boilerResp;
  printf("%s mod=%d out=%08lX reply=%08lX\n", tag, mod ? 1 : 0, strip(out), strip(reply));
}
int main(int argc, char** argv) {
  const char* t = argv[1];
  if (!strcmp(t, "ot")) {
    relay("before", 0x001B0000UL, 0x401B0000UL);
    cmd("OT", "-1.4");
    relay("set", 0x001B0000UL, 0x501BFE9AUL);     // boiler WRITE_ACK 27
    relay("again", 0x001B0000UL, 0x501BFE9AUL);   // every read, not once
    cmd("OT", "-");
    relay("cleared", 0x001B0000UL, 0x401B0000UL);
  } else if (!strcmp(t, "sw")) {                  // MsgID 6 allows both writes (LB bits 0 and 1)
    otBoilerCache[6] = 0x0303; otBoilerCacheValid[6] = true;
    cmd("SW", "55.7");
    relay("first", 0x00380000UL, 0x503837B4UL);   // boiler WRITE_ACK 56
    relay("second", 0x00380000UL, 0x40383200UL);  // once only: the next read passes
    cmd("SH", "60");
    relay("sh", 0x00390000UL, 0x50393C00UL);
  } else if (!strcmp(t, "nogate")) {              // MsgID 6 does not allow the DHW write
    otBoilerCache[6] = 0x0300; otBoilerCacheValid[6] = true;
    cmd("SW", "55.7");
    relay("read", 0x00380000UL, 0x40383200UL);
  }
  return 0;
}
"""

_tmp = None
_exe = None


def setUpModule():
    global _tmp, _exe
    table = re.search(r"^struct OTFrameOverride \{.*?^static constexpr uint8_t OT_OVERRIDE_COUNT[^\n]*\n", SRC, re.S | re.M)
    rw = block("otdRelayRw")
    helpers = block("otdPicFloat") or ""
    if "static bool otdPicInBounds(" in SRC:
        helpers += "\n" + pf.braced(SRC, r"^static bool otdPicInBounds\(")
    parts = [pf.braced(SRC, r"^static inline void setOTParityBit\(unsigned long &frame\) \{"),
             pf.braced(SRC, r"^static inline uint16_t floatToF88\(float celsius\) \{"), helpers,
             ("#define HAVE_RELAYRW 1\n" + rw) if rw else "",
             table.group(0), pf.braced(SRC, r"^static unsigned long applyOverrides\(unsigned long frame, bool &modified\) \{"),
             pf.braced(SRC, r"^static unsigned long buildOverriddenReply\("),
             pf.braced(SRC, r"^static unsigned long buildOTResponse\(uint8_t type, uint8_t msgId, uint16_t data\) \{")]
    branches = "\n  ".join(pf.braced(SRC, r"else if \(cmd0 == '%s' && cmd1 == '%s'\) \{" % c)
                           for c in (("O", "T"), ("S", "W"), ("S", "H")))
    src = pf.PRE + EXTRA + "\n".join(parts) + MAIN.replace("@BRANCHES@", branches)
    _tmp = Path(tempfile.mkdtemp(prefix="otd_rw_"))
    cpp = _tmp / "t.cpp"
    cpp.write_text(src, encoding="utf-8")
    _exe = compile_cpp(cpp, _tmp / ("t.exe" if os.name == "nt" else "t"))
    if _exe is None:
        raise unittest.SkipTest("no C++ compiler")


def tearDownModule():
    if _tmp:
        shutil.rmtree(_tmp, ignore_errors=True)


def run(t):
    return subprocess.run([str(_exe), t], capture_output=True, text=True, check=True).stdout.splitlines()


class RelayRewrite(unittest.TestCase):
    def test_ac1_ot_rides_on_every_thermostat_read_27(self):
        out = run("ot")
        self.assertEqual(out[0], "before mod=0 out=001B0000 reply=401B0000")
        self.assertEqual(out[1], "set mod=1 out=101BFE9A reply=401BFE9A")   # WRITE 27 -1.40, READ_ACK -1.40 back
        self.assertEqual(out[2], "again mod=1 out=101BFE9A reply=401BFE9A")
        self.assertEqual(out[3], "cleared mod=0 out=001B0000 reply=401B0000")

    def test_ac2_sw_sh_rewrite_the_next_read_once(self):
        out = run("sw")
        self.assertEqual(out[0], "first mod=1 out=103837B4 reply=403837B4")
        self.assertEqual(out[1], "second mod=0 out=00380000 reply=40383200")
        self.assertEqual(out[2], "sh mod=1 out=10393C00 reply=40393C00")

    def test_ac2_msgid6_gate(self):
        self.assertEqual(run("nogate"), ["read mod=0 out=00380000 reply=40383200"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
