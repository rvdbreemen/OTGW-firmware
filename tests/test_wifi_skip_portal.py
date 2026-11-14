#!/usr/bin/env python3
"""TASK-1455 (1.x skip-portal port, ADR-211): the boot after a WiFi supervisor restart skips the config portal.

Part 1 compiles the real boot path on the host: the skip-portal block (between the "TASK-1455 begin/end" markers)
and the real startWiFi() are cut out of src/OTGW-firmware/networkStuff.ino and built against a fake WiFi station,
a fake WiFiManager, an RTC_NOINIT struct that is a plain static, and a settable reset reason. Each run is one boot:
the RTC record is loaded from the command line, captureWifiSkipPortal() runs as at the top of setup(), then
startWiFi(). A fake config portal that times out restarts the board (the process ends with RESTART), as on the
device. Built twice: release (no _VERSION_PRERELEASE) and prerelease (AP fallback).

Part 2 drives the real WiFi state machine of tests/test_wifi_deadlink.py (cut from "enum WifiState_t {" through
loopWifi()) and checks that both supervisor restarts (dead link and WIFI_FAILED) write the mark, and that no other
path does.

Part 3 is source gates: the record is RTC_NOINIT, no NVS/flash write on the mark path, the mark sits right before
both supervisor doRestart() calls, and setup() takes it next to the TASK-1447/TASK-1450 records.

Old-vs-fix: OTGW_NETWORK_INO points at another networkStuff.ino (git show HEAD~:... > old.ino). The old code has no
mark: the supervisor-restart boot opens the portal and restarts again, and the restart sites write nothing.

Not covered here: WiFiManager itself, the real WiFi driver, RTC_NOINIT surviving esp_restart() on the chip, and the
services (telnet, HTTP, WS, MQTT, NTP, mDNS) coming back after an offline setup(). Those are bench checks.

Run: python tests/test_wifi_skip_portal.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_published_curve import compile_cpp  # noqa: E402
import test_wifi_deadlink as dl  # noqa: E402

REPO = Path(__file__).resolve().parent.parent
FW_DIR = Path(os.environ.get("OTGW_FW_DIR", REPO / "src" / "OTGW-firmware"))
NET_INO = Path(os.environ.get("OTGW_NETWORK_INO", FW_DIR / "networkStuff.ino"))
TIMERS = (REPO / "src" / "OTGW-firmware" / "safeTimers.h").as_posix()

SKIP_MAGIC = 0x4F545350   # "OTSP"
DEAD_MAGIC = 0x4F544452   # "OTDR" (TASK-1450)
NEVER = 0                 # the access point stays away

# ---------------------------------------------------------------------------------------------- part 1: boot path
BOOT_STUBS = r"""
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <cstdarg>
typedef uint8_t byte;
static uint32_t g_ms = 0;
static uint32_t millis() { return g_ms; }
static long random(long a, long) { return a; }
static void delay(uint32_t ms) { g_ms += ms; }
static void feedWatchDog() {}
#include "@TIMERS@"
#define F(s) (s)
#define PSTR(s) (s)
#define RTC_NOINIT_ATTR
#define HAS_ETH_CAPABLE 1
typedef std::string String;
static size_t t_strlcpy(char *d, const char *s, size_t n) { size_t l = strlen(s); if (n) { size_t c = l < n - 1 ? l : n - 1; memcpy(d, s, c); d[c] = 0; } return l; }
static size_t t_strlcat(char *d, const char *s, size_t n) { size_t l = strlen(d); return l + t_strlcpy(d + l, s, n > l ? n - l : 0); }
#define strlcpy t_strlcpy
#define strlcat t_strlcat
#define snprintf_P snprintf
#define DebugTf(...)  (printf("LOG|%u|", (unsigned)(g_ms / 1000)), printf(__VA_ARGS__), printf("\n"))
#define DebugTln(s)   printf("LOG|%u|%s\n", (unsigned)(g_ms / 1000), (const char *)(s))
template <class... A> static void DebugT(A...) {}
template <class... A> static void Debugln(A...) {}

enum NetMode { NET_WIFI, NET_ETHERNET };
struct NetState { NetMode eMode = NET_WIFI; bool bAPFallback = false; };
struct State { NetState net; };
struct WifiSettings { char sStaticIp[16] = ""; char sSubnet[16] = ""; char sGateway[16] = ""; char sDns1[16] = ""; char sDns2[16] = ""; bool bNoPortal = false; };
struct Settings { WifiSettings wifi; char sHTTPpasswd[16] = ""; };
static State state; static Settings settings;

static bool g_soft = false;
static bool platformResetWasSoftware() { return g_soft; }

// ---- fake station: connects once WiFi.begin() ran and the access point is back -------------------
enum { WL_IDLE_STATUS = 0, WL_CONNECTED = 3, WL_DISCONNECTED = 6 };
enum { WIFI_STA = 1 };
static bool     g_begun = false;
static uint32_t g_apUpAt = 0;              // ms; 0 = never
static int      g_auto = -1, g_pers = -1, g_sleepOff = 0, g_ap = 0, g_portal = 0, g_reset = 0;
struct FakeIp { std::string toString() const { return "0.0.0.0"; } };
struct IPAddress { bool fromString(const char *) { return true; } };
struct FakeWiFi {
  void mode(int) {}
  int status() { return (g_begun && g_apUpAt != 0 && g_ms >= g_apUpAt) ? WL_CONNECTED : WL_DISCONNECTED; }
  void begin() { g_begun = true; }
  FakeIp localIP() { return FakeIp(); }
  FakeIp gatewayIP() { return FakeIp(); }
  std::string SSID() { return "lab"; }
  void setAutoReconnect(bool b) { g_auto = b; }
  void persistent(bool b) { g_pers = b; }
  void config(IPAddress, IPAddress, IPAddress, IPAddress, IPAddress) {}
};
static FakeWiFi WiFi;
static void wifiLinkEventsBegin() {}
static void platformIgnoreDhcpNtp() {}
static void platformWifiPreferStrongestAp() {}
static void platformGetMacAddress(uint8_t *m) { memset(m, 0xAB, 6); }
static std::string platformGetHostname() { return "otgw"; }
static void platformSetHostname(const char *) {}
static void platformWifiDisableSleep() { g_sleepOff++; }
static void storeRebootReasonUnlessPending(const char *r) { printf("REASON|%s\n", r); }
static void platformRestart() { printf("RESTART|%u|portal timeout\n", (unsigned)(g_ms / 1000)); fflush(stdout); exit(0); }
struct FakeUpdater { void updateCredentials(const char *, const char *) {} };
static FakeUpdater httpUpdater;
static void startAPFallback() { g_ap++; state.net.bAPFallback = true; printf("EV|%u|ap_fallback\n", (unsigned)(g_ms / 1000)); }

// ---- fake WiFiManager: the portal runs its whole timeout (the station is off meanwhile) ----------
static bool g_saved = true;
struct WiFiManager {
  int tmo = 0;
  void setDebugOutput(bool) {}
  void setAPCallback(void (*)(WiFiManager *)) {}
  void setConfigPortalTimeout(int t) { tmo = t; }
  void setShowInfoUpdate(bool) {}
  void setShowInfoErase(bool) {}
  template <class T> void setMenu(T &) {}
  void setHostname(const char *) {}
  bool getWiFiIsSaved() { return g_saved; }
  void resetSettings() { g_reset++; g_saved = false; }
  bool startConfigPortal(const char *) {
    g_portal++;
    printf("EV|%u|portal\n", (unsigned)(g_ms / 1000));
    g_ms += (uint32_t)tmo * 1000UL;
    return false;   // nobody configures it and the station cannot rejoin while it runs
  }
};
static void configModeCallback(WiFiManager *) {}
"""

BOOT_MAIN = r"""
#ifndef TASK1455
struct { uint32_t magic, marked; } g_rtcWifiSkipPortal = {0, 0};
static void captureWifiSkipPortal() {}
static void wifiMarkSkipPortal() {}
#endif

static void done(const char *tag, uint32_t t0) {
  printf("%s|%u|portal=%d|ap=%d|auto=%d|pers=%d|sleep=%d|reset=%d|conn=%d\n", tag, (unsigned)((g_ms - t0) / 1000),
         g_portal, g_ap, g_auto, g_pers, g_sleepOff, g_reset, WiFi.status() == WL_CONNECTED ? 1 : 0);
}

// argv: mode magic marked soft saved force apUpSec eth
int main(int argc, char **argv) {
  if (argc < 9) return 2;
  const char *mode = argv[1];
  g_rtcWifiSkipPortal.magic  = (uint32_t)strtoul(argv[2], nullptr, 10);
  g_rtcWifiSkipPortal.marked = (uint32_t)strtoul(argv[3], nullptr, 10);
  g_soft  = atoi(argv[4]) != 0;
  g_saved = atoi(argv[5]) != 0;
  const bool force = atoi(argv[6]) != 0;
  g_apUpAt = (uint32_t)strtoul(argv[7], nullptr, 10) * 1000UL;
  state.net.eMode = atoi(argv[8]) ? NET_ETHERNET : NET_WIFI;
  if (argc > 9) settings.wifi.bNoPortal = atoi(argv[9]) != 0;   // TASK-1645 opt-in setting

  if (!strcmp(mode, "mark")) {   // the supervisor writes the mark before its doRestart()
    wifiMarkSkipPortal();
    printf("RTC|%lu|%lu\n", (unsigned long)g_rtcWifiSkipPortal.magic, (unsigned long)g_rtcWifiSkipPortal.marked);
    return 0;
  }
  g_ms = 5000;                   // setup() reached its top
  captureWifiSkipPortal();
  printf("RTC|%lu|%lu\n", (unsigned long)g_rtcWifiSkipPortal.magic, (unsigned long)g_rtcWifiSkipPortal.marked);
  if (!strcmp(mode, "nextboot")) {   // the boot after this one: take the record again
    g_soft = true;
    captureWifiSkipPortal();
  }
  uint32_t t0 = g_ms;
  startWiFi("otgw", 240, force);
  done("DONE", t0);
  if (!strcmp(mode, "again")) {      // a second startWiFi() in the same boot (telnet debug menu)
    t0 = g_ms;
    startWiFi("otgw", 240, false);
    done("DONE2", t0);
  }
  return 0;
}
"""


def cut_between(src, begin, end):
    a, b = src.find(begin), src.find(end)
    if a < 0 or b < 0:
        return None
    return src[a:b + len(end)]


def cut_startwifi(src):
    a = src.find("void startWiFi(const char* hostname, int timeOut, bool forcePortal)")
    b = src.find("} // startWiFi()")
    if a < 0 or b < 0:
        raise AssertionError("startWiFi() not found in networkStuff.ino")
    return src[a:b + len("} // startWiFi()")]


def parse(out):
    rows = {"EV": [], "LOG": []}
    for line in out.splitlines():
        parts = line.strip().split("|")
        k = parts[0]
        if k == "EV":
            rows["EV"].append(parts[2])
        elif k == "LOG":
            rows["LOG"].append("|".join(parts[2:]))
        elif k == "RTC":
            rows["RTC"] = (int(parts[1]), int(parts[2]))
        elif k in ("DONE", "DONE2"):
            d = {p.split("=")[0]: int(p.split("=")[1]) for p in parts[2:]}
            d["sec"] = int(parts[1])
            rows[k] = d
        elif k == "RESTART":
            rows["RESTART"] = int(parts[1])
        elif k == "REASON":
            rows["REASON"] = parts[1]
    return rows


class SkipPortalBoot(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        src = NET_INO.read_text(encoding="utf-8")
        block = cut_between(src, "// TASK-1455 begin", "// TASK-1455 end")
        cls.has_fix = block is not None
        body = (block or "") + "\n" + cut_startwifi(src)
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = {}
        for name, defs in (("rel", ""), ("pre", "#define _VERSION_PRERELEASE \"beta.1\"\n")):
            cpp = Path(cls.tmp.name) / ("b_%s.cpp" % name)
            head = defs + ("#define TASK1455 1\n" if cls.has_fix else "")
            cpp.write_text(head + BOOT_STUBS.replace("@TIMERS@", TIMERS) + body + BOOT_MAIN, encoding="utf-8")
            exe = compile_cpp(cpp, Path(cls.tmp.name) / ("b_%s.exe" % name))
            if exe is None:
                raise unittest.SkipTest("no host C++ compiler")
            cls.exe[name] = exe

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def boot(self, mode="boot", magic=0, marked=0, soft=1, saved=1, force=0, ap_up=NEVER, eth=0, build="rel", noportal=0):
        args = [str(self.exe[build]), mode, str(magic), str(marked), str(soft), str(saved), str(force), str(ap_up), str(eth),
                str(noportal)]
        out = subprocess.run(args, capture_output=True, text=True, check=True).stdout
        return parse(out), out

    def assert_portal(self, r):
        self.assertIn("portal", r["EV"], "the config portal did not open")
        self.assertIn("RESTART", r, "the portal timeout did not restart")

    # AC1: supervisor restart, AP still away -> no portal, setup() goes on within the direct-connect window
    def test_supervisor_restart_ap_away_skips_portal(self):
        r, out = self.boot(magic=SKIP_MAGIC, marked=1)
        print(out[-800:])
        self.assertNotIn("portal", r["EV"], "portal opened on the boot after a supervisor restart")
        self.assertNotIn("RESTART", r)
        self.assertIn("DONE", r)
        self.assertLessEqual(r["DONE"]["sec"], 121, "setup() blocked past the 120 s direct-connect window")
        self.assertEqual(r["DONE"]["conn"], 0)
        self.assertEqual((r["DONE"]["auto"], r["DONE"]["pers"], r["DONE"]["sleep"]), (1, 1, 1),
                         "auto-reconnect, persistent or radio-awake not set on the skipped boot")
        self.assertTrue(any("skipping the config portal" in s for s in r["LOG"]), "no skip log line")

    # The access point returns inside the direct-connect window: connected, no portal, mark consumed.
    def test_ap_back_during_skipped_boot_window_connects(self):
        r, _ = self.boot(magic=SKIP_MAGIC, marked=1, ap_up=60)
        self.assertNotIn("portal", r["EV"])
        self.assertEqual(r["DONE"]["conn"], 1)
        self.assertEqual(r["RTC"], (0, 0))

    # AC3: power-on (RTC garbage, even garbage that matches) -> portal
    def test_power_on_with_matching_garbage_opens_portal(self):
        r, _ = self.boot(magic=SKIP_MAGIC, marked=1, soft=0)
        self.assert_portal(r)
        self.assertEqual(r["RTC"], (0, 0), "the record is not cleared on a power-on")

    def test_power_on_with_random_garbage_opens_portal(self):
        r, _ = self.boot(magic=0xDEADBEEF, marked=0x12345678, soft=0)
        self.assert_portal(r)

    def test_old_rtc_layout_after_the_update_opens_portal(self):
        # TASK-1918: the new firmware keeps its RTC records 0x20 B higher (safeboot guard at 0x50000200). On the
        # first boot after the update this record (new +0x5c) reads the old FS-OTA snapshot's data[8..16):
        # zeros after a wipe, or settings.ini text. Neither may skip the portal.
        head = (REPO / "src" / "OTGW-firmware" / "data" / "settings.ini").read_bytes()[:16]
        for blob in (bytes(16), head):
            with self.subTest(blob=blob):
                r, _ = self.boot(magic=int.from_bytes(blob[8:12], "little"),
                                 marked=int.from_bytes(blob[12:16], "little"))
                self.assert_portal(r)

    def test_wrong_value_with_magic_opens_portal(self):
        r, _ = self.boot(magic=SKIP_MAGIC, marked=7)
        self.assert_portal(r)

    # AC3: first install, no credentials -> portal
    def test_no_saved_credentials_opens_portal(self):
        r, _ = self.boot(magic=SKIP_MAGIC, marked=1, saved=0)
        self.assert_portal(r)

    # AC3: triple reset -> credentials cleared and portal, the mark does not override it
    def test_triple_reset_opens_portal(self):
        r, _ = self.boot(magic=SKIP_MAGIC, marked=1, force=1)
        self.assert_portal(r)
        self.assertNotIn("DONE", r)

    def test_triple_reset_clears_credentials(self):
        # force=1 runs resetSettings() before the portal; the RESTART ends the run, so check the log line
        _, out = self.boot(magic=SKIP_MAGIC, marked=1, force=1)
        self.assertIn("Triple-reset detected", out)

    # AC4: a software restart that was not a supervisor restart (nightly, settings, REST) -> as before
    def test_other_software_restart_opens_portal(self):
        r, _ = self.boot(magic=0, marked=0, soft=1)
        self.assert_portal(r)

    # AC4: exactly one boot
    def test_mark_is_consumed_at_boot(self):
        r, _ = self.boot(magic=SKIP_MAGIC, marked=1)
        self.assertEqual(r["RTC"], (0, 0), "the mark is still in RTC after capture")

    def test_next_boot_without_new_mark_opens_portal(self):
        r, _ = self.boot(mode="nextboot", magic=SKIP_MAGIC, marked=1)
        self.assert_portal(r)

    def test_second_startwifi_in_same_boot_opens_portal(self):
        r, _ = self.boot(mode="again", magic=SKIP_MAGIC, marked=1)
        self.assertIn("DONE", r)
        self.assertEqual(r["DONE"]["portal"], 0)
        self.assertEqual(r["EV"].count("portal"), 1, "the telnet re-run of startWiFi() should open the portal")
        self.assertIn("RESTART", r)

    # AC7 decision: in a prerelease build the mark also bypasses the boot-time AP fallback
    def test_prerelease_marked_boot_skips_ap_fallback(self):
        r, _ = self.boot(magic=SKIP_MAGIC, marked=1, build="pre")
        self.assertNotIn("ap_fallback", r["EV"])
        self.assertNotIn("portal", r["EV"])
        self.assertEqual(r["DONE"]["ap"], 0)
        self.assertLessEqual(r["DONE"]["sec"], 121)

    def test_prerelease_unmarked_boot_keeps_ap_fallback(self):
        r, _ = self.boot(build="pre")
        self.assertIn("ap_fallback", r["EV"])
        self.assertNotIn("portal", r["EV"])

    def test_prerelease_triple_reset_opens_portal(self):
        r, _ = self.boot(magic=SKIP_MAGIC, marked=1, force=1, build="pre")
        self.assert_portal(r)

    # AC8: Ethernet active -> no mark; WiFi active -> mark
    def test_mark_written_on_wifi(self):
        r, _ = self.boot(mode="mark", eth=0)
        self.assertEqual(r["RTC"], (SKIP_MAGIC, 1))

    def test_no_mark_while_ethernet_active(self):
        r, _ = self.boot(mode="mark", eth=1)
        self.assertEqual(r["RTC"], (0, 0))


# --------------------------------------------------------------------------------- part 2: the restart sites
SKIP_DUMP = r"""
#ifdef TASK1455
static void printSkipMark() { printf("SKIP|%lu-%lu\n", (unsigned long)g_rtcWifiSkipPortal.magic, (unsigned long)g_rtcWifiSkipPortal.marked); }
#else
static void printSkipMark() { printf("SKIP|0-0\n"); }
#endif
"""
DUMP_ANCHOR = 'static void dumpRtc(const char *tag) {\n  printf("%s|%lu|%lu\\n", tag,'
EXTRA_SCENARIOS = r"""  } else if (!strcmp(sc, "ap_gone")) {     // TASK-1455: the access point disappears (no association)
    runFor(5 * 60); ev("ap_gone"); g_wifiStatus = WL_DISCONNECTED; g_dhcpWorks = false; runFor(15 * 60);
  } else if (!strcmp(sc, "ap_gone_noportal")) {   // TASK-1645: the same with settings.wifi.bNoPortal on
    settings.wifi.bNoPortal = true; runFor(5 * 60); ev("ap_gone"); g_wifiStatus = WL_DISCONNECTED; g_dhcpWorks = false; runFor(15 * 60);
  } else if (!strcmp(sc, "ap_gone_wired")) {   // the same while Ethernet is the active link
    runFor(5 * 60); state.net.eMode = NET_ETHERNET; g_wifiStatus = WL_DISCONNECTED; g_dhcpWorks = false; runFor(15 * 60);
  } else if (!strcmp(sc, "alive")) {"""


class SkipPortalRestartSites(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        src = NET_INO.read_text(encoding="utf-8")
        cut = dl.cut_state_machine(src)
        cls.has_fix = "// TASK-1455 begin" in cut
        main = dl.STATE_MAIN
        if DUMP_ANCHOR not in main or '} else if (!strcmp(sc, "alive")) {' not in main:
            raise AssertionError("test_wifi_deadlink.py harness changed; update the anchors")
        main = main.replace(DUMP_ANCHOR, DUMP_ANCHOR.replace("{\n", "{\n  printSkipMark();\n", 1))
        main = main.replace('  } else if (!strcmp(sc, "alive")) {', EXTRA_SCENARIOS, 1)
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = {}
        for name, defs in (("pre", "#define _VERSION_PRERELEASE \"beta.1\"\n"), ("rel", "")):
            head = defs + "#define TASK1450 1\n" + ("#define TASK1455 1\n" if cls.has_fix else "")
            cpp = Path(cls.tmp.name) / ("s_%s.cpp" % name)
            cpp.write_text(head + dl.STATE_STUBS.replace("@TIMERS@", TIMERS) + cut + SKIP_DUMP + main, encoding="utf-8")
            exe = compile_cpp(cpp, Path(cls.tmp.name) / ("s_%s.exe" % name))
            if exe is None:
                raise unittest.SkipTest("no host C++ compiler")
            cls.exe[name] = exe

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_sc(self, sc, build="rel", magic=0, count=0, soft=0):
        out = subprocess.run([str(self.exe[build]), sc, str(magic), str(count), str(soft), "1"],
                             capture_output=True, text=True, check=True).stdout
        self.last_out = out
        skip = re.search(r"^SKIP\|(\d+)-(\d+)", out, re.M)
        restart = re.search(r"^RESTART\|\d+\|(.*)$", out, re.M)
        return (int(skip.group(1)), int(skip.group(2))), (restart.group(1).strip() if restart else None)

    def test_wifi_failed_restart_marks(self):
        mark, reason = self.run_sc("ap_gone")
        self.assertEqual(reason, "WiFi: too many reconnect failures")
        self.assertEqual(mark, (SKIP_MAGIC, 1), "WIFI_FAILED restart did not mark the next boot")

    def test_dead_link_restart_marks(self):
        for build in ("pre", "rel"):
            mark, reason = self.run_sc("dead", build=build)
            self.assertEqual(reason, "WiFi: connected but no traffic", build)
            self.assertEqual(mark, (SKIP_MAGIC, 1), "%s: dead-link restart did not mark the next boot" % build)

    def test_failed_forced_reconnect_restart_marks(self):
        mark, reason = self.run_sc("dead_no_dhcp")
        self.assertEqual(reason, "WiFi: too many reconnect failures")
        self.assertEqual(mark, (SKIP_MAGIC, 1))

    def test_no_restart_no_mark(self):
        for sc, kw in (("alive", {}), ("dead_no_dhcp", dict(magic=DEAD_MAGIC, count=3, soft=1))):
            mark, reason = self.run_sc(sc, **kw)
            self.assertIsNone(reason, sc)
            self.assertEqual(mark, (0, 0), "%s: mark written without a supervisor restart" % sc)

    def test_prerelease_ap_gone_goes_to_ap_fallback_without_mark(self):
        mark, reason = self.run_sc("ap_gone", build="pre")
        self.assertIsNone(reason)
        self.assertEqual(mark, (0, 0))

    # TASK-1645 bench .61 2026-11-05: with wifinoportal on, loopWifi still opened the BETA AP fallback after 2 retries
    def test_prerelease_noportal_setting_never_opens_ap_fallback(self):
        self.run_sc("ap_gone", build="pre")
        self.assertRegex(self.last_out, r"\|ap=[1-9]", "control: without the setting the BETA AP fallback must start")
        mark, reason = self.run_sc("ap_gone_noportal", build="pre")
        self.assertRegex(self.last_out, r"\|ap=0\|", "wifinoportal on: loopWifi started the AP fallback")
        self.assertIsNone(reason, "wifinoportal on: the prerelease build must keep retrying, not restart")

    def test_wired_unit_neither_restarts_nor_marks(self):
        mark, reason = self.run_sc("ap_gone_wired")
        self.assertIsNone(reason)
        self.assertEqual(mark, (0, 0))


# ------------------------------------------------------------------------------------------- part 3: source gates
def strip_comments(text):
    return "\n".join(l.split("//", 1)[0] for l in text.replace("\r", "").splitlines())


class SkipPortalSourceGates(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.net = NET_INO.read_text(encoding="utf-8").replace("\r", "")
        cls.block = cut_between(cls.net, "// TASK-1455 begin", "// TASK-1455 end")

    def test_record_is_rtc_noinit_and_never_nvs(self):
        self.assertIsNotNone(self.block, "TASK-1455 block not found")
        code = strip_comments(self.block)
        self.assertRegex(code, r"static RTC_NOINIT_ATTR WifiSkipPortalRtc g_rtcWifiSkipPortal;")
        for bad in ("Preferences", "platformRtcWrite", "nvs_", "LittleFS", "writeSettings", "EEPROM", "File "):
            self.assertNotIn(bad, code, "flash/NVS access in the skip-portal block: %s" % bad)

    def test_mark_sits_right_before_both_supervisor_restarts(self):
        for reason in ("WiFi: connected but no traffic", "WiFi: too many reconnect failures"):
            i = self.net.find('doRestart("%s");' % reason)
            self.assertGreater(i, 0, reason)
            prev = [l.strip() for l in self.net[:i].split("\n")[:-1] if l.strip()][-1]
            self.assertTrue(prev.startswith("wifiMarkSkipPortal();"), "%s: previous line is %r" % (reason, prev))
        calls = re.findall(r"^\s*wifiMarkSkipPortal\(\);", strip_comments(self.net), re.M)
        self.assertEqual(len(calls), 2, "the mark is written somewhere else too")
        self.assertEqual(len(re.findall(r"\bdoRestart\(", strip_comments(self.net))), 2,
                         "a new doRestart() in networkStuff.ino: decide whether it is a supervisor restart")

    def test_restart_path_adds_no_flash_write(self):
        code = strip_comments(self.net)
        for reason in ("WiFi: connected but no traffic", "WiFi: too many reconnect failures"):
            i = code.find('doRestart("%s");' % reason)
            window = code[max(0, i - 300):i]
            for bad in ("platformRtcWrite", "Preferences", "nvs_", "writeSettings"):
                self.assertNotIn(bad, window, "%s: %s before the restart" % (reason, bad))

    def test_setup_takes_the_mark_with_the_other_records(self):
        ino = (FW_DIR / "OTGW-firmware.ino").read_text(encoding="utf-8").replace("\r", "")
        s = ino.find("void setup() {")
        lines = [l.strip() for l in ino[s:].split("\n")[1:4]]
        self.assertTrue(lines[0].startswith("captureRebootReason();"), lines)
        self.assertTrue(lines[1].startswith("captureWifiDeadRestarts();"), lines)
        self.assertTrue(lines[2].startswith("captureWifiSkipPortal();"), lines)

    def test_startwifi_skip_precedes_portal_and_ap_fallback(self):
        body = strip_comments(cut_startwifi(self.net))
        d = body.find("wifiSkipPortalDecide(")
        self.assertGreater(d, 0, "startWiFi() does not decide the skip")
        self.assertLess(d, body.find("startAPFallback();"))
        self.assertLess(d, body.find("startConfigPortal("))
        self.assertIn("if (WiFi.status() != WL_CONNECTED && !portalSkipped)", body, "final 240 s wait not skipped")


# ---------------------------------------------------------------------------------- TASK-1645: opt-in wifinoportal
# With settings.wifi.bNoPortal on, every boot with saved credentials that cannot reach the router skips the open
# config portal and the prerelease AP fallback (no RTC mark needed), and loopWifi keeps retrying. No credentials and
# the triple reset still open the portal. Red on the code before TASK-1645: the setting is ignored, the portal opens.
class NoPortalSetting(unittest.TestCase):
    # Same two host builds and helpers as SkipPortalBoot, without running its tests a second time.
    setUpClass = classmethod(SkipPortalBoot.setUpClass.__func__)
    tearDownClass = classmethod(SkipPortalBoot.tearDownClass.__func__)
    boot = SkipPortalBoot.boot
    assert_portal = SkipPortalBoot.assert_portal

    def test_power_on_router_down_no_portal_no_ap(self):
        for build in ("rel", "pre"):
            r, _ = self.boot(soft=0, noportal=1, build=build)
            self.assertNotIn("portal", r["EV"], build + ": portal opened with wifinoportal on")
            self.assertNotIn("ap_fallback", r["EV"], build + ": AP fallback opened with wifinoportal on")
            self.assertNotIn("RESTART", r)
            self.assertEqual(r["DONE"]["conn"], 0)
            self.assertEqual((r["DONE"]["auto"], r["DONE"]["pers"]), (1, 1), build + ": auto-reconnect not armed")
            self.assertTrue(any("Triple reset to reconfigure" in x for x in r["LOG"]), build + ": no hint in the log")

    def test_router_back_in_window_connects(self):
        r, _ = self.boot(soft=0, noportal=1, ap_up=60)
        self.assertNotIn("portal", r["EV"])
        self.assertEqual(r["DONE"]["conn"], 1)

    def test_every_startwifi_skips_not_only_the_first(self):
        r, _ = self.boot(mode="again", soft=0, noportal=1)
        self.assertEqual(r["EV"].count("portal"), 0, "the second startWiFi() opened the portal")
        self.assertIn("DONE2", r)

    def test_no_credentials_still_opens_portal(self):
        for build in ("rel", "pre"):
            r, _ = self.boot(soft=0, saved=0, noportal=1, build=build)
            self.assert_portal(r)

    def test_triple_reset_still_opens_portal(self):
        for build in ("rel", "pre"):
            r, out = self.boot(soft=0, force=1, noportal=1, build=build)
            self.assert_portal(r)
            self.assertIn("Triple-reset detected", out)

    def test_setting_off_unchanged(self):
        r, _ = self.boot(soft=0, noportal=0)
        self.assert_portal(r)
        r, _ = self.boot(soft=0, noportal=0, build="pre")
        self.assertIn("ap_fallback", r["EV"])


class NoPortalSettingWired(unittest.TestCase):
    def src(self, name):
        return (FW_DIR / name).read_text(encoding="utf-8")

    def test_default_off(self):
        self.assertRegex(self.src("Networktypes.h"), r"\bbool\s+bNoPortal\s*=\s*false\s*;")

    def test_settings_file_rest_help_ui(self):
        st = self.src("settingStuff.ino")
        self.assertIn('writeJsonBoolKV(file, F("WifiNoPortal"),', st)
        self.assertRegex(st, r'PSTR\("WifiNoPortal"\)\) == 0\) \{\s*settings\.wifi\.bNoPortal = EVALBOOLEAN\(newValue\);')
        r = self.src("restAPI.ino")
        self.assertIn('addBool(F("wifinoportal"), settings.wifi.bNoPortal, "b");', r)
        known = r[r.find("knownSettings[] = {"):]
        self.assertIn('"wifinoportal"', known[:known.find("};")])
        self.assertRegex(self.src("settingsHelp.h"), r'(?m)^"wifinoportal":"')
        self.assertRegex(self.src("data/v2.js"), r"wifinoportal:\s*\{ cat: 'network', sub: 'Wi-Fi'")


if __name__ == "__main__":
    unittest.main(verbosity=2)
