"""TASK-3119: a /ws broadcast that cannot allocate is dropped and counted; it never throws out of the firmware's own code.

Bench 2026-11-27 (.76, no PSRAM, release a9aa6a9 and 456): a K hold took the free heap to about 2 KB within a second; AsyncWebSocket::textAll()
-> AsyncWebSocketClient::_queueMessage() -> std::deque<AsyncWebSocketMessage>::emplace_back -> operator new threw std::bad_alloc on the
loop task, nothing caught it, std::terminate -> abort -> panic. canSendWebSocket() only blocks below 3072 B of TOTAL free heap
(ADR-167) and the allocation is made a moment later: a gate alone cannot close that window, so the send sites catch the exception.

What this test runs on the host: the REAL send paths cut out of webSocketStuff.ino (sendLogToWebSocket for valid and for sanitised lines,
sendWebSocketJSON, the 30 s keepalive in handleWebSocket and the helper they share) against a stub AsyncWebSocket whose textAll() throws
std::bad_alloc on demand. With the throw armed every path must return normally, count one alloc drop (and one ws drop) and log at most once
per interval; with it off the messages go out and nothing is counted. A text guard checks that no bare otLogWs.textAll( is left outside the helper.
Red on the code before the change (OTGW_WS_INO=<old file>): the helper does not exist and the exception escapes the old functions.
What it cannot reach: the real library, operator new on the unit, and whether the library state stays sound after the throw (its
queue lock is an RAII guard and std::deque::emplace_back keeps the strong guarantee; that is read, not run). Bench: the K hold on .76.
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

WS_INO = Path(os.environ.get("OTGW_WS_INO", REPO / "src" / "OTGW-firmware" / "webSocketStuff.ino"))

PRELUDE = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#define PSTR(s) (s)
static unsigned long g_now = 0;
static unsigned long millis() { return g_now; }
static int g_log = 0, g_sent = 0, g_throw = 0;
#define DebugTf(...) (++g_log)
#define Debugf(...) ((void)0)
struct AsyncWebSocket {
  typedef enum { DISCARDED = 0, ENQUEUED = 1, PARTIALLY_ENQUEUED = 2 } SendStatus;
  SendStatus textAll(const char*) { if (g_throw) throw std::bad_alloc(); ++g_sent; return ENQUEUED; }
  uint32_t count() const { return 1; }
  void cleanupClients(uint16_t) {}
};
static AsyncWebSocket otLogWs;
struct { struct { uint32_t iWsDropsTotal = 0, iWsAllocFail = 0; } heapdiag; } state;
static bool hasWebSocketClients() { return true; }
static bool canSendWebSocket() { return true; }
static bool wsUtf8Valid(const char* s) { return !strchr(s, '\xff'); }
static void wsUtf8Sanitize(const char* s, char* d, size_t n) { strncpy(d, s, n - 1); d[n - 1] = 0; }
static char wsSafeBuf[256];
static volatile bool wsSafeBufBusy = false;
static bool wsInitialized = true;
static const uint16_t MAX_WEBSOCKET_CLIENTS = 3;
static uint32_t platformFreeHeap() { return 1700; }
static uint32_t platformMaxFreeBlock() { return 900; }
static unsigned long lastKeepaliveMs = 0;
static const unsigned long KEEPALIVE_INTERVAL_MS = 30000;
"""

MAIN = r"""
int main(int argc, char** argv) {
  const char* mode = argc > 1 ? argv[1] : "log";
  g_throw = argc > 2 && argv[2][0] == '1';
  g_now = 100000;
  for (int i = 0; i < 3; i++) {
    if (!strcmp(mode, "log")) sendLogToWebSocket("a log line");
    else if (!strcmp(mode, "logbad")) sendLogToWebSocket("bad \xff byte");
    else if (!strcmp(mode, "json")) sendWebSocketJSON("{\"type\":\"status\"}");
    else if (!strcmp(mode, "keepalive")) { lastKeepaliveMs = 0; handleWebSocket(); }
    g_now += 1000;   // inside one log interval
  }
  std::printf("returned sent=%d allocFail=%u wsDrops=%u log=%d\n", g_sent, (unsigned)state.heapdiag.iWsAllocFail,
              (unsigned)state.heapdiag.iWsDropsTotal, g_log);
  return 0;
}
"""


def cut(src, signature):
    m = re.search(r"^" + re.escape(signature) + r".*?^\}\s*$", src, re.S | re.M)
    if not m:
        raise AssertionError(signature + " not found")
    return m.group(0)


class SendNoThrow(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.src = WS_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        parts = [
            cut(cls.src, "static void wsNoteSendStatus(AsyncWebSocket::SendStatus st) {"),
            cut(cls.src, "static AsyncWebSocket::SendStatus wsTextAllNoThrow(const char* msg) {"),
            cut(cls.src, "void sendLogToWebSocket("),
            cut(cls.src, "void sendWebSocketJSON("),
            cut(cls.src, "void handleWebSocket("),
        ]
        consts = "".join(m.group(0) + "\n" for m in re.finditer(r"^static const uint32_t WS_ALLOC_LOG_INTERVAL_MS\s*=.*;", cls.src, re.M))
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "s.cpp"
        cpp.write_text(PRELUDE + consts + "\n".join(parts) + MAIN, encoding="utf-8")
        cls.exe = compile_cpp(cpp, Path(cls.tmp.name) / ("s.exe" if os.name == "nt" else "s"))
        if cls.exe is None:
            raise unittest.SkipTest("no host C++ compiler")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_mode(self, mode, throw):
        r = subprocess.run([str(self.exe), mode, "1" if throw else "0"], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, "the exception escaped: " + r.stdout + r.stderr)
        return dict(kv.split("=") for kv in r.stdout.split()[1:])

    def test_every_send_path_survives_bad_alloc_and_counts(self):
        for mode in ("log", "logbad", "json", "keepalive"):
            with self.subTest(mode=mode):
                o = self.run_mode(mode, True)
                self.assertEqual((o["sent"], o["allocFail"], o["wsDrops"]), ("0", "3", "3"))
                self.assertEqual(o["log"], "1", "one rate-limited log line per interval, not one per drop")

    def test_without_the_throw_nothing_is_dropped_or_counted(self):
        for mode in ("log", "logbad", "json", "keepalive"):
            with self.subTest(mode=mode):
                o = self.run_mode(mode, False)
                self.assertEqual((o["sent"], o["allocFail"], o["wsDrops"], o["log"]), ("3", "0", "0", "0"))

    def test_names_are_declared_before_their_first_use(self):
        # the host harness orders the pieces itself; the real file is one translation unit in text order (the build broke on this)
        for name, decl in (("wsTextAllNoThrow", "static AsyncWebSocket::SendStatus wsTextAllNoThrow(const char* msg);"),
                           ("wsNoteSendStatus", "static void wsNoteSendStatus(AsyncWebSocket::SendStatus st);")):
            first_use = min(m.start() for m in re.finditer(name + r"\(", self.src) if not self.src[:m.start()].rstrip().endswith(("SendStatus", "void")))
            self.assertIn(decl, self.src, name + " needs a forward declaration")
            self.assertLess(self.src.index(decl), first_use, name + " is used before it is declared")

    def test_no_bare_textall_outside_the_helper(self):
        helper = cut(self.src, "static AsyncWebSocket::SendStatus wsTextAllNoThrow(const char* msg) {")
        rest = self.src.replace(helper, "")
        code = "\n".join(ln.split("//")[0] for ln in rest.split("\n"))
        self.assertNotIn("otLogWs.textAll(", code, "a send site bypasses the no-throw helper")
        self.assertIn("catch (const std::bad_alloc&)", helper)


if __name__ == "__main__":
    unittest.main()
