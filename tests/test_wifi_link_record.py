#!/usr/bin/env python3
"""TASK-1446 + TASK-1444 (GH #682, port of the 1.x outage record): the WiFi station link record.

Part 1 compiles the real record on the host. The block between "TASK-1446 record begin/end" in
src/OTGW-firmware/networkStuff.ino (lock, event callbacks, register-once, self mark, log drain, boot catch-up,
banner printer, snapshot accessor) and the types between "TASK-1446 types begin/end" in Networktypes.h are cut
out and compiled against fakes: a millis() clock, a portMUX that counts nesting and flags logging under the lock,
arduino_event_t structs shaped like arduino-esp32 3.3.5 / ESP-IDF (wifi_event_sta_connected_t and
wifi_event_sta_disconnected_t), and a WiFi object that stores the onEvent registrations so the test fires events
through the real registered callback. Scenarios: one outage with many driver retries, self-caused drops
(Ethernet takeover, dead-link forced reconnect, AP retry timeout), no leak of a self mark onto a later field
outage, the boot catch-up, millis() wrap, and the exact banner text.

Part 2 is source gates: no logging, allocation or String in the event callbacks, every record write inside the
lock, and the call sites (startWiFi registration, loopWifi drain before the Ethernet return, the three self marks
right before their disconnect, the banner lines between Net and Heap, the 14 device/info keys emitted after the
ssid/wifirssi chain from a snapshot frozen before restSendChunked()).

Old-vs-fix: OTGW_FW_DIR points at another src/OTGW-firmware (an origin/dev copy has no record: every test fails
with "record block not found" or a missing call site, not with a compile error).

Not covered here: the real driver's event order and reason codes, the reason names (the fake WiFi returns the
names arduino-esp32 3.3.5 STA.cpp:737-797 returns for the codes used), WiFi.BSSID()/channel() on the chip, the
telnet transport and the JSON on the wire. Those are bench checks (TASK-1446 AC6/AC8/AC9, TASK-1444 AC1-AC5).

Run: python tests/test_wifi_link_record.py
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

STUBS = r"""
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cstdarg>
static uint32_t g_ms = 0;
static uint32_t millis() { return g_ms; }
#define PSTR(s) (s)
#define F(s) (s)
#define PGM_P const char *
#define snprintf_P snprintf
static size_t strlcpy(char *d, const char *s, size_t n) {
  size_t l = strlen(s);
  if (n) { size_t c = l < n - 1 ? l : n - 1; memcpy(d, s, c); d[c] = 0; }
  return l;
}

// ---- lock: counts nesting; logging or a second enter while held is a violation -------------------
struct portMUX_TYPE { int held; };
#define portMUX_INITIALIZER_UNLOCKED {0}
static int g_lockDepth = 0, g_lockViolations = 0, g_lockEnters = 0;
static void muxEnter(portMUX_TYPE *m) { if (m->held) g_lockViolations++; m->held = 1; g_lockDepth++; g_lockEnters++; }
static void muxExit(portMUX_TYPE *m) { if (!m->held) g_lockViolations++; m->held = 0; g_lockDepth--; }
#define portENTER_CRITICAL(m) muxEnter(m)
#define portEXIT_CRITICAL(m)  muxExit(m)

// ---- output: telnet lines and debug log lines -----------------------------------------------------
static int g_logs = 0;
static void DebugTf(const char *fmt, ...) {
  if (g_lockDepth) g_lockViolations++;
  g_logs++;
  char buf[320]; va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
  size_t n = strlen(buf); while (n && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) buf[--n] = 0;
  printf("LOG|%s\n", buf);
}
static void _debugPrintf_P(const char *fmt, ...) {
  if (g_lockDepth) g_lockViolations++;
  char buf[256]; va_list ap; va_start(ap, fmt); int w = vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
  if (w >= (int)sizeof(buf)) printf("TRUNC|%d\n", w);
  size_t n = strlen(buf); while (n && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) buf[--n] = 0;
  printf("TEL|%s\n", buf);
}
struct FakeTelnet { void println(const char *s) { if (g_lockDepth) g_lockViolations++; printf("TEL|%s\n", s); } };
static FakeTelnet debugTelnet;

// ---- arduino-esp32 3.3.5 event shapes (esp_wifi_types_generic.h, NetworkEvents.h) ----------------
enum arduino_event_id_t { ARDUINO_EVENT_WIFI_STA_START = 2, ARDUINO_EVENT_WIFI_STA_CONNECTED = 4,
                          ARDUINO_EVENT_WIFI_STA_DISCONNECTED = 5, ARDUINO_EVENT_WIFI_STA_GOT_IP = 7 };
typedef struct { uint8_t ssid[32]; uint8_t ssid_len; uint8_t bssid[6]; uint8_t channel; int authmode; uint16_t aid; } wifi_event_sta_connected_t;
typedef struct { uint8_t ssid[32]; uint8_t ssid_len; uint8_t bssid[6]; uint8_t reason; int8_t rssi; } wifi_event_sta_disconnected_t;
typedef union { wifi_event_sta_connected_t wifi_sta_connected; wifi_event_sta_disconnected_t wifi_sta_disconnected; } arduino_event_info_t;
typedef struct { arduino_event_id_t event_id; arduino_event_info_t event_info; } arduino_event_t;
typedef int wifi_err_reason_t;
enum { WL_IDLE_STATUS = 0, WL_CONNECTED = 3, WL_DISCONNECTED = 6 };

struct FakeWiFi {
  void (*cb[8])(arduino_event_t *); arduino_event_id_t ids[8]; int n = 0;
  int st = WL_DISCONNECTED; uint8_t bssid[6] = {0}; int ch = 0;
  int onEvent(void (*f)(arduino_event_t *), arduino_event_id_t id) { if (n < 8) { cb[n] = f; ids[n] = id; n++; } return n; }
  int status() { return st; }
  uint8_t *BSSID(uint8_t *b) { if (st != WL_CONNECTED) return nullptr; memcpy(b, bssid, 6); return b; }
  int channel() { return ch; }
  // The names arduino-esp32 3.3.5 STA.cpp:737 returns for the codes used here; "" for an unknown code.
  const char *disconnectReasonName(wifi_err_reason_t r) {
    switch (r) {
      case 2: return "AUTH_EXPIRE"; case 8: return "ASSOC_LEAVE"; case 200: return "BEACON_TIMEOUT";
      case 201: return "NO_AP_FOUND"; case 203: return "ASSOC_FAIL";
    }
    return "";
  }
};
static FakeWiFi WiFi;
"""

MAIN = r"""
static const uint8_t AP_A[6] = {0xAA, 0xBB, 0xCC, 0x00, 0x11, 0x22};
static const uint8_t AP_B[6] = {0x48, 0xA9, 0x8A, 0x01, 0x02, 0x03};

// Fires an event through every callback the code registered for it (the network event task).
static void fire(arduino_event_t *ev) {
  const int logs = g_logs;
  for (int i = 0; i < WiFi.n; i++) if (WiFi.ids[i] == ev->event_id) WiFi.cb[i](ev);
  if (g_logs != logs) printf("CBLOG|%d\n", g_logs - logs);
  if (g_lockDepth) printf("CBLOCKHELD|%d\n", g_lockDepth);
}
static void connected(const uint8_t *b, uint8_t ch) {
  arduino_event_t ev; memset(&ev, 0, sizeof(ev));
  ev.event_id = ARDUINO_EVENT_WIFI_STA_CONNECTED;
  memcpy(ev.event_info.wifi_sta_connected.bssid, b, 6); ev.event_info.wifi_sta_connected.channel = ch;
  fire(&ev);
  WiFi.st = WL_CONNECTED; memcpy(WiFi.bssid, b, 6); WiFi.ch = ch;
}
static void disconnected(const uint8_t *b, uint8_t reason) {
  arduino_event_t ev; memset(&ev, 0, sizeof(ev));
  ev.event_id = ARDUINO_EVENT_WIFI_STA_DISCONNECTED;
  memcpy(ev.event_info.wifi_sta_disconnected.bssid, b, 6); ev.event_info.wifi_sta_disconnected.reason = reason;
  fire(&ev);
  WiFi.st = WL_DISCONNECTED;
}
static void gotIp() {   // not subscribed: must not reach the record
  arduino_event_t ev; memset(&ev, 0, sizeof(ev)); ev.event_id = ARDUINO_EVENT_WIFI_STA_GOT_IP; fire(&ev);
}
static void reset() {
  g_wifiLink = WifiLinkRecord(); g_wifiSelfPending = WIFI_SELF_NONE;
  g_wifiDropLogPending = g_wifiJoinLogPending = false;
  WiFi.st = WL_DISCONNECTED; memset(WiFi.bssid, 0, 6); WiFi.ch = 0;
}
static void dump(const char *tag) {
  WifiLinkRecord r; wifiLinkSnapshot(&r);
  printf("REC|%s|up=%d|join=%d|ch=%u|events=%lu|outages=%lu|drop=%d|first=%u|last=%u|dch=%u|devents=%lu|down=%lu|self=%d\n",
         tag, r.bLinkUp, r.bJoinSeen, (unsigned)r.iJoinedChannel, (unsigned long)r.iDisconnectEvents,
         (unsigned long)r.iOutages, r.bDropValid, (unsigned)r.iDropFirstReason, (unsigned)r.iDropLastReason,
         (unsigned)r.iDropChannel, (unsigned long)r.iDropEvents, (unsigned long)r.iDropDurationMs, (int)r.eDropSelf);
}
static void banner(const char *tag) { printf("BANNER|%s\n", tag); printWifiLinkBanner(); }

int main(int argc, char **argv) {
  if (argc < 2) return 2;
  const char *sc = argv[1];
  g_ms = 5000;
  wifiLinkEventsBegin(); wifiLinkEventsBegin();   // register once
  printf("REG|%d|%d|%d\n", WiFi.n, WiFi.n > 0 ? (int)WiFi.ids[0] : -1, WiFi.n > 1 ? (int)WiFi.ids[1] : -1);

  if (!strcmp(sc, "boot")) {               // fresh boot: the association is seen by the callback
    g_ms = 8000; connected(AP_A, 6); gotIp(); wifiLinkEventsLog();
    dump("boot"); banner("boot");
  } else if (!strcmp(sc, "retries")) {     // AC3: one outage, the driver retries 31 times
    g_ms = 10000; connected(AP_A, 6); wifiLinkEventsLog();
    g_ms = 100000; disconnected(AP_A, 200); wifiLinkEventsLog();
    for (int i = 0; i < 30; i++) { g_ms += 3000; disconnected(AP_A, 201); wifiLinkEventsLog(); }
    g_ms += 1000; disconnected(AP_A, 203);
    dump("during");
    g_ms = 201000; connected(AP_A, 6); wifiLinkEventsLog();
    dump("after");
    g_ms = 261000; banner("retries");
  } else if (!strcmp(sc, "ethernet")) {    // AC8: Ethernet takeover is recorded as self, not as an outage
    g_ms = 10000; connected(AP_A, 6); wifiLinkEventsLog();
    g_ms = 50000; wifiLinkMarkSelf(WIFI_SELF_ETHERNET); disconnected(AP_A, 8); wifiLinkEventsLog();
    dump("wired");
    g_ms = 80000; banner("wired");
  } else if (!strcmp(sc, "deadlink")) {    // TASK-1450 forced reconnect is self, not an outage
    g_ms = 10000; connected(AP_A, 6);
    g_ms = 60000; wifiLinkMarkSelf(WIFI_SELF_DEAD_LINK); disconnected(AP_A, 8); wifiLinkEventsLog();
    g_ms = 64000; connected(AP_A, 6); wifiLinkEventsLog();
    dump("deadlink");
    g_ms = 70000; banner("deadlink");
  } else if (!strcmp(sc, "apretry")) {     // AP retry timeout during a field outage: no relabel, no leak
    g_ms = 10000; connected(AP_A, 6);
    g_ms = 20000; disconnected(AP_A, 200);
    g_ms = 30000; disconnected(AP_A, 201);
    g_ms = 90000; wifiLinkMarkSelf(WIFI_SELF_AP_RETRY);   // the link is down: nothing to attribute
    dump("aprouting");
    g_ms = 400000; connected(AP_B, 11);
    g_ms = 500000; disconnected(AP_B, 2);                  // a later field drop must not be self
    dump("later");
  } else if (!strcmp(sc, "staleself")) {   // a mark whose disconnect never came is cleared by the next connect
    g_ms = 10000; connected(AP_A, 6);
    wifiLinkMarkSelf(WIFI_SELF_DEAD_LINK); connected(AP_A, 6);
    g_ms = 20000; disconnected(AP_A, 2);
    dump("stale");
  } else if (!strcmp(sc, "catchup")) {     // AC5: the boot association finished before registration
    WiFi.st = WL_CONNECTED; memcpy(WiFi.bssid, AP_B, 6); WiFi.ch = 11;
    g_ms = 9000; wifiLinkEventsLog();
    dump("catchup"); banner("catchup");
    g_ms = 30000; disconnected(AP_B, 2); wifiLinkEventsLog();
    dump("catchup_drop");
  } else if (!strcmp(sc, "wrap")) {        // millis() wraps inside the outage
    g_ms = 0xFFFFF000u; connected(AP_A, 6);
    g_ms = 0xFFFFF800u; disconnected(AP_A, 200);
    g_ms = 0x00001C00u; connected(AP_A, 6);
    dump("wrap");
  } else {
    return 3;
  }
  printf("LOCK|%d|%d|%d\n", g_lockViolations, g_lockDepth, g_lockEnters);
  return 0;
}
"""


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return "\n".join(l.split("//", 1)[0] for l in text.splitlines())


def block(src, begin, end):
    a, b = src.find(begin), src.find(end)
    if a < 0 or b < 0 or b < a:
        return None
    return src[a:b]


def function_body(src, signature):
    """Body of the function whose definition starts with signature (brace-matched)."""
    a = src.find(signature)
    if a < 0:
        return None
    i = src.find("{", a)
    depth = 0
    for j in range(i, len(src)):
        if src[j] == "{":
            depth += 1
        elif src[j] == "}":
            depth -= 1
            if depth == 0:
                return src[i + 1:j]
    return None


def parse(out):
    rows = {"LOG": [], "TEL": [], "REC": {}, "BANNER": []}
    for line in out.splitlines():
        parts = line.split("|")
        k = parts[0]
        if k == "LOG":
            rows["LOG"].append("|".join(parts[1:]))
        elif k == "TEL":
            rows["TEL"].append("|".join(parts[1:]))
        elif k == "REC":
            rows["REC"][parts[1]] = {p.split("=")[0]: int(p.split("=")[1]) for p in parts[2:]}
        elif k:
            rows[k] = parts[1:]
    return rows


class WifiLinkRecordBehaviour(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        net = (FW_DIR / "networkStuff.ino").read_text(encoding="utf-8")
        types = (FW_DIR / "Networktypes.h").read_text(encoding="utf-8")
        rec = block(net, "// TASK-1446 record begin", "// TASK-1446 record end")
        typ = block(types, "// TASK-1446 types begin", "// TASK-1446 types end")
        cls.missing = rec is None or typ is None
        cls.out = {}
        if cls.missing:
            return
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text(STUBS + typ + "\n" + rec + "\n" + MAIN, encoding="utf-8")
        exe = compile_cpp(cpp, Path(cls.tmp.name) / "t.exe")
        if exe is None:
            raise unittest.SkipTest("no host C++ compiler")
        for sc in ("boot", "retries", "ethernet", "deadlink", "apretry", "staleself", "catchup", "wrap"):
            r = subprocess.run([str(exe), sc], capture_output=True, text=True, timeout=60)
            if r.returncode:
                raise AssertionError("scenario %s exit %d:\n%s" % (sc, r.returncode, r.stdout + r.stderr))
            cls.out[sc] = parse(r.stdout)

    @classmethod
    def tearDownClass(cls):
        if getattr(cls, "tmp", None):
            cls.tmp.cleanup()

    def rows(self, sc):
        self.assertFalse(self.missing, "TASK-1446 record block not found in networkStuff.ino / Networktypes.h")
        return self.out[sc]

    def test_callbacks_registered_once_for_connected_and_disconnected(self):
        r = self.rows("boot")
        self.assertEqual(r["REG"], ["2", "4", "5"])

    def test_no_logging_in_callbacks_and_lock_balanced(self):
        for sc in self.out or ["boot"]:
            r = self.rows(sc)
            self.assertNotIn("CBLOG", r, "%s: an event callback logged" % sc)
            self.assertNotIn("CBLOCKHELD", r, "%s: a callback left the lock held" % sc)
            self.assertEqual(r["LOCK"][:2], ["0", "0"], "%s: lock violation (log under lock or nested enter)" % sc)
            self.assertNotIn("TRUNC", r, "%s: a banner line exceeds the 256-byte buffer" % sc)

    def test_fresh_boot_banner(self):
        r = self.rows("boot")
        self.assertEqual(r["REC"]["boot"]["outages"], 0)
        self.assertEqual(r["REC"]["boot"]["ch"], 6)
        self.assertIn("WiFi: joined AP AA:BB:CC:00:11:22 ch 6", r["LOG"])
        self.assertEqual(r["TEL"], ["  Link : AP AA:BB:CC:00:11:22 ch 6   outages 0   disconnect events 0",
                                    "  Drop : none since boot"])

    def test_one_outage_with_many_retries(self):
        r = self.rows("retries")
        d, a = r["REC"]["during"], r["REC"]["after"]
        self.assertEqual((d["outages"], d["events"], d["devents"], d["up"], d["down"]), (1, 32, 32, 0, 0))
        self.assertEqual((a["outages"], a["events"], a["devents"]), (1, 32, 32))
        self.assertEqual((a["first"], a["last"], a["dch"], a["self"]), (200, 203, 6, 0))
        self.assertEqual(a["down"], 101000)
        self.assertIn("WiFi: link lost, reason 200 BEACON_TIMEOUT, AP AA:BB:CC:00:11:22 ch 6", r["LOG"])
        self.assertIn("WiFi: joined AP AA:BB:CC:00:11:22 ch 6 after 101 s, 32 disconnect event(s), last reason 203 ASSOC_FAIL",
                      r["LOG"])
        self.assertEqual(sum(1 for l in r["LOG"] if l.startswith("WiFi: link lost")), 1, "one outage logs once")
        self.assertEqual(r["TEL"], [
            "  Link : AP AA:BB:CC:00:11:22 ch 6   outages 1   disconnect events 32",
            "  Drop : 161 s ago, down 101 s, reason 200 BEACON_TIMEOUT (last 203 ASSOC_FAIL, 32 events), "
            "AP AA:BB:CC:00:11:22 ch 6"])

    def test_ethernet_takeover_is_self_and_not_an_outage(self):
        r = self.rows("ethernet")
        w = r["REC"]["wired"]
        self.assertEqual((w["outages"], w["events"], w["drop"], w["self"], w["first"]), (0, 1, 1, 1, 8))
        self.assertIn("WiFi: link lost, reason 8 ASSOC_LEAVE, AP AA:BB:CC:00:11:22 ch 6 (self: Ethernet takeover)", r["LOG"])
        self.assertEqual(r["TEL"], [
            "  Link : AP AA:BB:CC:00:11:22 ch 6 (down)   outages 0   disconnect events 1",
            "  Drop : 30 s ago, down ongoing, reason 8 ASSOC_LEAVE (last 8 ASSOC_LEAVE, 1 events), "
            "AP AA:BB:CC:00:11:22 ch 6, self: Ethernet takeover"])

    def test_dead_link_reconnect_is_self_and_not_an_outage(self):
        r = self.rows("deadlink")
        d = r["REC"]["deadlink"]
        self.assertEqual((d["outages"], d["drop"], d["self"], d["down"], d["up"]), (0, 1, 3, 4000, 1))
        self.assertTrue(r["TEL"][1].endswith(", self: dead-link check"), r["TEL"])

    def test_ap_retry_mark_while_down_does_not_relabel_or_leak(self):
        r = self.rows("apretry")
        a, l = r["REC"]["aprouting"], r["REC"]["later"]
        self.assertEqual((a["outages"], a["self"], a["devents"]), (1, 0, 2))
        self.assertEqual((l["outages"], l["self"], l["first"], l["dch"]), (2, 0, 2, 11))

    def test_stale_self_mark_cleared_by_connect(self):
        s = self.rows("staleself")["REC"]["stale"]
        self.assertEqual((s["outages"], s["self"]), (1, 0))

    def test_boot_catchup_fills_joined_ap(self):
        r = self.rows("catchup")
        c, d = r["REC"]["catchup"], r["REC"]["catchup_drop"]
        self.assertEqual((c["join"], c["up"], c["ch"], c["outages"]), (1, 1, 11, 0))
        self.assertEqual(r["TEL"][0], "  Link : AP 48:A9:8A:01:02:03 ch 11   outages 0   disconnect events 0")
        self.assertEqual((d["outages"], d["first"], d["dch"]), (1, 2, 11), "a drop after the catch-up is an outage")

    def test_duration_across_millis_wrap(self):
        self.assertEqual(self.rows("wrap")["REC"]["wrap"]["down"], 0x1C00 + 0x800)


DEVICE_INFO_KEYS = ("wifi_ap_bssid", "wifi_ap_channel", "wifi_outages", "wifi_disconnect_events", "wifi_drop_bssid",
                    "wifi_drop_channel", "wifi_drop_reason", "wifi_drop_reason_name", "wifi_drop_last_reason",
                    "wifi_drop_last_reason_name", "wifi_drop_events", "wifi_drop_down_sec", "wifi_drop_ago_sec",
                    "wifi_drop_self")
CALLBACK_FORBIDDEN = re.compile(r"\b(Debug\w*|log_[a-z]|printf|_debugPrintf_P|debugTelnet|Serial|malloc|calloc|free|"
                                r"new|delete|String|delay|snprintf\w*|wifiReasonName|disconnectReasonName)\b")


class WifiLinkRecordSourceGates(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.net = (FW_DIR / "networkStuff.ino").read_text(encoding="utf-8").replace("\r", "")
        cls.eth = (FW_DIR / "Ethernet.ino").read_text(encoding="utf-8").replace("\r", "")
        cls.rest = (FW_DIR / "restAPI.ino").read_text(encoding="utf-8").replace("\r", "")

    def body(self, sig):
        b = function_body(self.net, sig)
        self.assertIsNotNone(b, "%s not found in networkStuff.ino" % sig)
        return strip_comments(b)

    def test_event_callbacks_only_record_under_the_lock(self):
        for sig in ("static void onWifiLinkEvent(", "static void wifiLinkOnConnected(", "static void wifiLinkOnDisconnected("):
            body = self.body(sig)
            self.assertEqual(CALLBACK_FORBIDDEN.findall(body), [], "%s logs, formats or allocates" % sig)
        for sig in ("static void wifiLinkOnConnected(", "static void wifiLinkOnDisconnected(", "void wifiLinkMarkSelf("):
            body = self.body(sig)
            lo, hi = body.find("portENTER_CRITICAL(&g_wifiLinkMux);"), body.rfind("portEXIT_CRITICAL(&g_wifiLinkMux);")
            self.assertTrue(0 <= lo < hi, sig)
            for w in re.finditer(r"\b(g_wifiLink\.\w+|g_wifiSelfPending|g_wifiDropLogPending|g_wifiJoinLogPending)\s*(=|\+\+)", body):
                self.assertTrue(lo < w.start() < hi, "%s: %s written outside the lock" % (sig, w.group(1)))

    def test_registration_and_drain_call_sites(self):
        start = self.body("void startWiFi(")
        lines = [l.strip() for l in start.split("\n") if l.strip()]
        self.assertEqual(lines[:2], ["WiFi.mode(WIFI_STA);", "wifiLinkEventsBegin();"])
        loop = self.body("void loopWifi() {")
        first = [l.strip() for l in loop.split("\n") if l.strip()][0]
        self.assertEqual(first, "wifiLinkEventsLog();", "the drain must run before the Ethernet early return")

    def test_self_marks_sit_right_before_their_disconnect(self):
        eth = strip_comments(function_body(self.eth, "static void switchToEthernet()") or "")
        self.assertRegex(eth, r"wifiLinkMarkSelf\(WIFI_SELF_ETHERNET\);\s*WiFi\.disconnect\(\);\s*WiFi\.mode\(WIFI_OFF\);")
        dl = self.body("static void wifiDeadLinkReconnect()")
        self.assertRegex(dl, r"wifiLinkMarkSelf\(WIFI_SELF_DEAD_LINK\);\s*platformWifiForceReconnect\(\);")
        loop = self.body("void loopWifi() {")
        self.assertRegex(loop, r"wifiLinkMarkSelf\(WIFI_SELF_AP_RETRY\);\s*WiFi\.disconnect\(\);")
        self.assertEqual(len(re.findall(r"\bWiFi\.disconnect\(", strip_comments(self.net))), 1,
                         "a new firmware-initiated WiFi.disconnect() needs a wifiLinkMarkSelf()")

    def test_banner_lines_between_net_and_heap(self):
        b = self.body("static void sendTelnetBanner(")
        net, link, heap = b.find('"  Net  : "'), b.find("printWifiLinkBanner();"), b.find('"  Heap : free')
        self.assertTrue(0 <= net < link < heap, "printWifiLinkBanner() must sit after the Net block, before Heap")

    def test_device_info_emits_the_14_keys_from_the_snapshot(self):
        # Bounded by its closing marker: string literals in the body hold unbalanced braces.
        fn = block(self.rest, "void sendDeviceInfoV2()", "} // sendDeviceInfoV2()")
        self.assertIsNotNone(fn, "sendDeviceInfoV2() not found in restAPI.ino")
        fn = strip_comments(fn)
        closure = fn.find("restSendChunked(")
        self.assertGreater(fn.find("wifiLinkSnapshot(&snap->wifiLink);"), 0)
        self.assertLess(fn.find("wifiLinkSnapshot(&snap->wifiLink);"), closure, "snapshot after the closure starts")
        chain_end = fn.find('je.field(F("wifiquality_text"), dBmtoQuality(snap->rssi));')
        time_block = fn.find('je.field(F("ntpenable")')
        self.assertTrue(0 < closure < chain_end < time_block)
        for k in DEVICE_INFO_KEYS:
            pos = fn.find('je.field(F("%s"),' % k)
            self.assertTrue(chain_end < pos < time_block, "%s missing or inside a mode branch" % k)
            self.assertEqual(fn.count('F("%s")' % k), 1, k)
        emit = fn[chain_end:time_block]
        self.assertNotRegex(emit, r"\b(millis|wifiLinkSnapshot|WiFi\.)\b", "live read inside the chunked closure")
        self.assertEqual(len(re.findall(r'je\.field\(F\("wifi_(ap|outages|disconnect|drop)', emit)), 14)


if __name__ == "__main__":
    unittest.main(verbosity=2)
