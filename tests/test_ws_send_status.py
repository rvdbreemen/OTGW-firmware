#!/usr/bin/env python3
"""TASK-3017: a /ws client whose send queue stays full must be visible in ws_drops.

ESPAsyncWebServer's AsyncWebSocketClient::_queueMessage() silently discards a message for a client whose queue holds
WS_MAX_QUEUED_MESSAGES; textAll() then returns DISCARDED (no client took it) or PARTIALLY_ENQUEUED (some did). The
firmware ignored that result, so a client that stopped reading kept its socket, got nothing, and ws_drops stayed 0
(capture e2e on .61, 2026-11-21 16:51:44).

Nothing of the firmware is re-implemented: the test cuts the real code from src/OTGW-firmware/webSocketStuff.ino (the
wsSafeBuf declarations, wsNoteSendStatus() when present, sendLogToWebSocket()) and compiles it with the real WsUtf8.h
against a stub AsyncWebSocket whose textAll() returns a chosen SendStatus (the library's contract, AsyncWebSocket.cpp
textAll(): hit == 0 ? DISCARDED : miss == 0 ? ENQUEUED : PARTIALLY_ENQUEUED). The library's queue itself is not under
test here; a bench run with a paused /ws reader covers the real library.

Red on the code before TASK-3017 (drops stay 0 for DISCARDED and PARTIALLY_ENQUEUED), green on the fix.
Run: python tests/test_ws_send_status.py
"""
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from test_sat_pwm_override import compile_cpp  # noqa: E402

FW = HERE.parent / "src" / "OTGW-firmware"
WS_INO = FW / "webSocketStuff.ino"

PRE = r"""
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include "@FW@/WsUtf8.h"
#define OT_LOG_BUFFER_SIZE 512
struct AsyncWebSocket {
  enum SendStatus { DISCARDED = 0, ENQUEUED = 1, PARTIALLY_ENQUEUED = 2 };
  SendStatus next = ENQUEUED;
  int calls = 0;
  SendStatus textAll(const char*) { calls++; return next; }
};
static AsyncWebSocket otLogWs;
struct { struct { uint32_t iWsDropsTotal = 0, iWsAllocFail = 0; } heapdiag; } state;
#include <new>
#define PSTR(s) (s)
#define DebugTf(...) ((void)0)
static unsigned long millis() { return 0; }
static uint32_t platformFreeHeap() { return 0; }
static uint32_t platformMaxFreeBlock() { return 0; }
static bool g_clients = true, g_heapOk = true;
static bool hasWebSocketClients() { return g_clients; }
static bool canSendWebSocket() { return g_heapOk; }
@CODE@
int main(int argc, char** argv) {
  const char* sc = argv[1];
  if (!strcmp(sc, "enqueued"))  otLogWs.next = AsyncWebSocket::ENQUEUED;
  if (!strcmp(sc, "discarded")) otLogWs.next = AsyncWebSocket::DISCARDED;
  if (!strcmp(sc, "partial"))   otLogWs.next = AsyncWebSocket::PARTIALLY_ENQUEUED;
  if (!strcmp(sc, "badutf8"))   otLogWs.next = AsyncWebSocket::DISCARDED;
  const int n = atoi(argv[2]);
  for (int i = 0; i < n; i++) sendLogToWebSocket(!strcmp(sc, "badutf8") ? "T80190000\xB0" : "T80190000");
  if (!strcmp(sc, "noclients")) { g_clients = false; otLogWs.next = AsyncWebSocket::DISCARDED; sendLogToWebSocket("x"); }
  printf("drops=%u calls=%d\n", (unsigned)state.heapdiag.iWsDropsTotal, otLogWs.calls);
  return 0;
}
"""


def build_cpp(src):
    src = src.replace("\r\n", "\n")
    a = src.index("char wsSafeBuf[OT_LOG_BUFFER_SIZE];")
    b = src.index("void sendLogToWebSocket(const char* logMessage) {", a)
    b = src.index("\n}\n", b) + 3
    return PRE.replace("@FW@", FW.as_posix()).replace("@CODE@", src[a:b])


class WsSendStatus(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.d = Path(tempfile.mkdtemp(prefix="ws_send_status_"))
        cpp = cls.d / "ws.cpp"
        cpp.write_text(build_cpp(WS_INO.read_text(encoding="utf-8")), encoding="utf-8")
        cls.exe = compile_cpp(cpp, cls.d / "ws.exe")

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.d, ignore_errors=True)

    def run_sc(self, sc, n=1):
        if self.exe is None:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe), sc, str(n)], capture_output=True, text=True, check=True).stdout.strip()
        return dict(kv.split("=", 1) for kv in out.split())

    def test_enqueued_counts_nothing(self):
        self.assertEqual(self.run_sc("enqueued", 40), {"drops": "0", "calls": "40"})

    def test_a_stalled_client_counts_every_discarded_message(self):
        # 40 messages to one client whose queue is full: every one is discarded and counted
        self.assertEqual(self.run_sc("discarded", 40), {"drops": "40", "calls": "40"})

    def test_a_stalled_client_beside_a_healthy_one_counts_too(self):
        self.assertEqual(self.run_sc("partial", 7)["drops"], "7")

    def test_the_sanitised_path_counts_too(self):
        self.assertEqual(self.run_sc("badutf8", 3)["drops"], "3")

    def test_no_clients_no_send_no_count(self):
        self.assertEqual(self.run_sc("noclients", 0), {"drops": "0", "calls": "0"})


if __name__ == "__main__":
    unittest.main(verbosity=2)
