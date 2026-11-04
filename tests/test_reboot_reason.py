#!/usr/bin/env python3
"""TASK-1447 (port of 1.x TASK-1247): reboot_log.txt names why the gateway restarted itself.

The reason store (storeRebootReason / takeRebootReason / captureRebootReason /
storeRebootReasonUnlessPending) and the log-line block of updateRebootLog() are cut out of the real
src/OTGW-firmware/helperStuff.ino (between the "TASK-1447 begin/end" markers and the "TASK-1447: the reason
doRestart() stored" block) and compiled on the host. RTC_NOINIT_ATTR is a plain static here, which is what it is
on the chip: it keeps its value across a software, panic or watchdog reset; a power-on is simulated by filling the
record with garbage. OTGW_HELPER_INO points at another copy for old-vs-fix runs (old code has no markers: red).

Not covered here: that the real chip keeps RTC_NOINIT over esp_restart() and that doRestart(), the portal timeout
and setup() call these functions in the right place. Those are bench checks (reboot_log.txt on the device).

Run: python tests/test_reboot_reason.py
"""
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_published_curve import compile_cpp  # noqa: E402

REPO = Path(__file__).resolve().parent.parent
HELPER_INO = Path(os.environ.get("OTGW_HELPER_INO", REPO / "src" / "OTGW-firmware" / "helperStuff.ino"))

STUBS = r"""
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#define PSTR(s) (s)
#define snprintf_P snprintf
#define RTC_NOINIT_ATTR
#define DebugTf(...) ((void)0)
static size_t strlcpy(char *d, const char *s, size_t n) {
  size_t l = strlen(s);
  if (n) { size_t c = l < n - 1 ? l : n - 1; memcpy(d, s, c); d[c] = 0; }
  return l;
}
static bool g_softReset = false;
static bool platformResetWasSoftware() { return g_softReset; }
"""

# The block of updateRebootLog() that formats the reason, wrapped so it can be called.
LOGFN = r"""
static void buildLine(char *out, size_t outN) {
  char log_line_excpt[64] = {0};   // same size as in updateRebootLog()
%s
  snprintf(out, outN, "reboot cause: Software reset (3) %%s", log_line_excpt);
}
"""

MAIN = r"""
static void powerOn() {
  memset(&g_rtcRebootReason, 0xA5, sizeof(g_rtcRebootReason));   // RTC_NOINIT after power-on: garbage
  g_softReset = false;
}
// One boot: capture at the top of setup(), then (unless the boot restarts first) updateRebootLog().
static void bootCapture(bool soft) { g_softReset = soft; captureRebootReason(); }
static void logLine(const char *tag) {
  char line[160];
  buildLine(line, sizeof(line));
  printf("%s|%s\n", tag, line);
}

int main() {
  // 1. doRestart("nightly"), boot -> logged once; next boot without a store -> nothing.
  powerOn();
  bootCapture(false);  logLine("boot0");
  storeRebootReason("[nightly] scheduled restart");
  bootCapture(true);   logLine("restart1");
  bootCapture(true);   logLine("restart2_repeat");

  // 2. power-on after a stored reason (record garbage / reset reason power-on) -> nothing.
  storeRebootReason("[OTA] Rebooting...");
  powerOn();
  bootCapture(false);  logLine("poweron");

  // 3. crash/WDT during doRestart()'s shutdown: record intact, reset reason is not software -> nothing,
  //    and the record is cleared, so the next software restart does not resurrect it.
  storeRebootReason("Reboot after upgrade");
  bootCapture(false);  logLine("crash");
  bootCapture(true);   logLine("crash_then_soft");

  // 4. chain: WiFi failure restart, portal timeout in the same boot, next boot logs the first reason.
  storeRebootReason("WiFi: too many reconnect failures");
  bootCapture(true);                                   // boot A: has not run updateRebootLog() yet
  storeRebootReasonUnlessPending("WiFi: config portal timeout");
  bootCapture(true);   logLine("chain");               // boot B
  bootCapture(true);   logLine("chain_repeat");

  // 5. portal timeout on a boot that took no reason: its own reason is stored.
  powerOn();
  bootCapture(false);
  storeRebootReasonUnlessPending("WiFi: config portal timeout");
  bootCapture(true);   logLine("portal_only");

  // 5b. stale record, then a crash reset (not software), then a portal timeout in that boot: the stale
  //     reason must not be handed on; the portal timeout names itself.
  storeRebootReason("Reboot after upgrade");
  bootCapture(false);
  storeRebootReasonUnlessPending("WiFi: config portal timeout");
  bootCapture(true);   logLine("stale_then_portal");

  // 6. a reason longer than the record is cut and terminated, never overruns.
  storeRebootReason("0123456789012345678901234567890123456789012345678901234567890123456789");
  bootCapture(true);   logLine("long");
  printf("longlen|%d\n", (int)strlen(g_bootRestartReason));

  // 7. null reason.
  storeRebootReason(nullptr);
  bootCapture(true);   logLine("null");
  return 0;
}
"""


class RebootReason(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        src = HELPER_INO.read_text(encoding="utf-8")
        a = src.find("// TASK-1447 begin")
        b = src.find("// TASK-1447 end")
        c = src.find("  // TASK-1447: the reason doRestart() stored")
        d = src.find("g_bootRestartReasonValid = false;", c)
        if min(a, b, c, d) < 0:
            raise AssertionError("TASK-1447 reason store not found in helperStuff.ino: doRestart() reason is lost")
        d = src.find("\n", d) + 1
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text(STUBS + src[a:b] + LOGFN % src[c:d] + MAIN, encoding="utf-8")
        exe = compile_cpp(cpp, Path(cls.tmp.name) / "t.exe")
        if exe is None:
            raise unittest.SkipTest("no host C++ compiler")
        out = subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout
        print(out)
        cls.rows = dict(l.split("|", 1) for l in out.splitlines())

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_reason_after_doRestart_is_logged(self):
        self.assertEqual(self.rows["restart1"], "reboot cause: Software reset (3) - [nightly] scheduled restart")

    def test_reason_is_reported_once(self):
        self.assertEqual(self.rows["restart2_repeat"], "reboot cause: Software reset (3) ")

    def test_power_on_has_no_reason(self):
        self.assertEqual(self.rows["boot0"], "reboot cause: Software reset (3) ")
        self.assertEqual(self.rows["poweron"], "reboot cause: Software reset (3) ")

    def test_crash_during_restart_has_no_reason_and_clears_it(self):
        self.assertEqual(self.rows["crash"], "reboot cause: Software reset (3) ")
        self.assertEqual(self.rows["crash_then_soft"], "reboot cause: Software reset (3) ")

    def test_portal_timeout_chain_keeps_the_first_reason_once(self):
        self.assertEqual(self.rows["chain"],
                         "reboot cause: Software reset (3) - WiFi: too many reconnect failures")
        self.assertEqual(self.rows["chain_repeat"], "reboot cause: Software reset (3) ")

    def test_portal_timeout_alone_stores_its_own_reason(self):
        self.assertEqual(self.rows["portal_only"], "reboot cause: Software reset (3) - WiFi: config portal timeout")

    def test_stale_record_is_not_handed_on_by_portal_timeout(self):
        self.assertEqual(self.rows["stale_then_portal"],
                         "reboot cause: Software reset (3) - WiFi: config portal timeout")

    def test_call_sites_are_wired(self):
        # Textual wiring check (the host harness cannot run setup()/startWiFi()).
        net = (HELPER_INO.parent / "networkStuff.ino").read_text(encoding="utf-8")
        i = net.find('storeRebootReasonUnlessPending("WiFi: config portal timeout")')
        self.assertGreater(i, 0, "portal timeout does not store its reason")
        self.assertIn("platformRestart();", net[i:i + 200])
        self.assertGreater(i, net.rfind("startConfigPortal", 0, i))
        ino = (HELPER_INO.parent / "OTGW-firmware.ino").read_text(encoding="utf-8")
        s = ino.find("void setup() {")
        self.assertGreater(s, 0)
        first = ino[s:].replace("\r", "").split("\n")[1].strip()
        self.assertTrue(first.startswith("captureRebootReason();"), first)

    def test_long_and_null_reasons_are_safe(self):
        self.assertEqual(self.rows["longlen"], "47")
        self.assertTrue(self.rows["long"].endswith("- 01234567890123456789012345678901234567890123456"))
        self.assertEqual(self.rows["null"], "reboot cause: Software reset (3) - ?")


if __name__ == "__main__":
    unittest.main(verbosity=2)
