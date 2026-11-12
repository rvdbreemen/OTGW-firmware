#!/usr/bin/env python3
"""TASK-1746 + TASK-1868: the reboot teardown stops the port-80 listener first; /ReBoot logs its own reason.

TASK-1746: 09's beta.215 bench panicked in tiT (AsyncTCP tcp_accept <- tcp_input) during the /ReBoot teardown:
a new connection was accepted while prepareForReboot() and the delay(2000) in doRestart() ran. Robert's decision
(B, firmware only): close the listener before the teardown. prepareForReboot() now calls server.end() before
anything else. In the pinned AsyncTCP, AsyncServer::end() clears the accept callback and closes only the listen
pcb, so open connections and the queued /ReBoot redirect page are not touched.

TASK-1868: doRedirect() always passed "Reboot after upgrade" to requestDeferredReboot(), so a plain /ReBoot and
a Wi-Fi reset were logged as an upgrade. The caller now passes the reason.

The code under test is REAL: reBootESP(), resetWirelessButton() and doRedirect() from FSexplorer.ino, and
prepareForReboot() from helperStuff.ino, compiled on the host with stubs for the web, MQTT and debug layers.

Old-vs-fix: OTGW_FSEXPLORER_INO / OTGW_HELPER_INO point at the old files.
Run: python tests/test_reboot_listener_and_reason.py
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

FW = REPO / "src" / "OTGW-firmware"
FSX = Path(os.environ.get("OTGW_FSEXPLORER_INO", FW / "FSexplorer.ino")).read_text(encoding="utf-8").replace("\r\n", "\n")
HELPER = Path(os.environ.get("OTGW_HELPER_INO", FW / "helperStuff.ino")).read_text(encoding="utf-8").replace("\r\n", "\n")
ASYNCTCP = REPO / ".pio" / "libdeps" / "esp32-combo" / "AsyncTCP" / "src" / "AsyncTCP.cpp"


def braced(src, start_pat):
    m = re.search(start_pat, src, re.M)
    if not m:
        return None
    i, depth = src.index("{", m.end() - 1), 0
    for j in range(i, len(src)):
        if src[j] == "{":
            depth += 1
        elif src[j] == "}":
            depth -= 1
            if depth == 0:
                return src[m.start():j + 1]
    return None


STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
struct __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper*>(s))
#define PSTR(s) (s)
#define snprintf_P snprintf
static std::vector<std::string> g_log;          // call order
static std::string g_body, g_reason;
static int g_rebootRequests = 0;
struct String {
  std::string s;
  String(const char *c = "") : s(c) {}
  String(int v) : s(std::to_string(v)) {}
  String(const std::string &v) : s(v) {}
  void replace(const char *a, const char *b) {
    std::string A(a), B(b); size_t p = 0;
    while ((p = s.find(A, p)) != std::string::npos) { s.replace(p, A.size(), B); p += B.size(); }
  }
  const char *c_str() const { return s.c_str(); }
  String operator+(const __FlashStringHelper *f) const { return String(s + reinterpret_cast<const char*>(f)); }
  String operator+(const String &o) const { return String(s + o.s); }
};
struct AsyncWebServerRequest {};
struct AsyncResponseStream {
  void print(const __FlashStringHelper *f) { g_body += reinterpret_cast<const char*>(f); }
  void print(const char *c) { g_body += c; }
  void print(const String &v) { g_body += v.s; }
};
static AsyncResponseStream g_stream;
static void webBeginRequest(AsyncWebServerRequest *) {}
static bool checkHttpAuth() { return true; }
static void resetWiFiSettings() { g_log.push_back("resetWiFiSettings"); }
static void webPushHeader(const __FlashStringHelper *, const char *) {}
static AsyncResponseStream *restBeginStream(const char *) { return &g_stream; }
static void restFinalize() { g_log.push_back("restFinalize"); }
static void requestDeferredReboot(const char *r) { g_rebootRequests++; g_reason = r ? r : "(null)"; g_log.push_back("requestDeferredReboot"); }
template <typename T> static void DebugTln(T) {}
template <typename... A> static void DebugTf(const char *, A...) {}
static void DebugFlush() {}
static uint32_t millis() { return 0; }
static uint32_t platformFreeHeap() { return 100000; }
static uint32_t platformMaxFreeBlock() { return 50000; }
struct AsyncWebServer { void end() { g_log.push_back("server.end"); } };   // the only server call allowed here
static AsyncWebServer server;
static void doMqttDisconnect() { g_log.push_back("mqtt"); }
static void doWebSocketClose() { g_log.push_back("ws"); }
struct Stream25238 { void stop() { g_log.push_back("otgwstream"); } };
static Stream25238 OTGWstream;
template <typename A, typename B> static auto max(A a, B b) { return a > b ? a : b; }
template <typename A, typename B> static auto min(A a, B b) { return a < b ? a : b; }
"""

MAIN = r"""
static void dump(const char *tag) {
  printf("%s reason=[%s] requests=%d page=%d order=", tag, g_reason.c_str(), g_rebootRequests,
         (int)(g_body.find("Redirect in") != std::string::npos));
  for (auto &e : g_log) printf("%s,", e.c_str());
  printf("\n");
  g_log.clear(); g_body.clear(); g_reason.clear(); g_rebootRequests = 0;
}
int main() {
  AsyncWebServerRequest rq;
  reBootESP(&rq);           dump("REBOOT");
  resetWirelessButton(&rq); dump("WIFI");
  prepareForReboot();       dump("TEARDOWN");
  return 0;
}
"""


class RealRebootPaths(unittest.TestCase):
    out = {}

    @classmethod
    def setUpClass(cls):
        parts = [braced(FSX, r"^void doRedirect\(String msg[^)]*\)\s*\{"),
                 braced(FSX, r"^void reBootESP\(AsyncWebServerRequest \*request\)\s*\{"),
                 braced(FSX, r"^void resetWirelessButton\(AsyncWebServerRequest \*request\)\s*\{"),
                 braced(HELPER, r"^static void prepareForReboot\(\)\s*\{")]
        if None in parts:
            raise AssertionError("a function under test was not found: %r" % [p is None for p in parts])
        # doRedirect is defined after its callers in FSexplorer.ino: declare it first, with its real signature
        sig = re.search(r"^void doRedirect\(String msg[^)]*\)", FSX, re.M).group(0)
        src = STUBS + sig + ";\n" + "\n".join(parts) + MAIN
        with tempfile.TemporaryDirectory() as d:
            cpp = Path(d) / "t.cpp"
            cpp.write_text(src, encoding="utf-8")
            exe = compile_cpp(cpp, Path(d) / ("t.exe" if os.name == "nt" else "t"))
            if exe is None:
                raise unittest.SkipTest("no host compiler")
            text = subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout
        for line in text.splitlines():
            tag, rest = line.split(" ", 1)
            cls.out[tag] = rest

    # TASK-1868
    def test_plain_reboot_logs_a_user_reboot_not_an_upgrade(self):
        line = self.out["REBOOT"]
        self.assertIn("requests=1", line)
        self.assertNotIn("reason=[Reboot after upgrade]", line, line)
        self.assertIn("reason=[User reboot (/ReBoot)]", line, line)

    def test_reset_wireless_logs_its_own_reason(self):
        line = self.out["WIFI"]
        self.assertIn("reason=[Reset wireless settings]", line, line)
        self.assertRegex(line, r"order=resetWiFiSettings,restFinalize,requestDeferredReboot,")

    def test_redirect_page_is_still_built_before_the_reboot_request(self):   # the browser still gets the page
        for tag in ("REBOOT", "WIFI"):
            self.assertIn("page=1", self.out[tag], self.out[tag])
            self.assertIn("restFinalize,requestDeferredReboot,", self.out[tag])

    # TASK-1746
    def test_teardown_stops_the_listener_before_anything_else(self):
        line = self.out["TEARDOWN"]
        self.assertRegex(line, r"order=server\.end,mqtt,ws,otgwstream,$", line)


class PinnedAsyncTcpEndOnlyClosesTheListener(unittest.TestCase):
    """The coordinator's caution: server.end() must not abort the in-flight redirect response."""

    def test_asyncserver_end_touches_only_the_listen_pcb(self):
        if not ASYNCTCP.exists():
            self.skipTest("AsyncTCP not in .pio/libdeps (run a build first)")
        body = braced(ASYNCTCP.read_text(encoding="utf-8"), r"^void AsyncServer::end\(\)\s*\{")
        self.assertIsNotNone(body)
        self.assertIn("tcp_accept(_pcb, NULL);", body)   # no accept callback after this
        self.assertIn("tcp_close(_pcb)", body)
        idents = set(re.findall(r"\b_[a-zA-Z]\w*", body))
        self.assertEqual(idents, {"_pcb"}, "AsyncServer::end() touches more than the listen pcb: %s" % idents)


if __name__ == "__main__":
    unittest.main(verbosity=2)
