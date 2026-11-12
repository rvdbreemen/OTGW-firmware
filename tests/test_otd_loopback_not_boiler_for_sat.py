#!/usr/bin/env python3
"""TASK-1856: synthetic loopback answers never count as a real boiler for SAT simulation.

Reproduction (otgw-bench-tester, .61, beta.197): satsimulation stays on in loopback, then 0.2 s after
POST otdmode=3 (loopback -> master) 'satOnBoilerD: boiler appeared on bus -> simulation disabled
completely', before any master-mode reply, and a re-enable is refused until a reboot. Cause: the loopback
path stores its synthetic answers in the boiler cache (otBoilerCacheStore), the cache survives the mode
switch, and otDirectBoilerPresent() read otBoilerCacheValid[3] alone; the SATcontrol.ino backstop
(`if (settings.sat.bSimulation && satBoilerHardwarePresent()) satOnBoilerDetected();`) then tore
simulation down, and the enable gate in updateSetting("SATsimulation") kept refusing.

The code under test is REAL: the presence block of OTDirect.ino (between the otdBoilerPresence markers;
before TASK-1856 the plain otDirectBoilerPresent()), otBoilerCacheStore(), satBoilerHardwarePresent() and the
backstop line of SATcontrol.ino. The backstop calls a stand-in satOnBoilerDetected() that does what its first
lines do (bSimulation = false). Source checks cover the two call sites of the marker.

Old-vs-fix: git show <old-rev>:src/OTGW-firmware/OTDirect.ino > $TMP/OTDirect.ino
            OTGW_OTD_INO=$TMP/OTDirect.ino python tests/test_otd_loopback_not_boiler_for_sat.py
Run: python tests/test_otd_loopback_not_boiler_for_sat.py
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
SAT_INO = REPO / "src" / "OTGW-firmware" / "SATcontrol.ino"
SRC = OTD_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
SAT = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
SET = (REPO / "src" / "OTGW-firmware" / "settingStuff.ino").read_text(encoding="utf-8").replace("\r\n", "\n")


def braced(src, start_pat):
    m = re.search(start_pat, src, re.M)
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


PRE = r"""
#include <cstdio>
#include <cstdint>
#define HAS_PIC 1
#define HAS_DIRECT_OT 1
enum OTDirectMode { OTD_MODE_BYPASS = 0, OTD_MODE_GATEWAY = 1, OTD_MODE_MONITOR = 2, OTD_MODE_MASTER = 3, OTD_MODE_LOOPBACK = 4 };
static OTDirectMode otCurrentMode = OTD_MODE_LOOPBACK;
#define IS_LOOPBACK_MODE() (otCurrentMode == OTD_MODE_LOOPBACK)
static uint16_t otBoilerCache[128];
static bool otBoilerCacheValid[128];
enum class OpenThermMessageType { READ_DATA, WRITE_DATA, INVALID_DATA, RESERVED, READ_ACK, WRITE_ACK, DATA_INVALID, UNKNOWN_DATA_ID };
struct OpenTherm { static OpenThermMessageType getMessageType(unsigned long m) { return (OpenThermMessageType)((m >> 28) & 7); } };
static bool satDebugForceBoilerPresent = false;
static bool isOTDirectEnabled() { return true; }
static bool otRealBoilerSeenRecently() { return false; }
static struct { struct { bool bSimulation = true; } sat; } settings;
static int g_teardown = 0;
#include <cstring>
#define PSTR(s) (s)
#define F(s) (s)
#define strcasecmp_P strcmp
#define EVALBOOLEAN(v) (strcmp((v), "true") == 0 || strcmp((v), "1") == 0)
static void DebugTln(const char*) {}
static void satPostWsEvent_P(char, const char*) {}
static struct { struct { uint32_t iSimLastUpdateMs = 0; bool bSimWarmupDone = false; } sat; } state;
static void satOnBoilerDetected() { if (!settings.sat.bSimulation) return; settings.sat.bSimulation = false; g_teardown++; }
"""

MAIN = r"""
@PRESENCE@
@STORE@
@SATPRESENT@
static void backstop() { @BACKSTOP@ }
static void setSim(const char* field, const char* newValue) { if (false) {} @GATE@ }
#ifndef HAS_MARKER
static void otdNoteMsgId3Source(unsigned long, bool) {}   // the code before TASK-1856 has no marker
#endif
// the two call sites, as they appear in OTDirect.ino (checked by the source test)
static void loopbackStore(unsigned long r) { otBoilerCacheStore(r); otdNoteMsgId3Source(r, false); }
static void realStore(unsigned long r)     { otBoilerCacheStore(r); otdNoteMsgId3Source(r, true); }
int main() {
  const unsigned long ack3 = 0xC0030A0BUL;        // READ_ACK MsgID 3, a slave configuration
  otCurrentMode = OTD_MODE_LOOPBACK;
  for (int i = 0; i < 5; i++) { loopbackStore(ack3); backstop(); }
  printf("loopback present=%d sim=%d\n", otDirectBoilerPresent(), settings.sat.bSimulation);
  otCurrentMode = OTD_MODE_MASTER;                 // POST otdmode=3, before any master-mode reply
  backstop();
  printf("master present=%d sim=%d teardown=%d\n", otDirectBoilerPresent(), settings.sat.bSimulation, g_teardown);
  otCurrentMode = OTD_MODE_GATEWAY;
  backstop();
  printf("gateway present=%d sim=%d\n", otDirectBoilerPresent(), settings.sat.bSimulation);
  setSim("SATsimulation", "false"); setSim("SATsimulation", "true");   // off and on again, no reboot (09's re-enable)
  printf("reenable sim=%d\n", settings.sat.bSimulation);
  realStore(ack3);                                 // a real boiler answers MsgID 3
  printf("real present=%d\n", otDirectBoilerPresent());
  otCurrentMode = OTD_MODE_LOOPBACK; loopbackStore(ack3); otCurrentMode = OTD_MODE_GATEWAY;
  printf("afterloop present=%d\n", otDirectBoilerPresent());
  return 0;
}
"""


def build_source():
    m = re.search(r"// --- otdBoilerPresence begin\n(.*?)// --- otdBoilerPresence end", SRC, re.S)
    presence = m.group(1) if m else braced(SRC, r"^bool otDirectBoilerPresent\(\) \{")
    store = braced(SRC, r"^static void otBoilerCacheStore\(unsigned long response\) \{")
    satp = braced(SAT, r"^bool satBoilerHardwarePresent\(\)")
    bs = re.search(r"^\s*(if \(settings\.sat\.bSimulation && satBoilerHardwarePresent\(\)\) satOnBoilerDetected\(\);)", SAT, re.M)
    gate = braced(SET, r'else if \(strcasecmp_P\(field, PSTR\("SATsimulation"\)\) == 0\) \{')
    assert presence and store and satp and bs and gate, "a slice is missing"
    flag = "#define HAS_MARKER 1\n" if m else ""
    return PRE + flag + MAIN.replace("@PRESENCE@", presence).replace("@STORE@", store) \
        .replace("@SATPRESENT@", satp).replace("@BACKSTOP@", bs.group(1)).replace("@GATE@", gate)


class LoopbackIsNoBoiler(unittest.TestCase):
    rows = None

    @classmethod
    def setUpClass(cls):
        with tempfile.TemporaryDirectory() as d:
            cpp = Path(d) / "t.cpp"
            cpp.write_text(build_source(), encoding="utf-8")
            exe = compile_cpp(cpp, Path(d) / ("t.exe" if os.name == "nt" else "t"))
            if exe is None:
                raise unittest.SkipTest("no host compiler")
            out = subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout
        cls.rows = {l.split()[0]: l for l in out.splitlines()}

    def test_loopback_itself_is_no_boiler(self):
        self.assertIn("present=0 sim=1", self.rows["loopback"])

    def test_simulation_survives_loopback_to_master_like_the_bench_run(self):
        self.assertIn("master present=0 sim=1 teardown=0", self.rows["master"],
                      "loopback answers in the cache must not count as a boiler after the switch: " + self.rows["master"])
        self.assertIn("present=0 sim=1", self.rows["gateway"])

    def test_re_enable_works_without_reboot(self):
        self.assertIn("reenable sim=1", self.rows["reenable"], "the enable gate must accept after loopback -> master: " + self.rows["reenable"])

    def test_a_real_boiler_answer_still_counts(self):
        self.assertIn("present=1", self.rows["real"])

    def test_a_later_loopback_store_withdraws_it(self):
        self.assertIn("present=0", self.rows["afterloop"])


class CallSites(unittest.TestCase):
    def test_loopback_branch_marks_synthetic_and_real_path_marks_real(self):
        loop = braced(SRC, r"^static bool sendMasterRequestAsync\(")
        real = braced(SRC, r"^static void handleMasterResponse\(\)")
        self.assertIn("otdNoteMsgId3Source(response, false)", loop or "")
        self.assertIn("otdNoteMsgId3Source(response, true)", real or "")


if __name__ == "__main__":
    unittest.main(verbosity=2)
