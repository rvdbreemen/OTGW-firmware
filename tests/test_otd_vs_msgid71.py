#!/usr/bin/env python3
"""TASK-1831: OT-Direct VS= (ventilation setpoint, MsgID 71) as the spec and the PIC.

Ground truth:
  - OpenTherm spec v4.2 (docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2.md:1514-1518): MsgID 71
    "LB: Control Setpoint ventilation / heat-recovery", u8 0..100.
  - PIC 6.8 gateway.asm (OTGW-other-projects/otgw-6.8): MessageID71 (3210-3217) turns every thermostat MsgID 71 request
    into a WRITE (MsgWriteOp), the VS value in the low byte (setbyte4), high byte 0; SetVentSetpoint (4241-4254) answers a
    non-numeric value with ValueCleared ("VS: -").
  - PIC test tests/ventilation-1.stc: thermostat writes 0x90470014, VS=15 -> "VS: 15" and R9047000F; VS=XX -> "VS: -".
OT-Direct before the fix put the value in the high byte, had no override slot and no write entry for MsgID 71 (so the
override and the boot restore were no-ops), and echoed the clear value.

The code under test is REAL: the VS branch of handleOTDirectCommand(), enqueueWriteCommand(), applyOverrides(),
setOverride(), clearOverride(), updateWriteCache(), clearWriteOverride(), setOTParityBit(), the otOverrides table and
the boot-restore block of initOTDirect() are cut from src/OTGW-firmware/OTDirect.ino. The schedule is a double with one
write entry per ID the code looks up; a separate check reads the real schedule table for W_ENTRY(71.

Old-vs-fix: OTGW_OTD_INO points at another copy of OTDirect.ino, e.g.
  git show <old-rev>:src/OTGW-firmware/OTDirect.ino > $TMP/OTDirect.ino
  OTGW_OTD_INO=$TMP/OTDirect.ino python tests/test_otd_vs_msgid71.py
Run: python tests/test_otd_vs_msgid71.py
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
template <class T> static T constrain(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }
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
static std::string_view dummy;
struct SchedEntry { uint8_t msgId; bool isWrite; uint16_t cachedValue; bool valueSet; };
static SchedEntry otSchedule[] = { { 1, true, 0, false }, { 71, true, 0, false } };
static const uint8_t OT_SCHEDULE_SIZE = 2;
struct { struct { uint8_t iVentSetpoint; } otd; } settings = { { 0 } };
static int g_writes = 0;
static void writeSettings(bool) { g_writes++; }
enum { OTD_MODE_GATEWAY = 1 };
static int otCurrentMode = OTD_MODE_GATEWAY;
static unsigned long otdRelayedStatusFrame(unsigned long f) { return f; }
static char g_reply[32];
static void synthesizeResponse(const char* c, const char* v) { snprintf(g_reply, sizeof g_reply, "%.2s: %s", c, v); }
static void otDirectBridgeProcessStatus(const char* s) { snprintf(g_reply, sizeof g_reply, "%s", s); }
"""

MAIN = r"""
static void vs(const char* v) { char buf[16]; snprintf(buf, sizeof buf, "VS=%s", v); g_nframes = 0; g_reply[0] = 0; cmd(buf, v);
  printf("REPLY %s\n", g_reply); for (int k = 0; k < g_nframes; k++) printf("SENT %08lX\n", g_frames[k]); }
static void thermo(unsigned long f) { bool m = false; unsigned long out = applyOverrides(f, m); printf("BOILER %08lX %d\n", out, m ? 1 : 0); }
int main(int argc, char** argv) {
  // script: v:<VS value> | t:<thermostat frame hex> | b:<persisted setpoint, then the boot restore>
  for (int i = 1; i < argc; i++) {
    const char* a = argv[i];
    if (a[0] == 'v') vs(a + 2);
    else if (a[0] == 't') thermo(strtoul(a + 2, nullptr, 16));
    else if (a[0] == 'b') { settings.otd.iVentSetpoint = (uint8_t)atoi(a + 2); bootRestore();
      for (int k = 0; k < OT_SCHEDULE_SIZE; k++) if (otSchedule[k].msgId == 71) printf("CACHE %d %04X\n", otSchedule[k].valueSet ? 1 : 0, otSchedule[k].cachedValue); }
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


def build_source() -> str:
    src = OTD_INO.read_text(encoding="utf-8")
    parts = [func(src, "static inline void setOTParityBit(unsigned long &frame)")]
    m = re.search(r"^static OTFrameOverride otOverrides\[\] = \{", src, re.M)
    table_end = src.index("};", m.start()) + 2
    parts.append("struct OTFrameOverride { uint8_t msgId; bool active; uint16_t overrideValue; };\n" + src[m.start():table_end])
    parts.append("static constexpr uint8_t OT_OVERRIDE_COUNT = sizeof(otOverrides) / sizeof(otOverrides[0]);")
    for sig in ("static void setOverride(uint8_t msgId, uint16_t value)", "static void clearOverride(uint8_t msgId)",
                "static void updateWriteCache(uint8_t msgId, uint16_t value)",
                "static unsigned long applyOverrides(unsigned long frame, bool &modified)",
                "static bool enqueueWriteCommand(uint8_t msgId, uint16_t dataValue, const char* label)",
                "static void clearWriteOverride(uint8_t msgId)"):
        parts.append(func(src, sig))
    vsm = re.search(r"else if \(cmd0 == 'V' && cmd1 == 'S'\) \{", src)
    parts.append("static bool cmd(const char* buf, const char* value) {\n  const char cmd0 = buf[0], cmd1 = buf[1];\n"
                 "  char rspBuf[16];\n  if (false) {}\n  " + braced(src, vsm.start(), "VS") + "\n  return true;\n}")
    bm = re.search(r"^  if \(settings\.otd\.iVentSetpoint > 0\) \{", src, re.M)
    parts.append("static void bootRestore() {\n" + braced(src, bm.start(), "boot restore") + "\n}")
    return STUBS.replace("static std::string_view dummy;\n", "") + "\n".join(parts) + MAIN


_tmp = None
_exe = None


def setUpModule():
    global _tmp, _exe
    _tmp = Path(tempfile.mkdtemp(prefix="otd_71_"))
    cpp = _tmp / "otd_71.cpp"
    cpp.write_text(build_source(), encoding="utf-8")
    _exe = compile_cpp(cpp, _tmp / ("otd_71.exe" if os.name == "nt" else "otd_71"))
    if _exe is None:
        raise unittest.SkipTest("no C++ compiler")


def tearDownModule():
    if _tmp:
        shutil.rmtree(_tmp, ignore_errors=True)


def run(*script) -> list:
    return subprocess.run([str(_exe), *script], capture_output=True, text=True, check=True).stdout.splitlines()


class VentSetpoint(unittest.TestCase):
    def test_vs15_writes_the_value_in_the_low_byte(self):
        out = run("v:15")
        self.assertIn("REPLY VS: 15", out)
        self.assertIn("SENT 9047000F", out)          # tests/ventilation-1.stc

    def test_thermostat_write_gets_the_vs_value_as_pic_ventilation1(self):
        out = run("v:15", "t:90470014")
        self.assertIn("BOILER 9047000F 1", out)      # tests/ventilation-1.stc: R9047000F

    def test_thermostat_read_goes_to_the_boiler_as_a_write(self):
        out = run("v:15", "t:00470000")
        self.assertIn("BOILER 9047000F 1", out)      # gateway.asm MessageID71: MsgWriteOp

    def test_clear_replies_dash_and_restores_pass_through(self):
        out = run("v:15", "v:XX", "t:90470014")
        self.assertIn("REPLY VS: -", out)
        self.assertIn("BOILER 90470014 0", out)

    def test_persisted_setpoint_is_applied_after_boot(self):
        out = run("b:40", "t:90470014")
        self.assertIn("CACHE 1 0028", out)
        self.assertIn("BOILER 90470028 1", out)      # WRITE 71 = 40 %, odd parity sets bit 31

    def test_the_real_schedule_has_a_write_entry_for_msgid_71(self):
        src = OTD_INO.read_text(encoding="utf-8")
        table = src[src.index("static OTScheduleEntry otSchedule[] = {"):]
        table = table[:table.index("};")]
        self.assertRegex(table, r"W_ENTRY\(\s*71\s*,")


if __name__ == "__main__":
    unittest.main(verbosity=2)
