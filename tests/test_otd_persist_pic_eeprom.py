#!/usr/bin/env python3
"""TASK-1836: OT-Direct keeps the command state the PIC keeps in EEPROM, and applies it again at boot.

PIC ground truth (OTGW-other-projects/otgw-6.8/gateway.asm, repo 770aa45): SaveConfig after HW (:4408),
SM, SW/SH (:4187-4188, :4214-4215), MI (:4610-4611), SB (:4160-4161), FS (:5300), IT/OH (:4959-4965),
UI/KI (:4900-4905); restored at reset (:1205-1240, :1298-1324); HW=P leaves the stored state (:4410-4412).
Tests in otgw-6.8/tests: serial-5.stc (SM), thermostat-5.stc (SH), dhw-7.stc (SW across a reset).

The code under test is REAL, cut from src/OTGW-firmware/OTDirect.ino: the SM, SB, MI, FS, IT, OH, SW, SH,
UI, KI and HW branches of handleOTDirectCommand() (braces matched), the otdPersist helpers and
otdRestorePersistedCommands() (between their markers), and OTDirectSettingsSection from OTDirecttypes.h.
settingsMarkDirty() is a counter. A "reboot" keeps settings.otd, wipes the RAM state, and calls the restore.

Old-vs-fix: git show <old-rev>:src/OTGW-firmware/OTDirect.ino > $TMP/OTDirect.ino
            git show <old-rev>:src/OTGW-firmware/OTDirecttypes.h > $TMP/OTDirecttypes.h
            OTGW_OTD_INO=$TMP/OTDirect.ino OTGW_OTD_TYPES=$TMP/OTDirecttypes.h python tests/test_otd_persist_pic_eeprom.py
Run: python tests/test_otd_persist_pic_eeprom.py
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
OTD_TYPES = Path(os.environ.get("OTGW_OTD_TYPES", REPO / "src" / "OTGW-firmware" / "OTDirecttypes.h"))
SRC = OTD_INO.read_text(encoding="utf-8")
TYPES = OTD_TYPES.read_text(encoding="utf-8")


def braced(src, start_pat):
    m = re.search(start_pat, src)
    if not m:
        return None
    i, depth = src.index("{", m.start()), 0
    for j in range(i, len(src)):
        if src[j] == "{":
            depth += 1
        elif src[j] == "}":
            depth -= 1
            if depth == 0:
                return src[m.start():j + 1]
    return None


def marked(src, name):
    m = re.search(r"// --- %s begin\r?\n(.*?)// --- %s end" % (name, name), src, re.S)
    return m.group(1) if m else None


CMDS = ["SM", "SB", "MI", "FS", "IT", "OH", "SW", "SH", "UI", "KI", "HW"]

PRE = r"""
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#define PSTR(s) (s)
#define F(s) (s)
#define snprintf_P snprintf
#define OTDDebugTln(...) do {} while (0)
#define OTDDebugTf(...) do {} while (0)
#define constrain(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))   /* Arduino's macro */
static size_t strlcpy(char* d, const char* s, size_t n) { size_t l = strlen(s); if (n) { size_t c = l < n - 1 ? l : n - 1; memcpy(d, s, c); d[c] = 0; } return l; }
static char* dtostrf(double v, int w, int p, char* out) { sprintf(out, "%*.*f", w, p, v); return out; }
@SETTINGS_STRUCT@
static struct { OTDirectSettingsSection otd; } settings;
static int g_dirty = 0;
static void settingsMarkDirty() { g_dirty++; }
static uint32_t millis() { return 1000; }
static bool checkBoolean(const char* v) { return (v[0] == '0' || v[0] == '1') && v[1] == 0; }
static char g_status[8];
static void otDirectBridgeProcessStatus(const char* s) { strlcpy(g_status, s, sizeof g_status); }
static char g_rsp[64];
static void synthesizeResponse(const char* buf, const char* rsp) { snprintf(g_rsp, sizeof g_rsp, "%s: %s", buf, rsp); }
static int g_enq = 0; static unsigned g_enqLast[2];
static bool enqueueWriteCommand(uint8_t id, uint16_t data, const char*) { g_enq++; g_enqLast[0] = id; g_enqLast[1] = data; return true; }
static void clearWriteOverride(uint8_t) {}
static bool otCmdEnqueue(unsigned long) { return true; }
struct OpenTherm { static unsigned long buildRequest(int t, int id, uint16_t d) { return ((unsigned long)t << 28) | ((unsigned long)id << 16) | d; } };
enum OpenThermMessageType { READ_DATA = 0, WRITE_DATA = 1 };
typedef int OpenThermMessageID;
#define OpenThermMessageType_WRITE_DATA 1
enum { PUSH_IDLE = 0, PUSH_PENDING = 1 };
static void otdPushStart() {}   // TASK-1845 push helpers the HW= branch calls (not under test here)
static void otdPushEnd() {}
static uint8_t otDHWPushState = PUSH_IDLE; static uint32_t otDHWPushStartMs = 0;
static uint8_t otOperModeDHW = 0, otOperModeCH1 = 0, otOperModeCH2 = 0;
static uint8_t otMasterStatusFlags = 0x01;
static bool otSummerMode = false, otFailSafeEnabled = true;
static float otSetbackTemp = 16.0f;
static uint16_t otMinIntervalMs = 100;
static uint8_t otIgnoreTransitions = 1, otOverrideHB = 0, otDHWOverride = 0xFF;
static constexpr uint8_t OT_UNKNOWN_ID_MAX = 16;
static uint8_t otUnknownIds[OT_UNKNOWN_ID_MAX]; static uint8_t otUnknownIdCount = 0;
static bool isUnknownId(uint8_t id) { for (uint8_t i = 0; i < otUnknownIdCount; i++) if (otUnknownIds[i] == id) return true; return false; }
static void clearUnknownCount(uint8_t) {}
struct OTScheduleEntry { uint8_t msgId; bool disabled; };
static constexpr uint8_t OT_SCHEDULE_SIZE = 3;
static OTScheduleEntry otSchedule[OT_SCHEDULE_SIZE] = { {56, false}, {70, false}, {5, false} };
static inline uint16_t floatToF88(float c) { return (uint16_t)(int16_t)(c * 256.0f); }
"""

MAIN = r"""
@HELPERS@
@RESTORE@
static bool cmd(const char* buf, const char* value) {
  const char cmd0 = buf[0], cmd1 = buf[1];
  char rspBuf[24];
  if (false) {}
  @BRANCHES@
  return true;
}
static void step(const char* name, const char* buf, const char* value) {
  int before = g_dirty;
  cmd(buf, value);
  printf("%s dirty=%d\n", name, g_dirty - before);
}
static void wipeRam() {
  otMasterStatusFlags = 0x01; otSummerMode = false; otFailSafeEnabled = true; otSetbackTemp = 16.0f;
  otMinIntervalMs = 100; otIgnoreTransitions = 1; otOverrideHB = 0; otDHWOverride = 0xFF;
  otUnknownIdCount = 0; for (auto &e : otSchedule) e.disabled = false; g_enq = 0;
}
int main() {
  step("SM", "SM", "1");
  step("SB", "SB", "12.5");
  step("MI", "MI", "500");
  step("FS", "FS", "0");
  step("IT", "IT", "0");
  step("OH", "OH", "1");
  step("SW", "SW", "55");
  step("SH", "SH", "70");
  step("UI1", "UI", "70");
  step("UI2", "UI", "5");
  step("KI", "KI", "5");
  step("HW0", "HW", "0");
  step("HWP", "HW", "P");
  step("SMsame", "SM", "1");
  printf("stored sm=%d sb=%.1f mi=%u fs=%d it=%d oh=%d sw=%.1f sh=%.1f hw=%u ui=[%s]\n",
         settings.otd.bSummerMode, settings.otd.fSetbackTemp, (unsigned)settings.otd.iMsgInterval, settings.otd.bFailSafe,
         HAS_IT_FIELD, HAS_OH_FIELD, HAS_SW_FIELD, HAS_SH_FIELD, HAS_HW_FIELD, HAS_UI_FIELD);
  wipeRam();
#ifdef HAS_RESTORE
  otdRestorePersistedCommands();
  printf("boot hw=%u dhwbit=%d it=%u oh=%u unknown=%u id0=%u sched70=%d enq=%d last=%u:%04X\n",
         otDHWOverride, (otMasterStatusFlags & 0x02) != 0, otIgnoreTransitions, otOverrideHB, otUnknownIdCount,
         otUnknownIdCount ? otUnknownIds[0] : 0, otSchedule[1].disabled, g_enq, g_enqLast[0], g_enqLast[1]);
#else
  printf("boot none\n");
#endif
  return 0;
}
"""


def build_source():
    struct = braced(TYPES, r"struct OTDirectSettingsSection \{")
    assert struct, "OTDirectSettingsSection not found"
    branches = []
    for c in CMDS:
        b = braced(SRC, r"else if \(cmd0 == '%s' && cmd1 == '%s'\) \{" % (c[0], c[1]))
        assert b, c + "= branch not found"
        branches.append(b)
    helpers = marked(SRC, "otdPersist") or ""
    restore = marked(SRC, "otdRestorePersistedCommands")
    fields = {
        "HAS_IT_FIELD": "settings.otd.bIgnoreTransitions" if "bIgnoreTransitions" in struct else "-1",
        "HAS_OH_FIELD": "settings.otd.bOverrideHB" if "bOverrideHB" in struct else "-1",
        "HAS_SW_FIELD": "settings.otd.fDhwSetpoint" if "fDhwSetpoint" in struct else "-1.0",
        "HAS_SH_FIELD": "settings.otd.fMaxChSetpoint" if "fMaxChSetpoint" in struct else "-1.0",
        "HAS_HW_FIELD": "(unsigned)settings.otd.iDhwOverride" if "iDhwOverride" in struct else "999u",
        "HAS_UI_FIELD": "settings.otd.sUnknownIds" if "sUnknownIds" in struct else '"none"',
    }
    defs = "".join("#define %s %s\n" % kv for kv in fields.items())
    if restore:
        defs += "#define HAS_RESTORE 1\n"
    return (PRE.replace("@SETTINGS_STRUCT@", struct + ";") + defs +
            MAIN.replace("@HELPERS@", helpers).replace("@RESTORE@", restore or "")
                .replace("@BRANCHES@", "\n  ".join(branches)))


class Persist(unittest.TestCase):
    out = None

    @classmethod
    def setUpClass(cls):
        with tempfile.TemporaryDirectory() as d:
            cpp = Path(d) / "t.cpp"
            cpp.write_text(build_source(), encoding="utf-8")
            exe = compile_cpp(cpp, Path(d) / ("t.exe" if os.name == "nt" else "t"))
            if exe is None:
                raise unittest.SkipTest("no host compiler")
            cls.out = subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout
        cls.rows = {l.split()[0]: l for l in cls.out.splitlines()}

    def dirty(self, name):
        return int(re.search(r"dirty=(\d+)", self.rows[name]).group(1))

    def test_ac1_sm_sb_mi_fs_mark_settings_dirty(self):
        for c in ("SM", "SB", "MI", "FS"):
            self.assertEqual(self.dirty(c), 1, f"{c}= must mark the settings dirty (PIC SaveConfig): {self.rows[c]}")

    def test_unchanged_value_marks_nothing(self):
        self.assertEqual(self.dirty("SMsame"), 0)

    def test_ac2_hw_sw_sh_ui_it_oh_are_stored(self):
        for c in ("IT", "OH", "SW", "SH", "UI1", "UI2", "KI", "HW0"):
            self.assertEqual(self.dirty(c), 1, f"{c} must mark the settings dirty: {self.rows[c]}")
        self.assertEqual(self.dirty("HWP"), 0, "HW=P is a one-off push, not stored (gateway.asm:4410-4412)")
        s = self.rows["stored"]
        self.assertIn("sm=1 sb=12.5 mi=500 fs=0 it=0 oh=1 sw=55.0 sh=70.0 hw=0 ui=[70]", s, s)

    def test_ac2_boot_restores_and_applies(self):
        b = self.rows["boot"]
        self.assertNotEqual(b.strip(), "boot none", "no otdRestorePersistedCommands() (the code before TASK-1836)")
        # HW=0 back (PR=W answers W=0), IT=0 and OH=1 back, UI=70 back and its schedule entry off,
        # SW=55 and SH=70 re-sent to the boiler (last write: MsgID 57, 70.0 = 0x4600)
        self.assertIn("boot hw=0 dhwbit=0 it=0 oh=1 unknown=1 id0=70 sched70=1 enq=2 last=57:4600", b, b)


if __name__ == "__main__":
    unittest.main(verbosity=2)
