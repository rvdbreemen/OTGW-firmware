#!/usr/bin/env python3
"""TASK-1985: the retained <label>/override MQTT topic is removed when the override ends (ADR-118 store goes stale).

Found on the TASK-1812 bench (09, .61, beta.328): OTGW/value/<id>/TSet/override = 40.00 stayed retained on the broker
after CS=0, because publishActiveOverrides() skipped a stale entry and nothing published a clear.

REAL code: recordOTOverride() and publishActiveOverrides() from src/OTGW-firmware/OTGW-Core.ino, OTOverrideEntry_t and
the OVERRIDE_* constants from OTGW-Core.h, compiled with a fake millis() and a sendMQTTData() that records each publish
(and can be told to fail, as with an offline broker). Each case is its own process.
Old-vs-fix: OTGW_CORE_INO=<older OTGW-Core.ino> OTGW_CORE_H=<older OTGW-Core.h> python tests/test_mqtt_override_retained_clear.py
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

REPO = Path(__file__).resolve().parent.parent
FW = REPO / "src" / "OTGW-firmware"
CORE = Path(os.environ.get("OTGW_CORE_INO", FW / "OTGW-Core.ino")).read_text(encoding="utf-8").replace("\r\n", "\n")
HDR = Path(os.environ.get("OTGW_CORE_H", FW / "OTGW-Core.h")).read_text(encoding="utf-8").replace("\r\n", "\n")


def func(src, sig):
    i = src.index(sig)
    j = src.index("{", i)
    depth = 0
    for k in range(j, len(src)):
        if src[k] == "{":
            depth += 1
        elif src[k] == "}":
            depth -= 1
            if depth == 0:
                return src[i:k + 1]
    raise AssertionError(sig)


PRE = r"""
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#define PSTR(s) (s)
#define snprintf_P snprintf
static uint32_t g_now = 1000;
static uint32_t millis() { return g_now; }
static struct { struct { bool bEnable = true; } mqtt; } settings;
enum OTLibMessageID { OT_DUMMY = 0 };
static const char* messageIDToString(OTLibMessageID id) { return id == 1 ? "TSet" : "TdhwSet"; }
static void dtostrf(float v, int, int, char* out) { snprintf(out, 15, "%.2f", v); }
static void feedWatchDog() {}
static bool g_fail = false;
static int g_pubs = 0;
static bool sendMQTTData(const char* topic, const char* payload, bool retain) {
  if (g_fail) return false;
  printf("PUB %u %s [%s] r=%d\n", (unsigned)g_now, topic, payload, retain ? 1 : 0);
  g_pubs++;
  return true;
}
"""

MAIN = r"""
OTOverrideEntry_t otOverrideStore[OVERRIDE_STORE_MAX] = {};
@@FUNCS@@
static void tick(uint32_t ms) { g_now += ms; publishActiveOverrides(); }
int main(int argc, char** argv) {
  const char* c = argv[1];
  if (!strcmp(c, "ends")) {            // CS=40 seen for 2 min, then no more override frames
    recordOTOverride(1, 0, 40.0f); tick(60000); recordOTOverride(1, 0, 40.0f); tick(60000);
    for (int m = 0; m < 14; m++) tick(60000);
  } else if (!strcmp(c, "never")) {    // the override goes stale before any publish (MQTT was down)
    g_fail = true; recordOTOverride(1, 0, 40.0f); tick(60000);
    g_now += OVERRIDE_ACTIVE_TIMEOUT; g_fail = false; tick(60000); tick(60000);
  } else if (!strcmp(c, "offline")) {  // the clear fails once (broker offline), then goes out
    recordOTOverride(1, 0, 40.0f); tick(60000);
    g_now += OVERRIDE_ACTIVE_TIMEOUT; g_fail = true; tick(60000); g_fail = false; tick(60000); tick(60000);
  } else if (!strcmp(c, "active")) {   // an override refreshed every minute is never cleared
    for (int m = 0; m < 30; m++) { recordOTOverride(1, 0, 40.0f); tick(60000); }
  }
  printf("END\n");
  return 0;
}
"""

_tmp = None
_exe = None


def setUpModule():
    global _tmp, _exe
    defs = "\n".join(re.findall(r"^#define OVERRIDE_(?:STORE_MAX|ACTIVE_TIMEOUT)\b.*$", HDR, re.M))
    st = re.search(r"struct OTOverrideEntry_t \{.*?\};", HDR, re.S).group(0)
    funcs = func(CORE, "void recordOTOverride(uint8_t id, uint8_t kind, float value) {") + "\n" + \
        func(CORE, "void publishActiveOverrides() {")
    _tmp = Path(tempfile.mkdtemp(prefix="ovr_clear_"))
    cpp = _tmp / "t.cpp"
    cpp.write_text(PRE + defs + "\n" + st + "\n" + MAIN.replace("@@FUNCS@@", funcs), encoding="utf-8")
    _exe = compile_cpp(cpp, _tmp / ("t.exe" if os.name == "nt" else "t"))
    if _exe is None:
        raise unittest.SkipTest("no C++ compiler")


def tearDownModule():
    if _tmp:
        shutil.rmtree(_tmp, ignore_errors=True)


def pubs(case):
    out = subprocess.run([str(_exe), case], capture_output=True, text=True, check=True).stdout.splitlines()
    assert out[-1] == "END", out
    return [(l.split(" ", 3)[2], l.split("[", 1)[1].split("]", 1)[0]) for l in out if l.startswith("PUB ")]


class RetainedClear(unittest.TestCase):
    def test_an_ended_override_gets_one_empty_retained_message(self):
        p = pubs("ends")
        values = [v for t, v in p if t == "TSet/override"]
        self.assertIn("40.00", values)
        self.assertEqual(values[-1], "", "the last publish on the topic must be the clear")
        self.assertEqual(values.count(""), 1, "exactly one clear, no repeats")

    def test_no_clear_for_an_override_that_was_never_published(self):
        self.assertEqual(pubs("never"), [])

    def test_a_failed_clear_is_sent_again(self):
        values = [v for t, v in pubs("offline") if t == "TSet/override"]
        self.assertEqual(values, ["40.00", ""])

    def test_an_active_override_is_never_cleared(self):
        values = [v for t, v in pubs("active") if t == "TSet/override"]
        self.assertTrue(values)
        self.assertNotIn("", values)


if __name__ == "__main__":
    unittest.main(verbosity=2)
