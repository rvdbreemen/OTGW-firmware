#!/usr/bin/env python3
"""TASK-1702: a saved OT-Direct mode takes effect at once, without a reboot.

Field report (tjfs, 2026-11-07): after choosing Gateway as OT-Direct mode in the settings the
monitor page still showed MODE: MASTER; only a reboot brought Gateway. The settings path
(updateSetting "OTDmode") stored settings.otd.iMode and nothing read it again before the next
initOTDirect(); the running mode otCurrentMode stayed as it was.

The test compiles REAL code cut from the sources with brace matching:
  - the OTDmode branch of updateSetting() (settingStuff.ino),
  - the start of loopOTDirect() up to the OpenTherm state-machine calls (OTDirect.ino),
  - setOTDirectMode() (OTDirect.ino),
  - on the fixed source also the TASK-1702 flag, otdModeSettingChanged()/Cancel() and
    otdModeFromSetting().
applyOTDirectMode() is a recording stub that sets otCurrentMode, as the real one does.

Run against older sources with OTGW_SRC_DIR=<dir holding OTDirect.ino and settingStuff.ino>;
on the code before TASK-1702 the live-apply tests fail with a named message.

Run: python tests/test_otd_mode_live_apply.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TESTS = Path(__file__).resolve().parent
REPO = TESTS.parent
sys.path.insert(0, str(TESTS))
from test_sat_pwm_override import compile_cpp  # noqa: E402

SRC = Path(os.environ.get("OTGW_SRC_DIR", REPO / "src" / "OTGW-firmware"))

PRELUDE = r"""
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <sstream>
#include <iostream>
#ifdef _MSC_VER   // the firmware uses the GCC __atomic builtins; MSVC gets plain single-thread stand-ins
#define __ATOMIC_RELEASE 0
#define __ATOMIC_ACQ_REL 0
#define __ATOMIC_RELAXED 0
#define __atomic_load_n(p, o) (*(p))
#define __atomic_store_n(p, v, o) (*(p) = (v))
template <typename T> static T __atomic_exchange_n(T* p, T v, int) { T o = *p; *p = v; return o; }
#endif
#define HAS_DIRECT_OT 1
#ifndef HAS_BYPASS_RELAY
#define HAS_BYPASS_RELAY 0
#endif
#define F(x) x
#define PSTR(x) x
#define OTDDebugTln(...) do {} while (0)
#define constrain(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))
static int strcasecmp_P(const char* a, const char* b) {
  for (;; a++, b++) { int d = tolower((unsigned char)*a) - tolower((unsigned char)*b); if (d || !*a) return d; }
}
enum OTDirectMode : uint8_t { OTD_MODE_BYPASS = 0, OTD_MODE_GATEWAY = 1, OTD_MODE_MONITOR = 2,
                              OTD_MODE_MASTER = 3, OTD_MODE_LOOPBACK = 4 };
struct { struct { int iMode = 1; bool bHasBypassRelay = false; } otd; } settings;
static OTDirectMode otCurrentMode = OTD_MODE_GATEWAY;
static bool otMasterRequestActive = false;
static int nApply = 0;
static void applyOTDirectMode(OTDirectMode m) { nApply++; otCurrentMode = m; }
static void otCmdApplyPendingFlush() {}   // the ring flush itself is covered by test_otd_mode_switch_queue_reset.py
struct OtStub { void process() {} } otMaster, otSlave;
static void updateSetting(const char* field, const char* newValue);
"""

MAIN = r"""
static void updateSetting(const char* field, const char* newValue) {
  if (false) {}
  @OTDMODE_BRANCH@
}
static void readSettingsTail() {
@READ_CANCEL@
}
int main() {
  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream is(line);
    std::string c, v;
    is >> c >> v;
    if (c == "stored") settings.otd.iMode = atoi(v.c_str());
    else if (c == "running") otCurrentMode = (OTDirectMode)atoi(v.c_str());
    else if (c == "relay") settings.otd.bHasBypassRelay = v == "1";
    else if (c == "busy") otMasterRequestActive = v == "1";
    else if (c == "post") updateSetting("OTDmode", v.c_str());
    else if (c == "boot") { updateSetting("OTDmode", v.c_str()); readSettingsTail(); }
    else if (c == "gw") setOTDirectMode((OTDirectMode)atoi(v.c_str()));
    else if (c == "loop") loopOTDirectHead();
    else if (c == "print") printf("%d %d %d\n", (int)otCurrentMode, settings.otd.iMode, nApply);
  }
  return 0;
}
"""


def cut_braced(src, signature):
    at = src.find(signature)
    if at < 0:
        raise AssertionError(signature + " not found")
    start = src.find("{", at)
    depth = 0
    for i in range(start, len(src)):
        depth += {"{": 1, "}": -1}.get(src[i], 0)
        if depth == 0:
            return src[at:i + 1]
    raise AssertionError("unbalanced braces in " + signature)


def read(name):
    return (SRC / name).read_text(encoding="utf-8")


def build_source():
    otd = read("OTDirect.ino")
    sset = read("settingStuff.ino")
    fixed = "otdModeSettingChanged" in otd

    # The OTDmode statement of the updateSetting() cascade, up to the next key.
    m = re.search(r"  else if \(strcasecmp_P\(field, PSTR\(\"OTDmode\"\)\) == 0\).*?(?=\n  else if )", sset, re.S)
    if not m:
        raise AssertionError("OTDmode branch not found in settingStuff.ino")
    branch = m.group(0)

    # The OTD part of the readSettings() tail (between the pendingSideEffects reset and the defaults).
    r = re.search(r"pendingSideEffects = 0;\n(#if defined\(HAS_DIRECT_OT\).*?#endif\n)", sset, re.S)
    read_cancel = r.group(1) if r else ""

    # Start of loopOTDirect(): everything before the OpenTherm state machines run.
    loop = cut_braced(otd, "void loopOTDirect()")
    head = loop[loop.index("{") + 1:loop.index("otMaster.process();")]
    loop_head = "static void loopOTDirectHead() {" + head + "}\n"

    helpers = ""
    if fixed:
        a = otd.index("static bool otdModeSettingPending")
        helpers = otd[a:otd.index("\n}\n", otd.index("static OTDirectMode otdModeFromSetting(")) + 3]
    set_mode = cut_braced(otd, "static void setOTDirectMode(").replace("static void setOTDirectMode(",
                                                                         "static void setOTDirectMode(", 1)
    # setOTDirectMode() formats with snprintf_P.
    pre = PRELUDE + "#define snprintf_P snprintf\n" + helpers + "\n"
    code = pre + loop_head + set_mode + "\n" + MAIN.replace("@OTDMODE_BRANCH@", branch).replace("@READ_CANCEL@", read_cancel)
    return code, fixed


class OtdModeLiveApply(unittest.TestCase):
    exe = None
    fixed = False

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        d = Path(cls.tmp.name)
        cpp = d / "otdmode.cpp"
        code, cls.fixed = build_source()
        cpp.write_text(code, encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / ("otdmode.exe" if os.name == "nt" else "otdmode"))
        if cls.exe is None:
            raise unittest.SkipTest("no C++ compiler")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_script(self, lines):
        out = subprocess.run([str(self.exe)], input="\n".join(lines) + "\n", capture_output=True, text=True,
                             check=True).stdout
        return [tuple(int(x) for x in row.split()) for row in out.splitlines()]

    OLD = ("saving OTDmode only stores settings.otd.iMode; the running mode stays until the next boot "
           "(TASK-1702). ")

    def test_saved_mode_runs_after_one_loop_pass(self):
        # tjfs: stored and running Master, the user picks Gateway.
        rows = self.run_script(["stored 3", "running 3", "post 1", "loop", "print"])
        self.assertEqual(rows[0], (1, 1, 1), self.OLD + "after saving Gateway the bus must run Gateway (mode, stored, applies)")

    def test_gateway_after_autodetect_master_is_honoured(self):
        # Stored Gateway, the boot auto-detect runs Master; the user saves Gateway again.
        rows = self.run_script(["stored 1", "running 3", "post 1", "loop", "print"])
        self.assertEqual(rows[0][0], 1, self.OLD + "an explicit Gateway choice must switch the running Master back")

    def test_applies_once_and_only_on_a_change(self):
        rows = self.run_script(["stored 1", "running 1", "post 2", "loop", "loop", "print",
                                "post 2", "loop", "print"])
        self.assertEqual(rows[0], (2, 2, 1), self.OLD + "saving Monitor must apply it once")
        self.assertEqual(rows[1], (2, 2, 1), "saving the running mode again must not reconfigure the bus")

    def test_waits_for_a_master_request_in_flight(self):
        rows = self.run_script(["stored 1", "running 1", "busy 1", "post 3", "loop", "print",
                                "busy 0", "loop", "print"])
        self.assertEqual(rows[0][0], 1, "must not switch while a master request is in flight")
        self.assertEqual(rows[1][0], 3, self.OLD + "the saved Master must apply once the request is done")

    def test_bypass_without_relay_runs_gateway(self):
        rows = self.run_script(["stored 3", "running 3", "relay 0", "post 0", "loop", "print"])
        self.assertEqual(rows[0][0], 1, self.OLD + "bypass without the relay enabled must run Gateway, as at boot")

    def test_boot_read_does_not_apply(self):
        # readSettings() runs updateSetting() for every key before initOTDirect(); the boot
        # auto-detect may then run Master. The first loop pass must leave that alone.
        rows = self.run_script(["boot 1", "running 3", "loop", "print"])
        self.assertEqual(rows[0], (3, 1, 0), "the settings file read at boot must not switch the mode")

    def test_gw_command_path_does_not_apply_twice(self):
        rows = self.run_script(["stored 1", "running 1", "gw 2", "loop", "print"])
        self.assertEqual(rows[0], (2, 2, 1), "setOTDirectMode() applies once; the loop must not apply again")


if __name__ == "__main__":
    unittest.main()
