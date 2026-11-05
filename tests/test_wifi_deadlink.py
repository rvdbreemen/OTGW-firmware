#!/usr/bin/env python3
"""TASK-1450 (port of 1.x TASK-1248, GH #682): recover from a WiFi link that stays associated but passes no traffic.

Part 1 drives the real WiFi state machine. The block from "enum WifiState_t {" through the end of loopWifi() is cut
out of src/OTGW-firmware/networkStuff.ino, so the dead-link block (between the "TASK-1450 begin/end" markers) and its
wiring in loopWifi() come along. It is compiled on the host twice, as a prerelease build (_VERSION_PRERELEASE, AP
fallback) and as a release build (WIFI_FAILED restart). The fakes are a millis() clock, the real safeTimers.h, a
WiFi station that reconnects 3 s after WiFi.begin() while DHCP works, a gateway that answers ICMP or not, the probe
shims, the three LAN witnesses and doRestart(). doRestart() ends the process and prints the RTC record. The next
"boot" is a new process that gets that record, which is how a software restart is simulated (RTC_NOINIT is a plain
static here, and a power-on is simulated with garbage).

Part 2 cuts the probe shims (between "TASK-1450 probe begin/end") out of platform_esp32.h and compiles them against a
fake lwIP that records whether the core lock is held at every raw_*/pbuf_* call. It feeds echo replies to the receive
callback and checks the reply mask, that the callback never eats a packet and never moves its payload.

Part 3 is source gates: no raw pcb, pbuf or core-lock symbol in application code, no logging, allocation or Arduino
call in the receive callback, and the call sites (setup(), both port 25238 readers, processAPI(), isFlashing()).

Old-vs-fix: OTGW_NETWORK_INO points at another networkStuff.ino (an origin/dev copy without the check compiles and
runs: the behaviour tests go red, the dead link is never recovered). OTGW_PLATFORM_H and OTGW_FW_DIR do the same for
the shim and the source gates.

Not covered here: the real lwIP and WiFi driver (does the echo reach the router, does the reply arrive, does
disconnect() reconnect), and RTC_NOINIT surviving esp_restart() on the chip. Those are bench checks (TASK-1450 AC1-8).

Run: python tests/test_wifi_deadlink.py
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

REPO = Path(__file__).resolve().parent.parent
FW_DIR = Path(os.environ.get("OTGW_FW_DIR", REPO / "src" / "OTGW-firmware"))
NET_INO = Path(os.environ.get("OTGW_NETWORK_INO", FW_DIR / "networkStuff.ino"))
PLATFORM_H = Path(os.environ.get("OTGW_PLATFORM_H", REPO / "src" / "libraries" / "Platform" / "src" / "platform_esp32.h"))
TIMERS = (REPO / "src" / "OTGW-firmware" / "safeTimers.h").as_posix()

MIN = 60  # seconds

# ---------------------------------------------------------------------------------------------- part 1: loopWifi()
STATE_STUBS = r"""
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cstdarg>
#include <string>
typedef uint8_t byte;
static uint32_t g_ms = 0;
static uint32_t millis() { return g_ms; }
static long random(long a, long) { return a; }
#include "@TIMERS@"
#define F(s) (s)
#define PSTR(s) (s)
#define PGM_P const char *
#define CSTR(x) (x)
#define RTC_NOINIT_ATTR
#define HAS_ETH_CAPABLE 1
static bool g_quiet = false;
#define DebugTf(...)  (g_quiet ? 0 : (printf("LOG|%u|", (unsigned)(g_ms / 1000)), printf(__VA_ARGS__)))
#define DebugTln(s)   (g_quiet ? 0 : printf("LOG|%u|%s\n", (unsigned)(g_ms / 1000), (const char *)(s)))
static void ev(const char *what) { printf("EV|%u|%s\n", (unsigned)(g_ms / 1000), what); }

enum NetMode { NET_WIFI, NET_ETHERNET };
struct NetState { NetMode eMode = NET_WIFI; bool bAPFallback = false; };
struct MqttState { bool bConnected = false; };
struct State { NetState net; MqttState mqtt; };
struct MqttSettings { bool bEnable = true; };
struct WifiSettings { bool bNoPortal = false; };   // TASK-1645: read by wifiSkipPortalDecide()
struct Settings { MqttSettings mqtt; WifiSettings wifi; const char *sHostname = "otgw"; };
static State state; static Settings settings;

enum HeapHealthLevel { HEAP_HEALTHY, HEAP_LOW, HEAP_WARNING, HEAP_CRITICAL };
static HeapHealthLevel g_heap = HEAP_HEALTHY;
static HeapHealthLevel getHeapHealth() { return g_heap; }

// ---- fake network -----------------------------------------------------------------------------
enum { WL_IDLE_STATUS = 0, WL_CONNECTED = 3, WL_DISCONNECTED = 6 };
static int      g_wifiStatus = WL_CONNECTED;
static uint32_t g_connectAt = 0;          // WiFi.begin() completes at this time (0: never)
static bool     g_dhcpWorks = true;       // a (re)connect gets an IP
static uint32_t g_gateway = 0x0158A8C0;   // 192.168.88.1 in lwIP byte order
static bool     g_gwIcmp = true;          // the gateway answers ping when traffic passes
static bool     g_traffic = true;         // the link passes traffic
static int      g_forced = 0, g_begins = 0, g_apFallback = 0;
static uint32_t g_forcedAt[16];
struct FakeIp { std::string toString() const { return "192.168.88.63"; } };
struct FakeWiFi {
  int status() {
    if (g_wifiStatus != WL_CONNECTED && g_connectAt != 0 && g_ms >= g_connectAt) { g_wifiStatus = WL_CONNECTED; g_connectAt = 0; }
    return g_wifiStatus;
  }
  void begin() { g_begins++; if (g_wifiStatus != WL_CONNECTED) g_connectAt = g_dhcpWorks ? g_ms + 3000 : 0; }
  bool disconnect(bool = false, bool = false, unsigned long = 100) { g_wifiStatus = WL_DISCONNECTED; g_connectAt = 0; return true; }
  FakeIp localIP() { return FakeIp(); }
};
static FakeWiFi WiFi;
static int g_gwReads = 0;
static uint32_t platformWifiGatewayV4() { g_gwReads++; return g_gateway; }
static void platformWifiForceReconnect() {
  if (g_forced < 16) g_forcedAt[g_forced] = g_ms;
  g_forced++; ev("forced_reconnect");
  g_wifiStatus = WL_DISCONNECTED; g_connectAt = 0;
}
static void platformSetHostname(const char *) {}
static void platformWifiPreferStrongestAp() {}
static void platformWifiDisableSleep() {}
static void startTelnet() {}
static void startMQTT() {}
static void startWebSocket() {}
static void startAPFallback() { g_apFallback++; state.net.bAPFallback = true; ev("ap_fallback"); }
static void stopAPFallback() { state.net.bAPFallback = false; }
// TASK-1446 link record hooks called from the state machine (tested in test_wifi_link_record.py)
enum WifiSelfCause : uint8_t { WIFI_SELF_NONE, WIFI_SELF_ETHERNET, WIFI_SELF_AP_RETRY, WIFI_SELF_DEAD_LINK };
static void wifiLinkMarkSelf(WifiSelfCause) {}
static void wifiLinkEventsLog() {}

// ---- fake probe shims -------------------------------------------------------------------------
static bool     g_pcbFail = false, g_pbufFail = false;
static bool     g_probeOpen = false;
static uint16_t g_probeBase = 0;
static uint8_t  g_probeMask = 0;
static int      g_begun = 0, g_echoes = 0, g_violations = 0, g_echoesWired = 0;
static bool platformGatewayProbeBegin(uint32_t target, uint16_t seqBase) {
  if (g_probeOpen) { g_violations++; return false; }
  if (target != g_gateway) g_violations++;
  if (g_pcbFail) return false;
  g_probeOpen = true; g_probeBase = seqBase; g_probeMask = 0; g_begun++;
  return true;
}
static bool platformGatewayProbeSend(uint16_t seq) {
  if (!g_probeOpen) { g_violations++; return false; }
  if (state.net.eMode == NET_ETHERNET) g_echoesWired++;
  if (g_pbufFail) return false;
  g_echoes++;
  if (g_traffic && g_gwIcmp) g_probeMask |= (uint8_t)(1u << (uint16_t)(seq - g_probeBase));
  return true;
}
static uint8_t platformGatewayProbeEnd() { uint8_t m = g_probeOpen ? g_probeMask : 0; g_probeOpen = false; return m; }

// ---- witnesses, restart, reset reason -------------------------------------------------------------
static bool g_restWitness = false, g_streamWitness = false;
static bool restRequestWithin(uint32_t) { return g_restWitness; }
static bool otgwStreamRxWithin(uint32_t) { return g_streamWitness; }
static bool g_softReset = false;
static bool platformResetWasSoftware() { return g_softReset; }
static void dumpRtc(const char *tag);
static void doRestart(const char *reason) {
  printf("RESTART|%u|%s\n", (unsigned)(g_ms / 1000), reason);
  dumpRtc("RTC");
  printf("SUM|forced=%d|restarts=1|begun=%d|ap=%d|viol=%d|wired=%d\n", g_forced, g_begun, g_apFallback, g_violations, g_echoesWired);
  for (int i = 0; i < g_forced && i < 16; i++) printf("FORCEDAT|%u\n", (unsigned)(g_forcedAt[i] / 1000));
  fflush(stdout);
  exit(0);
}
"""

STATE_MAIN = r"""
#ifndef TASK1450
static void dumpRtc(const char *tag) { printf("%s|0|0\n", tag); }
#else
static void dumpRtc(const char *tag) {
  printf("%s|%lu|%lu\n", tag, (unsigned long)g_rtcWifiDeadRestarts.magic, (unsigned long)g_rtcWifiDeadRestarts.count);
}
#endif

// One loop pass every 100 ms, like doBackgroundTasks() calls loopWifi() (skipped while flashing).
static bool g_flashing = false;
static void runFor(uint32_t seconds) {
  const uint32_t end = g_ms + seconds * 1000UL;
  while (g_ms < end) {
    if (!g_flashing) loopWifi();
    g_ms += 100;
  }
}

int main(int argc, char **argv) {
  if (argc < 5) return 2;
  const char *sc = argv[1];
  const unsigned long rtcMagic = strtoul(argv[2], nullptr, 10), rtcCount = strtoul(argv[3], nullptr, 10);
  g_softReset = atoi(argv[4]) != 0;
  g_quiet = (argc > 5 && atoi(argv[5]) != 0);
  g_ms = 20000;   // setup() took 20 s
#ifdef TASK1450
  g_rtcWifiDeadRestarts.magic = (uint32_t)rtcMagic; g_rtcWifiDeadRestarts.count = (uint32_t)rtcCount;
  captureWifiDeadRestarts();
#else
  (void)rtcMagic; (void)rtcCount;
#endif
  state.mqtt.bConnected = false;   // the broker is behind the same dead path unless a scenario says otherwise

  if (!strcmp(sc, "dead")) {               // AC1/AC2: alive 5 min, then dead (association kept, DHCP passes)
    runFor(5 * 60); ev("link_dead"); g_traffic = false; runFor(40 * 60);
  } else if (!strcmp(sc, "dead_long")) {   // AC5 cap: dead for 75 min
    runFor(5 * 60); ev("link_dead"); g_traffic = false; runFor(75 * 60);
  } else if (!strcmp(sc, "dead_no_dhcp")) {// release vs prerelease: the forced reconnect cannot get an IP back
    runFor(5 * 60); ev("link_dead"); g_traffic = false; g_dhcpWorks = false; runFor(30 * 60);
  } else if (!strcmp(sc, "dead_from_boot")) {
    g_traffic = false; runFor(30 * 60);
  } else if (!strcmp(sc, "never_icmp")) {  // AC4: a router that never answers ping, link otherwise dead too
    g_gwIcmp = false; runFor(5 * 60); g_traffic = false; ev("link_dead"); runFor(30 * 60);
  } else if (!strcmp(sc, "witness_mqtt")) {// AC3: ICMP to the gateway blocked, LAN traffic flows
    runFor(5 * 60); ev("icmp_blocked"); g_gwIcmp = false; state.mqtt.bConnected = true; runFor(40 * 60);
  } else if (!strcmp(sc, "witness_mqtt_disabled")) {  // a connected flag without MQTT enabled is no witness
    runFor(5 * 60); g_gwIcmp = false; state.mqtt.bConnected = true; settings.mqtt.bEnable = false; runFor(20 * 60);
  } else if (!strcmp(sc, "witness_rest")) {
    runFor(5 * 60); ev("icmp_blocked"); g_gwIcmp = false; g_restWitness = true; runFor(40 * 60);
  } else if (!strcmp(sc, "witness_stream")) {
    runFor(5 * 60); ev("icmp_blocked"); g_gwIcmp = false; g_streamWitness = true; runFor(40 * 60);
  } else if (!strcmp(sc, "gw_change")) {   // AC4: armed on .1, DHCP hands out a new gateway that never answers
    runFor(5 * 60); ev("gw_changed"); g_gateway = 0x0258A8C0; g_gwIcmp = false; g_traffic = false; runFor(30 * 60);
  } else if (!strcmp(sc, "heap")) {        // AC6: heap not HEALTHY -> no probe, nothing counted
    runFor(5 * 60); g_heap = HEAP_LOW; g_traffic = false; ev("link_dead_heap_low"); runFor(30 * 60);
    printf("HEAPBEGUN|%d\n", g_begun);
  } else if (!strcmp(sc, "nopbuf")) {      // allocation failures are skipped, not counted
    runFor(5 * 60); g_pbufFail = true; g_traffic = false; runFor(30 * 60);
  } else if (!strcmp(sc, "nopcb")) {
    runFor(5 * 60); g_pcbFail = true; g_traffic = false; runFor(30 * 60);
  } else if (!strcmp(sc, "ethernet")) {    // AC6: wired for 10 min (mid-probe switch), back on WiFi with a dead link
    runFor(5 * 60 + 61);                   // the probe of minute 6 is open now
    printf("OPEN_AT_SWITCH|%d\n", g_probeOpen ? 1 : 0);
    state.net.eMode = NET_ETHERNET; runFor(10 * 60);
    printf("OPEN_WIRED|%d\n", g_probeOpen ? 1 : 0);
    state.net.eMode = NET_WIFI; g_traffic = false; ev("back_on_wifi_dead"); runFor(30 * 60);
  } else if (!strcmp(sc, "flash_stall")) { // AC6: the loop stops (flashing) while a probe is open; it is discarded
    // Without the stall guard the open probe would send its last two echoes after the stall and be judged dead
    // within 5 s; with it the probe is dropped and the next one is not judged yet.
    runFor(5 * 60 + 59); g_traffic = false; runFor(2);
    printf("OPEN_AT_FLASH|%d\n", g_probeOpen ? 1 : 0);
    g_flashing = true; runFor(3 * 60); g_flashing = false;
    runFor(5);
#ifdef TASK1450
    printf("DEAD_AFTER_STALL|%u\n", (unsigned)wifiDeadProbes);
#endif
    g_traffic = true; runFor(10 * 60);
  } else if (!strcmp(sc, "alive")) {       // clear rule: alive for 65 min
    runFor(65 * 60);
  } else {
    return 3;
  }
  dumpRtc("RTC");
  printf("SUM|forced=%d|restarts=0|begun=%d|ap=%d|viol=%d|wired=%d\n", g_forced, g_begun, g_apFallback, g_violations, g_echoesWired);
  for (int i = 0; i < g_forced && i < 16; i++) printf("FORCEDAT|%u\n", (unsigned)(g_forcedAt[i] / 1000));
  return 0;
}
"""


def cut_state_machine(src):
    a = src.find("enum WifiState_t {")
    b = src.find("void loopWifi() {")
    if a < 0 or b < 0:
        raise AssertionError("WiFi state machine not found in networkStuff.ino")
    m = re.compile(r"\r?\n\}\r?\n").search(src, b)
    return src[a:m.end()]


def parse(out):
    rows = {"EV": [], "LOG": [], "RESTART": None, "FORCEDAT": []}
    for line in out.splitlines():
        parts = line.split("|")
        k = parts[0]
        if k == "EV":
            rows["EV"].append((int(parts[1]), parts[2]))
        elif k == "LOG":
            rows["LOG"].append((int(parts[1]), "|".join(parts[2:]).strip()))
        elif k == "RESTART":
            rows["RESTART"] = (int(parts[1]), parts[2])
        elif k == "RTC":
            rows["RTC"] = (int(parts[1]), int(parts[2]))
        elif k == "SUM":
            rows["SUM"] = {p.split("=")[0]: int(p.split("=")[1]) for p in parts[1:]}
        elif k == "FORCEDAT":
            rows["FORCEDAT"].append(int(parts[1]))
        else:
            rows[k] = parts[1] if len(parts) > 1 else ""
    return rows


MAGIC = 0x4F544452


class WifiDeadLinkStateMachine(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        src = NET_INO.read_text(encoding="utf-8")
        cut = cut_state_machine(src)
        cls.has_fix = "// TASK-1450 begin" in cut
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = {}
        for name, defs in (("pre", "#define _VERSION_PRERELEASE \"beta.1\"\n"), ("rel", "")):
            cpp = Path(cls.tmp.name) / ("t_%s.cpp" % name)
            head = defs + ("#define TASK1450 1\n" if cls.has_fix else "")
            cpp.write_text(head + STATE_STUBS.replace("@TIMERS@", TIMERS) + cut + STATE_MAIN, encoding="utf-8")
            exe = compile_cpp(cpp, Path(cls.tmp.name) / ("t_%s.exe" % name))
            if exe is None:
                raise unittest.SkipTest("no host C++ compiler")
            cls.exe[name] = exe

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_sc(self, sc, build="pre", magic=0, count=0, soft=0, quiet=False):
        out = subprocess.run([str(self.exe[build]), sc, str(magic), str(count), str(soft), "1" if quiet else "0"],
                             capture_output=True, text=True, check=True).stdout
        return parse(out), out

    def logs(self, rows, needle):
        return [t for t, s in rows["LOG"] if needle in s]

    # AC1: 8 dead probes -> forced reconnect, then (still dead) doRestart with the reason, logged once per change.
    def test_dead_link_reconnects_then_restarts(self):
        r, out = self.run_sc("dead")
        print(out[-1500:])
        self.assertEqual(r["SUM"]["viol"], 0)
        self.assertEqual(r["SUM"]["forced"], 1, "no forced reconnect on a dead link")
        dead_at = 5 * MIN + 20
        t_forced = r["FORCEDAT"][0]
        self.assertGreaterEqual(t_forced - dead_at, 8 * MIN - 60, "acted before 8 dead probes")
        self.assertLessEqual(t_forced - dead_at, 9 * MIN + 30)
        self.assertIsNotNone(r["RESTART"], "no restart after a forced reconnect that did not help")
        t_restart, reason = r["RESTART"]
        self.assertEqual(reason, "WiFi: connected but no traffic")
        self.assertGreaterEqual(t_restart - t_forced, 8 * MIN)
        self.assertLessEqual(t_restart - t_forced, 9 * MIN + 30)
        self.assertEqual(r["RTC"], (MAGIC, 1))
        self.assertEqual(len(self.logs(r, "dead-link check armed")), 1)
        self.assertEqual(len(self.logs(r, "forcing a reconnect")), 1)
        self.assertEqual(len(self.logs(r, "stopped answering")), 2, "one line per dead streak")

    def test_dead_link_release_build_too(self):
        r, _ = self.run_sc("dead", build="rel", quiet=True)
        self.assertEqual(r["SUM"]["forced"], 1)
        self.assertEqual(r["RESTART"][1], "WiFi: connected but no traffic")

    # AC4
    def test_router_that_never_answers_never_arms(self):
        r, _ = self.run_sc("never_icmp")
        self.assertEqual(r["SUM"]["forced"], 0)
        self.assertIsNone(r["RESTART"])
        if self.has_fix:
            self.assertGreater(r["SUM"]["begun"], 25, "the probe did not run at all")
        self.assertEqual(self.logs(r, "armed"), [])

    def test_dead_from_boot_never_arms(self):
        r, _ = self.run_sc("dead_from_boot", quiet=True)
        self.assertEqual(r["SUM"]["forced"], 0)
        self.assertIsNone(r["RESTART"])

    def test_gateway_change_disarms(self):
        r, _ = self.run_sc("gw_change")
        self.assertEqual(r["SUM"]["forced"], 0)
        self.assertIsNone(r["RESTART"])
        self.assertEqual(len(self.logs(r, "dead-link check armed")), 1)
        self.assertEqual(len(self.logs(r, "gateway address changed to 192.168.88.2, dead-link check disarmed")), 1)

    # AC3
    def test_witnesses_suppress_action(self):
        for sc in ("witness_mqtt", "witness_rest", "witness_stream"):
            with self.subTest(sc=sc):
                r, _ = self.run_sc(sc)
                self.assertEqual(r["SUM"]["forced"], 0, sc)
                self.assertIsNone(r["RESTART"], sc)
                self.assertEqual(len(self.logs(r, "traffic flows, no action")), 1, sc + ": logged once, not per streak")
                self.assertEqual(len(self.logs(r, "stopped answering")), 1, sc)

    def test_mqtt_connected_flag_without_mqtt_enabled_is_no_witness(self):
        r, _ = self.run_sc("witness_mqtt_disabled", quiet=True)
        self.assertEqual(r["SUM"]["forced"], 1)

    # AC5
    def test_restart_count_survives_a_software_restart(self):
        r, _ = self.run_sc("dead", magic=MAGIC, count=1, soft=1, quiet=True)
        self.assertEqual(r["RTC"], (MAGIC, 2))

    def test_restart_count_clears_on_power_on_panic_or_garbage(self):
        for magic, count, soft in ((MAGIC, 2, 0), (0xA5A5A5A5, 2, 1), (MAGIC, 99, 1)):
            with self.subTest(magic=magic, count=count, soft=soft):
                r, _ = self.run_sc("dead", magic=magic, count=count, soft=soft, quiet=True)
                self.assertEqual(r["RTC"], (MAGIC, 1), "count not restarted from zero")

    def test_cap_stops_restarts_and_spaces_reconnects(self):
        r, out = self.run_sc("dead_long", magic=MAGIC, count=3, soft=1)
        self.assertIsNone(r["RESTART"], "restarted at the cap")
        self.assertGreaterEqual(r["SUM"]["forced"], 3)
        gaps = [b - a for a, b in zip(r["FORCEDAT"], r["FORCEDAT"][1:])]
        self.assertTrue(all(g >= 15 * MIN for g in gaps), gaps)
        self.assertEqual(r["RTC"], (MAGIC, 3))
        self.assertGreaterEqual(len(self.logs(r, "restart cap (3) reached, reconnecting only")), 3)

    def test_cap_reached_through_successive_boots(self):
        magic, count, reasons = 0, 0, []
        for boot in range(4):
            r, _ = self.run_sc("dead", magic=magic, count=count, soft=1 if boot else 0, quiet=True)
            magic, count = r["RTC"]
            reasons.append(r["RESTART"][1] if r["RESTART"] else None)
        self.assertEqual(reasons, ["WiFi: connected but no traffic"] * 3 + [None])
        self.assertEqual(count, 3)

    def test_count_clears_after_an_hour_alive(self):
        r, out = self.run_sc("alive", magic=MAGIC, count=2, soft=1)
        self.assertEqual(r["RTC"], (MAGIC, 0))
        self.assertEqual(len(self.logs(r, "restart count cleared")), 1)
        self.assertEqual(len(self.logs(r, "2 dead-link restart(s)")), 1)

    def test_failed_forced_reconnect_counts_toward_cap_release_build(self):
        r, _ = self.run_sc("dead_no_dhcp", build="rel", quiet=True)
        self.assertEqual(r["SUM"]["forced"], 1)
        self.assertEqual(r["RESTART"][1], "WiFi: too many reconnect failures")
        self.assertEqual(r["RTC"], (MAGIC, 1))

    def test_failed_forced_reconnect_at_cap_retries_release_build(self):
        r, _ = self.run_sc("dead_no_dhcp", build="rel", magic=MAGIC, count=3, soft=1)
        self.assertIsNone(r["RESTART"])
        self.assertGreaterEqual(len(self.logs(r, "dead-link restart cap reached, retrying")), 1)

    def test_failed_forced_reconnect_prerelease_goes_to_ap_fallback(self):
        r, _ = self.run_sc("dead_no_dhcp", build="pre", quiet=True)
        self.assertEqual(r["SUM"]["forced"], 1)
        self.assertEqual(r["SUM"]["ap"], 1)
        self.assertIsNone(r["RESTART"])

    # AC6
    def test_no_probe_while_heap_not_healthy(self):
        r, _ = self.run_sc("heap")
        self.assertEqual(r["SUM"]["forced"], 0)
        self.assertIsNone(r["RESTART"])
        if self.has_fix:
            # probes at 80, 140, 200 and 260 s ran before the heap left HEALTHY at 320 s
            self.assertEqual(int(r["HEAPBEGUN"]), 4, "probes ran while the heap was not HEALTHY")
            self.assertEqual(len(self.logs(r, "probe skipped (heap not healthy)")), 1)

    def test_allocation_failures_are_not_counted(self):
        for sc, why in (("nopbuf", "no echo buffer"), ("nopcb", "no pcb")):
            with self.subTest(sc=sc):
                r, _ = self.run_sc(sc)
                self.assertEqual(r["SUM"]["forced"], 0)
                self.assertIsNone(r["RESTART"])
                if self.has_fix:
                    self.assertEqual(len(self.logs(r, "probe skipped (%s)" % why)), 1)

    def test_ethernet_closes_the_probe_and_disarms(self):
        r, _ = self.run_sc("ethernet")
        if self.has_fix:
            self.assertEqual(r["OPEN_AT_SWITCH"], "1", "scenario does not switch mid-probe")
        self.assertEqual(r["OPEN_WIRED"], "0", "probe pcb left open on the wired link")
        self.assertEqual(r["SUM"]["wired"], 0, "echo sent while wired")
        self.assertEqual(r["SUM"]["viol"], 0)
        if self.has_fix:
            self.assertEqual(len(self.logs(r, "wired link active, dead-link check disarmed")), 1)
        # back on WiFi the gateway never answers again: the check stays disarmed
        self.assertEqual(r["SUM"]["forced"], 0)
        self.assertIsNone(r["RESTART"])

    def test_probe_open_across_a_loop_stall_is_discarded(self):
        r, _ = self.run_sc("flash_stall")
        if not self.has_fix:
            self.fail("no dead-link check")
        self.assertEqual(r["OPEN_AT_FLASH"], "1")
        self.assertEqual(r["DEAD_AFTER_STALL"], "0", "a stalled probe was counted as dead")
        self.assertEqual(r["SUM"]["viol"], 0)


# ---------------------------------------------------------------------------------------------- part 2: the shim
SHIM_STUBS = r"""
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
typedef uint8_t u8_t; typedef uint16_t u16_t; typedef uint32_t u32_t; typedef int8_t err_t;
static int g_lock = 0, g_lockViolations = 0, g_lockCalls = 0;
#define LOCK_TCPIP_CORE()   do { if (g_lock) g_lockViolations++; g_lock = 1; g_lockCalls++; } while (0)
#define UNLOCK_TCPIP_CORE() do { if (!g_lock) g_lockViolations++; g_lock = 0; } while (0)
#define NEED_LOCK() do { if (!g_lock) g_lockViolations++; } while (0)
struct pbuf { void *payload; u16_t tot_len; u16_t len; };
struct ip_addr_t { u32_t addr; u8_t type; };
#define IP_PROTO_ICMP 1
#define ICMP_ER 0
#define ICMP_ECHO 8
#define PBUF_IP 0
#define PBUF_RAM 0
#pragma pack(push, 1)
struct icmp_echo_hdr { u8_t type; u8_t code; u16_t chksum; u16_t id; u16_t seqno; };
#pragma pack(pop)
#define ICMPH_TYPE(h) ((h)->type)
#define ICMPH_TYPE_SET(h, t) ((h)->type = (t))
#define ICMPH_CODE_SET(h, c) ((h)->code = (c))
static u16_t swap16(u16_t x) { return (u16_t)((x << 8) | (x >> 8)); }
#define PP_HTONS(x) ((u16_t)((((x) & 0xff) << 8) | (((x) & 0xff00) >> 8)))
#define lwip_htons(x) swap16(x)
#define lwip_ntohs(x) swap16(x)
#define ip_addr_get_ip4_u32(a) ((a)->addr)
#define ip_addr_set_ip4_u32_val(a, v) do { (a).addr = (v); (a).type = 0; } while (0)
struct raw_pcb; typedef u8_t (*raw_recv_fn)(void *, struct raw_pcb *, struct pbuf *, const ip_addr_t *);
struct raw_pcb { raw_recv_fn recv; void *arg; };
static raw_pcb g_pcb; static int g_pcbLive = 0, g_failNew = 0, g_failPbuf = 0, g_pbufLive = 0, g_sent = 0;
static u8_t g_lastSent[64]; static u32_t g_lastDst = 0;
static raw_pcb *raw_new(u8_t proto) { NEED_LOCK(); if (proto != IP_PROTO_ICMP || g_failNew) return nullptr; g_pcbLive++; g_pcb.recv = nullptr; return &g_pcb; }
static void raw_recv(raw_pcb *p, raw_recv_fn f, void *a) { NEED_LOCK(); p->recv = f; p->arg = a; }
static void raw_remove(raw_pcb *p) { NEED_LOCK(); p->recv = nullptr; g_pcbLive--; }
static err_t raw_sendto(raw_pcb *, pbuf *p, const ip_addr_t *d) { NEED_LOCK(); g_sent++; memcpy(g_lastSent, p->payload, p->len); g_lastDst = d->addr; return 0; }
static pbuf *pbuf_alloc(int, u16_t len, int) { NEED_LOCK(); if (g_failPbuf) return nullptr; pbuf *p = (pbuf *)malloc(sizeof(pbuf)); p->payload = malloc(len); p->len = p->tot_len = len; g_pbufLive++; return p; }
static u8_t pbuf_free(pbuf *p) { NEED_LOCK(); free(p->payload); free(p); g_pbufLive--; return 1; }
static u8_t pbuf_get_at(const pbuf *p, u16_t off) { return off < p->tot_len ? ((const u8_t *)p->payload)[off] : 0; }
static u16_t pbuf_copy_partial(const pbuf *p, void *d, u16_t n, u16_t off) {
  if (off >= p->tot_len) return 0; u16_t c = (u16_t)(p->tot_len - off); if (c > n) c = n;
  memcpy(d, (const u8_t *)p->payload + off, c); return c; }
static u16_t inet_chksum(const void *, u16_t) { return 0x1234; }
"""

SHIM_MAIN = r"""
// An inbound IPv4 packet as raw_input() hands it to the callback: payload at the IP header.
static u8_t deliver(u8_t type, u16_t id, u16_t seq, u32_t src, int ihl_words = 5, u16_t truncate = 0) {
  // Heap pbuf like lwIP's own, so an eat-and-free mutant fails on EATEN instead of crashing.
  if (g_pcb.recv == nullptr) return 9;
  g_lock = 1;   // the tcpip task holds the core lock while it delivers input
  pbuf *p = pbuf_alloc(PBUF_IP, 64, PBUF_RAM);
  u8_t *buf = (u8_t *)p->payload; memset(buf, 0, 64);
  buf[0] = (u8_t)(0x40 | ihl_words);
  icmp_echo_hdr h = { type, 0, 0, PP_HTONS(id), swap16(seq) };
  memcpy(buf + ihl_words * 4, &h, sizeof(h));
  p->len = p->tot_len = (u16_t)(truncate ? truncate : ihl_words * 4 + 8 + 16);
  ip_addr_t a = { src, 0 };
  void *before = p->payload;
  u8_t eaten = g_pcb.recv(g_pcb.arg, &g_pcb, p, &a);
  if (!eaten) {
    if (p->payload != before) printf("PAYLOAD_MOVED|1\n");
    pbuf_free(p);
  }
  g_lock = 0;
  return eaten;
}
int main() {
  const u32_t gw = 0x0158A8C0;
  printf("BEGIN|%d\n", platformGatewayProbeBegin(gw, 300) ? 1 : 0);
  printf("BEGIN_TWICE|%d\n", platformGatewayProbeBegin(gw, 303) ? 1 : 0);
  printf("SEND|%d\n", platformGatewayProbeSend(300) ? 1 : 0);
  const icmp_echo_hdr *s = (const icmp_echo_hdr *)g_lastSent;
  printf("SENT_HDR|%u|%u|%u|%u|%lu\n", s->type, s->code, (unsigned)swap16(s->id), (unsigned)swap16(s->seqno), (unsigned long)g_lastDst);
  int eaten = 0;
  eaten += deliver(ICMP_ER, 0x4F54, 301, gw);          // ours, seq base+1
  eaten += deliver(ICMP_ER, 0x4F54, 302, 0x0258A8C0);  // ours, wrong source
  eaten += deliver(ICMP_ER, 0x1234, 300, gw);          // somebody else's ping
  eaten += deliver(ICMP_ECHO, 0x4F54, 300, gw);        // an echo request to us
  eaten += deliver(ICMP_ER, 0x4F54, 299, gw);          // previous probe's late reply
  eaten += deliver(ICMP_ER, 0x4F54, 300, gw, 6);       // IP options: header 24 bytes
  eaten += deliver(ICMP_ER, 0x4F54, 302, gw, 5, 22);   // truncated
  printf("EATEN|%d\n", eaten);
  printf("MASK|%u\n", (unsigned)platformGatewayProbeEnd());
  printf("END_AGAIN|%u\n", (unsigned)platformGatewayProbeEnd());
  printf("SEND_CLOSED|%d\n", platformGatewayProbeSend(1) ? 1 : 0);
  g_failNew = 1; printf("BEGIN_NOPCB|%d\n", platformGatewayProbeBegin(gw, 1) ? 1 : 0); g_failNew = 0;
  platformGatewayProbeBegin(gw, 10);
  g_failPbuf = 1; printf("SEND_NOPBUF|%d\n", platformGatewayProbeSend(10) ? 1 : 0); g_failPbuf = 0;
  platformGatewayProbeEnd();
  printf("LEAKS|%d|%d\n", g_pcbLive, g_pbufLive);
  printf("LOCK|%d|%d|%d\n", g_lockViolations, g_lock, g_lockCalls);
  return 0;
}
"""


class GatewayProbeShim(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        src = PLATFORM_H.read_text(encoding="utf-8")
        a = src.find("// TASK-1450 probe begin")
        b = src.find("// TASK-1450 probe end")
        if a < 0 or b < 0:
            raise AssertionError("TASK-1450 probe shims not found in platform_esp32.h")
        cls.block = src[a:b]
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "s.cpp"
        cpp.write_text(SHIM_STUBS + cls.block + SHIM_MAIN, encoding="utf-8")
        exe = compile_cpp(cpp, Path(cls.tmp.name) / "s.exe")
        if exe is None:
            raise unittest.SkipTest("no host C++ compiler")
        out = subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout
        print(out)
        cls.rows = {}
        for line in out.splitlines():
            k, _, v = line.partition("|")
            cls.rows[k] = v

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_every_lwip_call_holds_the_core_lock(self):
        violations, held, calls = self.rows["LOCK"].split("|")
        self.assertEqual(violations, "0", "raw_*/pbuf_* called without LOCK_TCPIP_CORE, or the lock nested")
        self.assertEqual(held, "0", "core lock left held")
        self.assertGreater(int(calls), 0)

    def test_echo_request_format(self):
        self.assertEqual(self.rows["BEGIN"], "1")
        self.assertEqual(self.rows["BEGIN_TWICE"], "0")
        self.assertEqual(self.rows["SEND"], "1")
        self.assertEqual(self.rows["SENT_HDR"], "8|0|20308|300|%d" % 0x0158A8C0)

    def test_only_own_replies_from_the_target_count(self):
        # seq 301 -> bit 1; seq 300 via a 24-byte IP header -> bit 0; the rest is ignored
        self.assertEqual(self.rows["MASK"], "3")
        self.assertEqual(self.rows["END_AGAIN"], "0")

    def test_callback_never_eats_and_never_moves_the_payload(self):
        self.assertEqual(self.rows["EATEN"], "0")
        self.assertNotIn("PAYLOAD_MOVED", self.rows)

    def test_failures_and_cleanup(self):
        self.assertEqual(self.rows["SEND_CLOSED"], "0")
        self.assertEqual(self.rows["BEGIN_NOPCB"], "0")
        self.assertEqual(self.rows["SEND_NOPBUF"], "0")
        self.assertEqual(self.rows["LEAKS"], "0|0")


# ---------------------------------------------------------------------------------------------- part 3: source gates
LWIP_IN_APP = re.compile(r"\b(raw_new|raw_recv|raw_sendto|raw_remove|raw_bind|raw_connect|pbuf_alloc|pbuf_free|"
                         r"pbuf_copy_partial|LOCK_TCPIP_CORE|UNLOCK_TCPIP_CORE|tcpip_callback|IP_PROTO_ICMP|"
                         r"icmp_echo_hdr|esp_ping_new_session|esp_reset_reason)\b")
CALLBACK_FORBIDDEN = re.compile(r"\b(Debug\w*|log_[a-z]|printf|Serial|malloc|calloc|free|new|delete|String|"
                                r"pbuf_alloc|pbuf_free|millis|delay|raw_\w+|LOCK_TCPIP_CORE|UNLOCK_TCPIP_CORE)\b")


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return "\n".join(l.split("//", 1)[0] for l in text.splitlines())


class WifiDeadLinkSourceGates(unittest.TestCase):
    def test_no_lwip_or_esp_calls_in_application_code(self):
        hits = []
        for path in sorted(FW_DIR.glob("*.ino")) + sorted(FW_DIR.glob("*.h")) + sorted(FW_DIR.glob("*.cpp")):
            for i, line in enumerate(strip_comments(path.read_text(encoding="utf-8", errors="replace")).splitlines()):
                if LWIP_IN_APP.search(line):
                    hits.append("%s:%d: %s" % (path.name, i + 1, line.strip()))
        self.assertEqual(hits, [], "lwIP/ESP-IDF call outside platform_esp32.h")

    def test_receive_callback_only_sets_flags(self):
        src = PLATFORM_H.read_text(encoding="utf-8")
        m = re.search(r"inline u8_t platformGatewayProbeRecv\([^)]*\)\s*\{(.*?)\n\}", src, re.S)
        self.assertIsNotNone(m, "platformGatewayProbeRecv missing")
        body = strip_comments(m.group(1))
        self.assertEqual(CALLBACK_FORBIDDEN.findall(body), [], "the tcpip-task callback logs, allocates or calls Arduino")
        self.assertNotRegex(body, r"return\s+1\s*;", "the callback eats packets")

    def test_lwip_calls_in_the_shim_sit_inside_the_lock(self):
        src = PLATFORM_H.read_text(encoding="utf-8")
        a, b = src.find("// TASK-1450 probe begin"), src.find("// TASK-1450 probe end")
        block = strip_comments(src[a:b])
        seen = set()
        for fn in re.finditer(r"inline \w+ (platformGatewayProbe(Begin|Send|End))\([^)]*\)\s*\{(.*?)\n\}", block, re.S):
            seen.add(fn.group(2))
            body = fn.group(3)
            lo, hi = body.find("LOCK_TCPIP_CORE();"), body.find("UNLOCK_TCPIP_CORE();")
            self.assertTrue(0 <= lo < hi, fn.group(1))
            for call in re.finditer(r"\b(raw_\w+|pbuf_\w+)\s*\(", body):
                self.assertTrue(lo < call.start() < hi, "%s: %s outside the core lock" % (fn.group(1), call.group(1)))
        self.assertEqual(seen, {"Begin", "Send", "End"}, "probe shims not found")

    def test_call_sites_are_wired(self):
        ino = (FW_DIR / "OTGW-firmware.ino").read_text(encoding="utf-8")
        s = ino.find("void setup() {")
        lines = [l.strip() for l in ino[s:].replace("\r", "").split("\n")[1:3]]
        self.assertTrue(lines[0].startswith("captureRebootReason();"), lines)
        self.assertTrue(lines[1].startswith("captureWifiDeadRestarts();"), lines)
        self.assertIn("if (!isFlashing()) loopWifi();", ino)
        for name in ("OTGW-Core.ino", "OTDirect.ino"):
            text = (FW_DIR / name).read_text(encoding="utf-8").replace("\r", "")
            reads = [m.start() for m in re.finditer(r"int inByte = OTGWstream\.read\(\);", text)]
            self.assertTrue(reads, name)
            for pos in reads:
                self.assertIn("otgwStreamRxNote();", text[pos:pos + 160], "%s: port 25238 read without witness" % name)
        rest = (FW_DIR / "restAPI.ino").read_text(encoding="utf-8").replace("\r", "")
        p = rest.find("void processAPI(AsyncWebServerRequest *request)\n{")
        self.assertGreater(p, 0)
        first = [l.strip() for l in rest[p:].split("\n")[2:5] if l.strip() and not l.strip().startswith("//")][0]
        self.assertEqual(first, "g_lastRestRequestMs = millis();")


if __name__ == "__main__":
    unittest.main(verbosity=2)
