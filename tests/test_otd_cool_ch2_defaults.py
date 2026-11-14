#!/usr/bin/env python3
"""TASK-1833: OT-Direct CE= and H2= default to on, as the PIC; CL=0 holds 0 %.

Ground truth (PIC 6.8, OTGW-other-projects/otgw-6.8/gateway.asm):
  - readstatus (2720-2747): with a CL= cooling level held (SysCoolLevel clear) bit 2 of the relayed MsgID 0 master status
    is !CoolModeOff; with a C2= CH2 setpoint held (SysCH2Setpoint clear) bit 4 is !CH2ModeOff. Both flags are clear after
    reset (RAM clear 1178-1188), so the bits are on until CE=0 / H2=0 (SetCoolEnable 4446-4452, SetCentralHeat2
    5264-5270). The command reference gives CE= and H2= "Initial value 1"; ADR-229 says the same.
  - SetCoolLevel (4454-4464): a number, 0 included, is held; only a value that is not a number clears it (ValueCleared,
    reply "CL: -").
OT-Direct before the fix: CE= and H2= defaulted to off, so C2=40 alone switched the CH2 enable bit off and CL=50 the
cooling enable bit; CL=0 cleared the cooling level and echoed "0.00".

The code under test is REAL: the C2, CL, CE and H2 branches of handleOTDirectCommand(), enqueueWriteCommand(),
set/clearOverride(), updateWriteCache(), clearWriteOverride(), setOTParityBit(), the otOverrides table, the declarations
of otCoolingEnable and otCH2Enable, otdCsOverrideActive(), the otdEffectiveMasterFlags and otdRelayedStatus blocks and
the OtdStatusCmd struct are cut from src/OTGW-firmware/OTDirect.ino and OTDirecttypes.h. The other command state the
status rule reads is a double with its boot value.

Old-vs-fix: OTGW_OTD_INO points at another copy of OTDirect.ino, e.g.
  git show <old-rev>:src/OTGW-firmware/OTDirect.ino > $TMP/OTDirect.ino
  OTGW_OTD_INO=$TMP/OTDirect.ino python tests/test_otd_cool_ch2_defaults.py
Run: python tests/test_otd_cool_ch2_defaults.py
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
#include <cctype>
#define PSTR(s) (s)
#define F(s) (s)
#define snprintf_P snprintf
#define OTDDebugTf(...) do {} while (0)
#define DebugTf(...) do {} while (0)
enum class OpenThermMessageType { READ_DATA = 0, WRITE_DATA = 1 };
typedef uint8_t OpenThermMessageID;
static inline void setOTParityBit(unsigned long &frame);
struct OpenTherm {
  static unsigned long buildRequest(OpenThermMessageType t, OpenThermMessageID id, uint16_t data) {
    unsigned long f = ((unsigned long)(int)t << 28) | ((unsigned long)id << 16) | data; setOTParityBit(f); return f;
  }
};
static unsigned long g_frames[16]; static int g_nframes = 0;
static bool otCmdEnqueue(unsigned long f) { g_frames[g_nframes++] = f; return true; }
static bool otdWriteRefused = false;
struct SchedEntry { uint8_t msgId; bool isWrite; uint16_t cachedValue; bool valueSet; };
static SchedEntry otSchedule[] = { { 7, true, 0, false }, { 8, true, 0, false } };
static const uint8_t OT_SCHEDULE_SIZE = 2;
static unsigned long millis() { return 1000; }
static uint32_t otC2LastCommandMs = 0;
static uint32_t otdCsc2ExpiryStamp(uint16_t) { return 1; }  // TASK-1917 expiry rule, tested in test_otd_csc2_below8
static uint16_t floatToF88(float v) { return (uint16_t)(int16_t)(v * 256.0f); }
static char* dtostrf(double v, int w, int p, char* out) { sprintf(out, "%*.*f", w, p, v); return out; }
enum DHWPushState { PUSH_IDLE, PUSH_PENDING, PUSH_ACTIVE };
static DHWPushState otDHWPushState = PUSH_IDLE;
static uint8_t otDHWOverride = 0xFF;
static uint8_t otMasterStatusFlags = 0x01;
static bool otChModeOff = false;
static bool otSummerMode = false;
static bool otDHWBlocking = false;
static bool otPushSpoof = false;   // TASK-1845: the relayed status forces DHW enable only while the push is spoofed
static char g_reply[32];
static void synthesizeResponse(const char* c, const char* v) { snprintf(g_reply, sizeof g_reply, "%.2s: %s", c, v); }
static void otDirectBridgeProcessStatus(const char* s) { snprintf(g_reply, sizeof g_reply, "%s", s); }
"""

MAIN = r"""
int main(int argc, char** argv) {
  // argv: commands "XX=value", then the thermostat's master status byte as "hb:<hex>"; prints replies and the relayed byte
  for (int i = 1; i < argc; i++) {
    if (strncmp(argv[i], "hb:", 3) == 0) {
      unsigned long req = strtoul(argv[i] + 3, nullptr, 16) << 8;   // READ_DATA MsgID 0
      setOTParityBit(req);
      printf("HB %02lX\n", (otdRelayedStatusFrame(req) >> 8) & 0xFF);
    } else {
      g_reply[0] = 0; g_nframes = 0;
      cmd(argv[i], argv[i] + 3);
      printf("REPLY %s\n", g_reply);
      for (int k = 0; k < g_nframes; k++) printf("SENT %08lX\n", g_frames[k]);
    }
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
        raise AssertionError(signature + " not found in " + str(OTD_INO))
    return braced(src, m.start(), signature)


def block(src: str, name: str) -> str:
    m = re.search(r"// --- %s begin\n(.*?)// --- %s end" % (name, name), src, re.S)
    if not m:
        raise AssertionError(name + " block not found")
    return m.group(1)


def branch(src: str, a: str, b: str) -> str:
    m = re.search(r"else if \(cmd0 == '%s' && cmd1 == '%s'\) \{" % (a, b), src)
    if not m:
        raise AssertionError(f"{a}{b}= branch not found")
    return braced(src, m.start(), a + b)


def build_source() -> str:
    src = OTD_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
    types = OTD_TYPES.read_text(encoding="utf-8").replace("\r\n", "\n")
    parts = [func(src, "static inline void setOTParityBit(unsigned long &frame)")]
    sm = re.search(r"// --- OtdStatusCmd begin\n(.*?)// --- OtdStatusCmd end", types, re.S)
    parts.append(sm.group(1))
    decls = re.findall(r"^static bool\s+(?:otCoolingEnable|otCH2Enable)\s*=.*$", src, re.M)
    if not any("otCH2Enable" in d for d in decls):
        decls.append("static bool otCH2Enable = false;   // absent before TASK-1833")
    parts.extend(decls)
    m = re.search(r"^static OTFrameOverride otOverrides\[\] = \{", src, re.M)
    parts.append("struct OTFrameOverride { uint8_t msgId; bool active; uint16_t overrideValue; };\n" + src[m.start():src.index("};", m.start()) + 2])
    parts.append("static constexpr uint8_t OT_OVERRIDE_COUNT = sizeof(otOverrides) / sizeof(otOverrides[0]);")
    for sig in ("static void setOverride(uint8_t msgId, uint16_t value)", "static void clearOverride(uint8_t msgId)",
                "static void updateWriteCache(uint8_t msgId, uint16_t value)",
                "static bool enqueueWriteCommand(uint8_t msgId, uint16_t dataValue, const char* label)",
                "static void clearWriteOverride(uint8_t msgId)", "static bool otdCsOverrideActive()",
                "static inline bool checkBoolean(const char* value)"):
        parts.append(func(src, sig))
    parts.append(block(src, "otdEffectiveMasterFlags"))
    parts.append(block(src, "otdRelayedStatus"))
    body = "".join(branch(src, a, b) + "\n  " for a, b in (("C", "2"), ("C", "L"), ("C", "E"), ("H", "2")))
    parts.append("static bool cmd(const char* buf, const char* value) {\n  const char cmd0 = buf[0], cmd1 = buf[1];\n"
                 "  char rspBuf[16];\n  if (false) {}\n  " + body + "\n  return true;\n}")
    return STUBS + "\n".join(parts) + MAIN


_tmp = None
_exe = None


def setUpModule():
    global _tmp, _exe
    _tmp = Path(tempfile.mkdtemp(prefix="otd_cool_"))
    cpp = _tmp / "otd_cool.cpp"
    cpp.write_text(build_source(), encoding="utf-8")
    _exe = compile_cpp(cpp, _tmp / ("otd_cool.exe" if os.name == "nt" else "otd_cool"))
    if _exe is None:
        raise unittest.SkipTest("no C++ compiler")


def tearDownModule():
    if _tmp:
        shutil.rmtree(_tmp, ignore_errors=True)


def run(*script) -> list:
    return subprocess.run([str(_exe), *script], capture_output=True, text=True, check=True).stdout.splitlines()


def hb(out):
    return [line for line in out if line.startswith("HB ")][-1]


class CoolCh2Defaults(unittest.TestCase):
    def test_c2_alone_sets_the_ch2_enable_bit(self):
        self.assertEqual(hb(run("C2=40", "hb:00")), "HB 10")

    def test_cl_alone_sets_the_cooling_enable_bit(self):
        self.assertEqual(hb(run("CL=50", "hb:00")), "HB 04")

    def test_h2_0_and_ce_0_clear_them(self):
        self.assertEqual(hb(run("C2=40", "H2=0", "hb:10")), "HB 00")
        self.assertEqual(hb(run("CL=50", "CE=0", "hb:04")), "HB 00")

    def test_no_held_value_leaves_the_bits_alone(self):
        self.assertEqual(hb(run("hb:14")), "HB 14")
        self.assertEqual(hb(run("hb:00")), "HB 00")

    def test_cl_0_holds_0_percent(self):
        out = run("CL=0", "hb:00")
        self.assertIn("REPLY CL: 0.00", out)
        self.assertIn("SENT 10070000", out)          # WRITE MsgID 7, 0 %
        self.assertEqual(hb(out), "HB 04")           # held, so the cooling enable bit follows CE (on)

    def test_cl_non_numeric_clears_and_replies_dash(self):
        out = run("CL=50", "CL=x", "hb:00")
        self.assertIn("REPLY CL: -", out)
        self.assertEqual(hb(out), "HB 00")


if __name__ == "__main__":
    unittest.main(verbosity=2)
