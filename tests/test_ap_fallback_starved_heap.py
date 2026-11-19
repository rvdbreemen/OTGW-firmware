#!/usr/bin/env python3
"""TASK-2923: the beta AP fallback is not started on a starved heap.

09's bench on .61 (2.0.0-beta.393+4bb2097, TASK-2026 heap hold) panicked twice in the wifi task: LoadProhibited at
NULL+0x2c in ieee80211_hostap_attach, reached from loopWifi() -> startAPFallback() -> WiFi.mode(WIFI_AP_STA) after two
failed connect attempts. The REAL 'case WIFI_CONNECTING:' block of networkStuff.ino (and wifiApFallbackHeapOk() when
it exists) is compiled on the host against stubs and driven through failed attempts:
- starved heap (free 6000, largest block 1800, as under the hold): no softAP, the skip line, and the release path
  (WIFI_FAILED, i.e. a clean restart, after 10 attempts). Red on 4bb2097ed: the AP fallback starts after 2 attempts.
- healthy heap: the AP fallback still starts after 2 attempts (beta behaviour unchanged).
- settings.wifi.bNoPortal on a starved heap: keeps retrying, no restart (TASK-1645 unchanged).
Run: python tests/test_ap_fallback_starved_heap.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from test_sat_pwm_override import compile_cpp  # noqa: E402

SRC = (HERE.parent / "src" / "OTGW-firmware" / "networkStuff.ino").read_text(encoding="utf-8").replace("\r\n", "\n")


def connecting_case():
    a = SRC.index("    case WIFI_CONNECTING:")
    b = SRC.index("    case WIFI_RECONNECTED:", a)
    return SRC[a:b]


def helper():
    m = re.search(r"static bool wifiApFallbackHeapOk\(.*?\n\}\n", SRC, re.S)
    return m.group(0) if m else ""


def define(name):
    m = re.search(r"#define\s+%s\s+(\d+)" % name, SRC)
    return m.group(1) if m else None


PRE = r"""
#include <cstdio>
#include <cstdint>
#include <cstring>
#define _VERSION_PRERELEASE beta.0
#define F(s) (s)
#define PSTR(s) (s)
#define DebugTln(s) printf("%s\n", s)
#define DebugTf(...) printf(__VA_ARGS__)
#define HEAP_LOW_THRESHOLD 8192
#define WIFI_STARVED_MAXBLOCK __MAXBLK__
#define WL_CONNECTED 3
#define DUE(t) (true)
#define RESTART_TIMER(t) ((void)0)
static int timerWifiRetry, timerAPRetry;
struct { int status() { return 6; } } WiFi;            // never connected: every attempt fails
struct { struct { bool bNoPortal; } wifi; } settings;
static uint32_t g_free, g_blk;
static uint32_t platformFreeHeap() { return g_free; }
static uint32_t platformMaxFreeBlock() { return g_blk; }
static int g_apStarts = 0;
static void startAPFallback() { g_apStarts++; }
enum WifiState_t { WIFI_IDLE, WIFI_DISCONNECTED, WIFI_CONNECTING, WIFI_RECONNECTED, WIFI_FAILED, WIFI_AP_FALLBACK, WIFI_AP_RETRY };
static WifiState_t wifiState = WIFI_CONNECTING;
static int wifiRetryCount = 0;
"""

MAIN = r"""
static void step() {
  switch (wifiState) {
__CASE__
    default: break;
  }
}
int main(int argc, char** argv) {
  g_free = (uint32_t)atoi(argv[1]); g_blk = (uint32_t)atoi(argv[2]); settings.wifi.bNoPortal = atoi(argv[3]) != 0;
  for (int i = 0; i < 12; i++) {
    wifiState = WIFI_CONNECTING; step();
    printf("attempt %d state=%d ap=%d\n", wifiRetryCount, (int)wifiState, g_apStarts);
    if (wifiState == WIFI_AP_FALLBACK || wifiState == WIFI_FAILED) break;
  }
  return 0;
}
"""


class ApFallbackStarvedHeap(unittest.TestCase):
    exe = None

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "a.cpp"
        code = (PRE.replace("__MAXBLK__", define("WIFI_STARVED_MAXBLOCK") or "4096") + "#include <cstdlib>\n"
                + helper() + MAIN.replace("__CASE__", connecting_case()))
        cpp.write_text(code, encoding="utf-8")
        cls.exe = compile_cpp(cpp, Path(cls.tmp.name) / ("a.exe" if os.name == "nt" else "a"))
        if cls.exe is None:
            raise unittest.SkipTest("no host compiler")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_c(self, free, blk, noportal):
        out = subprocess.run([str(self.exe), str(free), str(blk), str(noportal)], capture_output=True,
                             check=True).stdout.decode().replace("\r", "")
        rows = [dict(kv.split("=") for kv in l.split()[2:]) | {"n": l.split()[1]}
                for l in out.splitlines() if l.startswith("attempt ")]
        return rows, out

    def test_a_starved_heap_skips_the_ap_and_restarts_as_release(self):
        rows, out = self.run_c(6000, 1800, 0)
        self.assertEqual(rows[-1]["ap"], "0", "softAP started on a starved heap: " + out)
        self.assertIn("BETA: AP fallback skipped, heap starved (free 6000 B, largest block 1800 B)", out)
        self.assertEqual(rows[-1]["state"], "4", "no WIFI_FAILED (clean restart) after 10 attempts: " + out)
        self.assertEqual(rows[-1]["n"], "10", out)

    def test_a_healthy_heap_still_starts_the_ap_after_two_attempts(self):
        rows, out = self.run_c(40000, 30000, 0)
        self.assertEqual(rows[-1]["ap"], "1", out)
        self.assertEqual(rows[-1]["state"], "5", out)
        self.assertEqual(rows[-1]["n"], "2", out)
        self.assertNotIn("AP fallback skipped", out)

    def test_low_free_with_a_large_block_is_also_starved(self):
        rows, out = self.run_c(7000, 6000, 0)
        self.assertEqual(rows[-1]["ap"], "0", out)

    def test_no_portal_keeps_retrying_without_a_restart(self):
        rows, out = self.run_c(6000, 1800, 1)
        self.assertEqual(len(rows), 12, out)
        self.assertEqual(rows[-1]["state"], "1", "bNoPortal restarted or left WIFI_DISCONNECTED: " + out)
        self.assertEqual(rows[-1]["ap"], "0", out)
        self.assertNotIn("AP fallback skipped", out)


if __name__ == "__main__":
    unittest.main(verbosity=2)
