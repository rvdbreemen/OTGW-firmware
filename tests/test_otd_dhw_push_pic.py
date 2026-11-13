#!/usr/bin/env python3
"""TASK-1845: OT-Direct manual DHW push (HW=P) as the PIC 6.8.

Ground truth (OTGW-other-projects/otgw-6.8/gateway.asm):
  - SetHotWater (4392-4414): HW=P calls StartDHWPush and leaves the HW= state (HotWaterSwitch/Enable) alone; any other
    HW= aborts the push (clrf dhwpushflags, bcf manualdhwpush) and sets the HW= state.
  - StartDHWPush / PushOperModes (3270-3288): sets manualdhwpush (operatingmode1 bit 4); if MsgID 99 is known
    unsupported the gateway spoofs the push, else it writes MsgID 99 with the push bit.
  - MessageID99 (3218-3262): Unknown-DataID during a push, or a WRITE_ACK without the push bit to the push request,
    makes the gateway spoof it; otherwise the push bit follows the boiler (OperModeFlag).
  - readstatus (2720-2747): the relayed MsgID 0 DHW enable bit is forced only by HotWaterSwitch (HW=0/1) or dhwpushspoof.
  - statusreturn/dhwpushcheck (2753-2797): only while spoofing: flame on with DHW mode starts the push, 8 status answers
    with the flame off before the start end it, once started it ends when DHW mode clears. No wall-clock limit.
  - tests/dhw-5.stc: with boiler support and HW=0, HW=P sends R10631000 and the status stays R00000000.
OT-Direct before the fix: HW=P set HW=1 (PR=W answered W=1, then A after the push: a previous HW=0 was lost), forced
DHW enable for every push, and cut a started push after 30 s (old OTDirect.ino OT_DHW_PUSH_TIMEOUT_MS, read in
handleMasterResponse; that code path is not cut into this harness, so the 30 s cut is shown red only by reading).

The code under test is REAL: the HW branch of handleOTDirectCommand(), otdOperModeData() and the otdPush block (absent
before the fix), the otOverrides table, otdCsOverrideActive(), the otdEffectiveMasterFlags and otdRelayedStatus blocks
and OtdStatusCmd are cut from src/OTGW-firmware/OTDirect.ino / OTDirecttypes.h.

Old-vs-fix: OTGW_OTD_INO points at another copy of OTDirect.ino, e.g.
  git show <old-rev>:src/OTGW-firmware/OTDirect.ino > $TMP/OTDirect.ino
  OTGW_OTD_INO=$TMP/OTDirect.ino python tests/test_otd_dhw_push_pic.py
Run: python tests/test_otd_dhw_push_pic.py
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402

OTD_INO = Path(os.environ.get("OTGW_OTD_INO", REPO / "src" / "OTGW-firmware" / "OTDirect.ino"))
OTD_TYPES = REPO / "src" / "OTGW-firmware" / "OTDirecttypes.h"

STUBS = r"""
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#define PSTR(s) (s)
#define F(s) (s)
#define snprintf_P snprintf
#define OTDDebugTf(...) do {} while (0)
#define OTDDebugTln(...) do {} while (0)
enum class OpenThermMessageType { READ_DATA = 0, WRITE_DATA = 1 };
typedef uint8_t OpenThermMessageID;
static inline void setOTParityBit(unsigned long &frame) {
  frame &= 0x7FFFFFFFUL; uint32_t p = frame; p ^= (p >> 16); p ^= (p >> 8); p ^= (p >> 4); p ^= (p >> 2); p ^= (p >> 1);
  if (p & 1) frame |= 0x80000000UL;
}
struct OpenTherm {
  static unsigned long buildRequest(OpenThermMessageType t, OpenThermMessageID id, uint16_t data) {
    unsigned long f = ((unsigned long)(int)t << 28) | ((unsigned long)id << 16) | data; setOTParityBit(f); return f;
  }
};
static unsigned long g_frames[16]; static int g_nframes = 0;
static bool otCmdEnqueue(unsigned long f) { g_frames[g_nframes++] = f; return true; }
static void otdPersistHW(char) {}   // TASK-1836: the HW= branch stores HW 0/1/A (not under test here)
static unsigned long g_now = 1000;
static unsigned long millis() { return g_now; }
static bool g_unknown99 = false;
static bool isUnknownId(uint8_t id) { return id == 99 && g_unknown99; }
static uint8_t otMasterStatusFlags = 0x01;
static uint8_t otDHWOverride = 0xFF;
static bool otChModeOff = false, otSummerMode = false, otDHWBlocking = false, otCoolingEnable = true, otCH2Enable = true;
static uint8_t otOperModeDHW = 0, otOperModeCH1 = 0, otOperModeCH2 = 0;
// state of the code before TASK-1845 (unused by the fix)
enum DHWPushState : uint8_t { PUSH_IDLE = 0, PUSH_PENDING, PUSH_STARTED };
static DHWPushState otDHWPushState = PUSH_IDLE;
static uint32_t otDHWPushStartMs = 0;
static char g_reply[16];
static void synthesizeResponse(const char* c, const char* v) { snprintf(g_reply, sizeof g_reply, "%.2s: %s", c, v); }
"""

OLD_PUSH = r"""
// before TASK-1845 the push followed the boiler inside handleMasterResponse(), which this harness does not cut
static bool otPushManual = false, otPushPending = false, otPushSpoof = false, otPushStarted = false;
static void otdPushOnBoiler99(uint8_t, uint16_t) {}
static void otdPushOnBoilerStatus(uint8_t) {}
"""

MAIN = r"""
static unsigned long relayed(uint8_t hb) { unsigned long r = (unsigned long)hb << 8; setOTParityBit(r); return otdRelayedStatusFrame(r); }
int main(int argc, char** argv) {
  // script: c:<HW value> | b99:<type>:<data hex> (boiler answer to MsgID 99) | b0:<slave LB hex> (boiler MsgID 0 READ_ACK)
  //         | s:<thermostat master HB hex> (print the relayed status HB) | t:<ms> (advance the clock) | u (MsgID 99 unknown)
  for (int i = 1; i < argc; i++) {
    const char* a = argv[i];
    if (a[0] == 'c') { char buf[8]; snprintf(buf, sizeof buf, "HW=%s", a + 2); g_nframes = 0; g_reply[0] = 0; cmd(buf, a + 2);
      printf("REPLY %s\n", g_reply); for (int k = 0; k < g_nframes; k++) printf("SENT %08lX\n", g_frames[k]); }
    else if (a[0] == 'b' && a[1] == '9') { unsigned t = 0, d = 0; sscanf(a + 4, "%u:%x", &t, &d); otdPushOnBoiler99((uint8_t)t, (uint16_t)d); }
    else if (a[0] == 'b' && a[1] == '0') otdPushOnBoilerStatus((uint8_t)strtoul(a + 3, nullptr, 16));
    else if (a[0] == 's') printf("HB %02lX\n", (relayed((uint8_t)strtoul(a + 2, nullptr, 16)) >> 8) & 0xFF);
    else if (a[0] == 't') g_now += strtoul(a + 2, nullptr, 10);
    else if (a[0] == 'u') g_unknown99 = true;
    printf("STATE W=%c push=%d\n", otDHWOverride == 0xFF ? 'A' : (char)('0' + otDHWOverride), otPushManual ? 1 : 0);
  }
  return 0;
}
"""


def braced(src: str, start: int, what: str) -> str:
    i, depth = src.index("{", start), 0
    for j in range(i, len(src)):
        if src[j] == "{":
            depth += 1
        elif src[j] == "}":
            depth -= 1
            if depth == 0:
                return src[start:j + 1]
    raise AssertionError("unbalanced " + what)


def func(src: str, signature: str) -> str:
    m = re.search(r"^" + re.escape(signature), src, re.M)
    if not m:
        raise AssertionError(signature + " not found")
    return braced(src, m.start(), signature)


def block(src: str, name: str, required=True) -> str:
    m = re.search(r"// --- %s begin\n(.*?)// --- %s end" % (name, name), src, re.S)
    if not m and required:
        raise AssertionError(name + " block not found")
    return m.group(1) if m else ""


def build_source() -> str:
    src = OTD_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
    types = OTD_TYPES.read_text(encoding="utf-8").replace("\r\n", "\n")
    parts = [re.search(r"// --- OtdStatusCmd begin\n(.*?)// --- OtdStatusCmd end", types, re.S).group(1)]
    m = re.search(r"^static OTFrameOverride otOverrides\[\] = \{", src, re.M)
    parts.append("struct OTFrameOverride { uint8_t msgId; bool active; uint16_t overrideValue; };\n" + src[m.start():src.index("};", m.start()) + 2])
    parts.append("static constexpr uint8_t OT_OVERRIDE_COUNT = sizeof(otOverrides) / sizeof(otOverrides[0]);")
    parts.append(func(src, "static bool otdCsOverrideActive()"))
    parts.append(func(src, "static uint16_t otdOperModeData(bool push)"))
    push = block(src, "otdPush", required=False)
    if push:
        flags = re.findall(r"^static (?:bool|uint8_t)\s+otPush\w+\s*=.*$", src, re.M)
        parts.extend(flags)
        parts.append(push)
    else:
        parts.append(OLD_PUSH)
    parts.append(block(src, "otdEffectiveMasterFlags"))
    parts.append(block(src, "otdRelayedStatus"))
    hw = re.search(r"else if \(cmd0 == 'H' && cmd1 == 'W'\) \{", src)
    parts.append("static bool cmd(const char* buf, const char* value) {\n  const char cmd0 = buf[0], cmd1 = buf[1];\n"
                 "  char rspBuf[16];\n  if (false) {}\n  " + braced(src, hw.start(), "HW") + "\n  return true;\n}")
    return STUBS + "\n".join(parts) + MAIN


_tmp = None
_exe = None


def setUpModule():
    global _tmp, _exe
    _tmp = Path(tempfile.mkdtemp(prefix="otd_push_"))
    cpp = _tmp / "otd_push.cpp"
    cpp.write_text(build_source(), encoding="utf-8")
    _exe = compile_cpp(cpp, _tmp / ("otd_push.exe" if os.name == "nt" else "otd_push"))
    if _exe is None:
        raise unittest.SkipTest("no C++ compiler")


def tearDownModule():
    if _tmp:
        shutil.rmtree(_tmp, ignore_errors=True)


def run(*script) -> list:
    return subprocess.run([str(_exe), *script], capture_output=True, text=True, check=True).stdout.splitlines()


def states(out):
    return [line for line in out if line.startswith("STATE")]


def hbs(out):
    return [line.split()[1] for line in out if line.startswith("HB ")]


class DhwPush(unittest.TestCase):
    def test_hw_p_keeps_hw0_during_and_after_a_boiler_push(self):
        # AC1, dhw-5.stc: HW=0, HW=P sends R10631000; the boiler takes the push and later clears the bit
        out = run("c:0", "c:P", "b99:5:1000", "s:00", "b99:4:0000", "s:00")
        self.assertIn("SENT 10631000", out)                  # WRITE 99 with the push bit: exactly R10631000 of dhw-5.stc
        self.assertIn("REPLY HW: P", out)
        self.assertTrue(all(s.startswith("STATE W=0") for s in states(out)[1:]), states(out))   # PR=W answers W=0 throughout
        self.assertEqual(hbs(out), ["00", "00"])              # DHW enable stays off (HotWaterSwitch, no spoof)
        self.assertEqual(states(out)[-1], "STATE W=0 push=0")

    def test_dhw_enable_not_forced_when_the_boiler_takes_the_push(self):
        # AC3: thermostat says DHW off, HW auto, boiler accepts: the relayed bit is the thermostat's
        out = run("c:P", "b99:5:1000", "s:00")
        self.assertEqual(hbs(out), ["00"])

    def test_dhw_enable_forced_when_the_boiler_refuses_or_lacks_the_push(self):
        # AC3: WRITE_ACK without the push bit, or Unknown-DataID, or MsgID 99 known unsupported -> spoof
        self.assertEqual(hbs(run("c:P", "b99:5:0000", "s:00")), ["02"])
        self.assertEqual(hbs(run("c:P", "b99:7:1000", "s:00")), ["02"])
        # TASK-1907: a boiler Data-Invalid (type 6) is the PIC's Unknown-DataID path too: MessageID99's jump table has a
        # nop for Data-Invalid that falls through into ID99Unknown (gateway.asm:3227-3230), which spoofs during a push
        self.assertEqual(hbs(run("c:P", "b99:6:1000", "s:00")), ["02"])
        self.assertEqual(hbs(run("b99:6:0000", "s:00")), ["00"])   # no push asked: nothing spoofed (manualdhwpush clear)
        out = run("u", "c:P", "s:00")
        self.assertNotIn("SENT", " ".join(out))              # nothing written when 99 is known unsupported
        self.assertEqual(hbs(out), ["02"])

    def test_handle_master_response_feeds_a_data_invalid_99_to_the_push(self):
        # TASK-1907: the boiler's Data-Invalid comes in through the boilerDataInvalid branch of handleMasterResponse(),
        # not the SUCCESS branch; that branch must hand MsgID 99 to otdPushOnBoiler99() with type 6
        src = OTD_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        a = src.index("static void handleMasterResponse() {")
        branch = src[src.index("  } else if (boilerDataInvalid) {", a):]
        branch = branch[:branch.index("  } else {")]
        self.assertRegex(branch, r"otdPushOnBoiler99\(6 /\* DATA_INVALID \*/, \(uint16_t\)\(response & 0xFFFF\)\)")
        self.assertRegex(branch, r"if \(\(\(response >> 16\) & 0xFF\) == 99\)")

    def test_a_read_ack_while_the_push_write_is_pending_does_not_end_it(self):
        # a READ 99 (the gateway's own slow poll, or the thermostat's) answered before the WRITE 99: the PIC turns such
        # reads into writes while opermodewrite is set (ID99ReadData, gateway.asm:3253-3257), so the answer cannot end the
        # push; a refusing boiler must still trigger the spoof afterwards
        out = run("c:P", "b99:4:0000", "b99:5:0000", "s:00")
        self.assertEqual(hbs(out), ["02"])
        self.assertEqual(states(out)[-1].split()[2], "push=1")

    def test_a_started_spoofed_push_is_not_cut_at_30_s(self):
        # AC2: flame on + DHW mode starts it; 60 s later it still runs; it ends when DHW mode clears
        out = run("c:P", "b99:7:0000", "b0:0C", "t:60000", "b0:0C", "s:00", "b0:08", "s:00")
        self.assertEqual(hbs(out), ["02", "00"])
        self.assertEqual(states(out)[-1].split()[2], "push=0")

    def test_eight_flame_off_answers_end_a_push_that_never_started(self):
        script = ["c:P", "b99:7:0000"] + ["b0:00"] * 7 + ["s:00", "b0:00", "s:00"]
        self.assertEqual(hbs(run(*script)), ["02", "00"])

    def test_any_other_hw_aborts_the_push(self):
        out = run("c:P", "b99:7:0000", "c:A", "s:00")
        self.assertEqual(hbs(out), ["00"])
        self.assertEqual(states(out)[-1], "STATE W=A push=0")


if __name__ == "__main__":
    unittest.main(verbosity=2)
