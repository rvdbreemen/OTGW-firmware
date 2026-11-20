#!/usr/bin/env python3
"""TASK-1906: OT-Direct gateway mode handles the thermostat's own MsgID 99 as the PIC 6.8 (MessageID99).

PIC ground truth (OTGW-other-projects/otgw-6.8/gateway.asm, repo 770aa45): ID99ReadData :3252-3256 (a thermostat
READ 99 becomes a WRITE with the gateway's operating modes while opermodewrite is set), OperModeAck/OperModeData
:3245-3250 (the thermostat gets the ack with those modes), ID99WriteData :3268-3279 (a thermostat WRITE 99 sets the
modes; its push bit starts a push), ID99ReadAck :3258-3265 (the push bit is set while the gateway spoofs a push),
OperModeBoiler :3239 (a WRITE_ACK clears opermodewrite), PushOperModes :3281-3288.

The code under test is REAL, cut from src/OTGW-firmware/OTDirect.ino: applyOverrides() with the otOverrides table and
setOTParityBit(), buildOverriddenReply(), buildOTResponse(), the operating mode state and otdOperModeData(), the
otdPush and otdOper99 blocks, isUnknownId(), and the MW= branch of handleOTDirectCommand().

Old-vs-fix: git show <old-rev>:src/OTGW-firmware/OTDirect.ino > $TMP/OTDirect.ino
            OTGW_OTD_INO=$TMP/OTDirect.ino python tests/test_otd_msgid99_relay.py
Run: python tests/test_otd_msgid99_relay.py
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
import otd_pic_decimal_block as opd  # noqa: E402  (TASK-1804 B8/B9)
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402

OTD_INO = Path(os.environ.get("OTGW_OTD_INO", REPO / "src" / "OTGW-firmware" / "OTDirect.ino"))
SRC = OTD_INO.read_text(encoding="utf-8").replace("\r\n", "\n")


def braced(src, start_pat):
    m = re.search(start_pat, src, re.M)
    if not m:
        raise AssertionError("not found: " + start_pat)
    i, depth = src.index("{", m.start()), 0
    for j in range(i, len(src)):
        if src[j] == "{":
            depth += 1
        elif src[j] == "}":
            depth -= 1
            if depth == 0:
                return src[m.start():j + 1]
    raise AssertionError("unbalanced")


def block(name):
    m = re.search(r"// --- %s begin\n(.*?)// --- %s end" % (name, name), SRC, re.S)
    return m.group(1) if m else None


PRE = r"""
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#define PSTR(s) (s)
#define snprintf_P snprintf
#define OTDDebugTf(...) do {} while (0)
enum class OpenThermMessageType : uint8_t { READ_DATA = 0, WRITE_DATA = 1 };
typedef uint8_t OpenThermMessageID;
struct OpenTherm { static unsigned long buildRequest(OpenThermMessageType t, OpenThermMessageID id, uint16_t d) {
  return ((unsigned long)t << 28) | ((unsigned long)id << 16) | d; } };
enum OTDirectMode { OTD_MODE_BYPASS = 0, OTD_MODE_GATEWAY = 1 };
static OTDirectMode otCurrentMode = OTD_MODE_GATEWAY;
static unsigned long otdRelayedStatusFrame(unsigned long f) { return f; }
static uint16_t otdStatusReplyData(uint16_t t, uint16_t) { return t; }
static uint32_t g_enq = 0;
static bool otCmdEnqueue(unsigned long f) { g_enq = (uint32_t)f; return true; }
static bool otdWriteRefused = false;   // TASK-1804 B3e: set on a refused enqueue (never here: the stub accepts)
static char g_rsp[32];
static void synthesizeResponse(const char*, const char* r) { snprintf(g_rsp, sizeof g_rsp, "%s", r); }
static void otDirectBridgeProcessStatus(const char*) {}
static constexpr uint8_t OT_UNKNOWN_ID_MAX = 8;
static uint8_t otUnknownIds[OT_UNKNOWN_ID_MAX]; static uint8_t otUnknownIdCount = 0;
static unsigned long buildOTResponse(uint8_t type, uint8_t msgId, uint16_t data);
static bool otdRelayRewriteRead(unsigned long, unsigned long &) { return false; }   // TASK-1803, tests/test_otd_relay_rw.py
static bool otdId99Unsupported();   // TASK-1804 ID-99: defined in the otdPush block, used by the otdOper99 block
"""

MAIN = r"""
#ifndef HAVE_OPER99
static void otdOnThermostatWrite99(uint16_t) {}
static unsigned long otdReply99(unsigned long r) { return r; }
static bool otOperModeWrite = false;
#endif
static unsigned long strip(unsigned long f) { return f & 0x7FFFFFFFUL; }
static bool cmd(const char* buf, const char* value) {
  const char cmd0 = buf[0], cmd1 = buf[1]; char rspBuf[16];
  if (false) {}
  @MW@
  return true;
}
int main(int argc, char** argv) {
  const char* t = argv[1];
  if (!strcmp(t, "relay")) {            // MW=5 pending, then the thermostat reads 99
    cmd("MW", "5");
    bool mod = false;
    unsigned long out = applyOverrides(0x00630000UL, mod);
    unsigned long ack = buildOverriddenReply(0x00630000UL, out, 0x50630500UL);   // boiler WRITE_ACK 99
    printf("mod=%d out=%08lX ack=%08lX\n", mod ? 1 : 0, strip(out), strip(ack));
    otdPushOnBoiler99(5, 0x0500);         // the WRITE_ACK clears the pending write
    mod = false; out = applyOverrides(0x00630000UL, mod);
    printf("after_ack mod=%d out=%08lX\n", mod ? 1 : 0, strip(out));
  } else if (!strcmp(t, "nowrite")) {   // nothing pending: the read goes as it came
    bool mod = false; unsigned long out = applyOverrides(0x00630000UL, mod);
    printf("mod=%d out=%08lX\n", mod ? 1 : 0, strip(out));
  } else if (!strcmp(t, "twrite")) {    // the thermostat writes modes, then MW=... keeps the rest
    otdOnThermostatWrite99(0x0321);       // DHW 3, HC1 1, HC2 2
    cmd("MH", "4");
    printf("modes=%04X\n", otdOperModeData(false));
    otdOnThermostatWrite99(0x1300);       // push bit
    printf("push=%d pending=%d\n", otPushManual ? 1 : 0, otPushPending ? 1 : 0);
  } else if (!strcmp(t, "mh_ui99") || !strcmp(t, "mh_strike99") || !strcmp(t, "mh_ok")) {   // TASK-1804 ID-99
    if (!strcmp(t, "mh_ui99")) otUnknownIds[otUnknownIdCount++] = 99;            // UI=99
    if (!strcmp(t, "mh_strike99")) otUnknownCounters[99 >> 2] |= (uint8_t)(3 << ((99 & 3) * 2));   // three Unknown-DataID
    g_enq = 0; g_rsp[0] = 0;
    const bool ok = cmd("MH", "5");
    printf("ok=%d enq=%08lX write=%d rsp=%s modes=%04X\n", ok ? 1 : 0, (unsigned long)g_enq, otOperModeWrite ? 1 : 0, g_rsp,
           otdOperModeData(false));
  } else if (!strcmp(t, "hwp_strike99")) {   // TASK-1804 ID-99: HW=P with ID 99 struck out three times spoofs at once
    otUnknownCounters[99 >> 2] |= (uint8_t)(3 << ((99 & 3) * 2));
    g_enq = 0; otdPushStart();
    printf("spoof=%d pending=%d enq=%08lX\n", otPushSpoof ? 1 : 0, otPushPending ? 1 : 0, (unsigned long)g_enq);
  } else if (!strcmp(t, "spoof")) {     // the gateway spoofs a push; the boiler's READ_ACK 99 has no push bit
    otUnknownIds[otUnknownIdCount++] = 99; otdPushStart();
    unsigned long r = otdReply99(0x40630200UL);
    printf("spoof=%d reply=%08lX\n", otPushSpoof ? 1 : 0, strip(r));
    otdPushEnd(); r = otdReply99(0x40630200UL);
    printf("after reply=%08lX\n", strip(r));
  }
  return 0;
}
"""

_tmp = None
_exe = None


def unknown_counts():
    """TASK-1804 ID-99: the real three-strike counter (otUnknownCounters + getUnknownCount)."""
    a = SRC.index("static uint8_t otUnknownCounters[32]")
    return SRC[a:SRC.index("\n}\n", SRC.index("static uint8_t getUnknownCount(uint8_t msgId) {", a)) + 3]


def setUpModule():
    global _tmp, _exe
    table = re.search(r"^struct OTFrameOverride \{.*?^static constexpr uint8_t OT_OVERRIDE_COUNT[^\n]*\n", SRC, re.S | re.M)
    pushflags = SRC[SRC.index("static bool    otPushManual"):SRC.index("\n", SRC.index("static uint8_t otPushFails")) + 1]
    modes = SRC[SRC.index("static uint8_t otOperModeDHW"):]
    modes = modes[:modes.index("\n}\n", modes.index("static uint16_t otdOperModeData(bool push) {")) + 3]
    oper = block("otdOper99")
    parts = [pushflags, braced(SRC, r"^static inline void setOTParityBit\(unsigned long &frame\) \{"),
             braced(SRC, r"^static bool isUnknownId\(uint8_t msgId\) \{"),
             unknown_counts(), modes,
             ("#define HAVE_OPER99 1\n" + oper) if oper else "",
             block("otdPush") or "",
             "static unsigned long otdRelayedStatusFrameDummy;",
             table.group(0), braced(SRC, r"^static unsigned long applyOverrides\(unsigned long frame, bool &modified\) \{"),
             braced(SRC, r"^static unsigned long buildOverriddenReply\("),
             braced(SRC, r"^static unsigned long buildOTResponse\(uint8_t type, uint8_t msgId, uint16_t data\) \{")]
    mw = "\n  ".join(braced(SRC, r"else if \(cmd0 == 'M' && cmd1 == '%s'\) \{" % c) for c in ("W", "H"))
    _tmp = Path(tempfile.mkdtemp(prefix="otd_99_"))
    cpp = _tmp / "t.cpp"
    cpp.write_text(opd.with_block(PRE) + "\n".join(parts) + MAIN.replace("@MW@", mw), encoding="utf-8")
    _exe = compile_cpp(cpp, _tmp / ("t.exe" if os.name == "nt" else "t"))
    if _exe is None:
        raise unittest.SkipTest("no C++ compiler")


def tearDownModule():
    if _tmp:
        shutil.rmtree(_tmp, ignore_errors=True)


def run(t):
    return subprocess.run([str(_exe), t], capture_output=True, text=True, check=True).stdout.splitlines()


class MsgId99Unsupported(unittest.TestCase):
    """TASK-1804 ID-99: PushOperModes (gateway.asm 6.8 :3281-3288) writes MsgID 99 only when the boiler is not known to
    reject it; the PIC unknown map holds the UI= marks and the three-strike blacklist (UnknownMask :3519-3538). The nibble
    is stored and echoed either way (SetOperModeHC1 :4424-4436). Old code: MH= always sent the write, and HW=P checked
    the UI= table only, so mh_ui99, mh_strike99 and hwp_strike99 are red on it."""

    def test_mh_with_99_marked_unknown_stores_and_echoes_without_a_write(self):
        self.assertEqual(run("mh_ui99"), ["ok=1 enq=00000000 write=0 rsp=5 modes=0005"])

    def test_mh_with_99_struck_out_three_times_stores_and_echoes_without_a_write(self):
        self.assertEqual(run("mh_strike99"), ["ok=1 enq=00000000 write=0 rsp=5 modes=0005"])

    def test_mh_with_99_supported_writes_99(self):
        self.assertEqual(run("mh_ok"), ["ok=1 enq=10630005 write=1 rsp=5 modes=0005"])

    def test_hw_p_with_99_struck_out_spoofs_without_a_write(self):
        self.assertEqual(run("hwp_strike99"), ["spoof=1 pending=0 enq=00000000"])


class MsgId99(unittest.TestCase):
    def test_ac1_a_thermostat_read_99_carries_the_pending_mode_write(self):
        out = run("relay")
        # WRITE_DATA 99 with DHW mode 5 (HB 0x05), the thermostat gets READ_ACK 99 with those modes
        self.assertEqual(out[0], "mod=1 out=10630500 ack=40630500")
        self.assertEqual(out[1], "after_ack mod=0 out=00630000", "the WRITE_ACK clears the pending write")

    def test_without_a_pending_write_the_read_passes(self):
        self.assertEqual(run("nowrite"), ["mod=0 out=00630000"])

    def test_ac2_a_thermostat_write_99_sets_the_stored_modes(self):
        out = run("twrite")
        self.assertEqual(out[0], "modes=0324", "DHW 3 and HC2 2 from the thermostat (LB 0x21), HC1 4 from MH=")
        self.assertEqual(out[1], "push=1 pending=1", "a push bit in the thermostat's write starts a push")

    def test_ac3_the_read_ack_has_the_push_bit_while_the_gateway_spoofs(self):
        out = run("spoof")
        self.assertEqual(out[0], "spoof=1 reply=40631000")
        self.assertEqual(out[1], "after reply=40630200")


class Wiring(unittest.TestCase):
    def test_thermostat_write_and_reply_hooks(self):
        loop = braced(SRC, r"^void loopOTDirect\(\) \{")
        self.assertIn("if (slaveMsgId == 99 && ((otSlaveFrame >> 28) & 0x07) == 1) otdOnThermostatWrite99(", loop)
        reply = braced(SRC, r"^static void replyToThermostat\(")
        self.assertIn("reply = otdReply99(reply);", reply)
        for c in ("H", "W", "2"):   # TASK-1804 ID-99: the branches write 99 through otdPushOperModes(), which sets the flag
            b = braced(SRC, r"else if \(cmd0 == 'M' && cmd1 == '%s'\) \{" % c)
            self.assertIn("if (!otdPushOperModes())", b, "M" + c)
        self.assertIn("otOperModeWrite = true;", block("otdPushOperModes"))


if __name__ == "__main__":
    unittest.main(verbosity=2)
