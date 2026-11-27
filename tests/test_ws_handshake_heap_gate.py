"""TASK-3119: a new /ws connection is refused at the handshake while the heap is low, before the library builds a client object.

On the bench (beta.453, K hold, tabs reconnecting every 5 s) async_tcp panicked inside AsyncWebSocketClient::_queueControl, called from
close() in our WS_EVT_CONNECT 'Low heap' branch: by then the library had already constructed the client, whose std::recursive_mutex is
created by pthread_mutex_init() in its constructor (malloc plus a FreeRTOS semaphore), which fails when the heap is critical. So the
refusal has to come BEFORE the client exists: otLogWs.handleHandshake() (AsyncWebSocket::handleRequest calls it before it sends the 101
response and before _newClient()).

What this test runs on the host: the real wsAdmitHandshake() decision, cut out of webSocketStuff.ino, on a grid of free heap and
connected clients. What it reads as text: that startWebSocket() registers the handshake handler before server.addHandler() and that
the handler uses the decision. What it cannot run: the library crash itself (it needs the ESP pthread and the heap of the unit); that
is the bench run of TASK-3119.
Red on the code before the change: wsAdmitHandshake() does not exist.
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
HELPER_INO = REPO / "src" / "OTGW-firmware" / "helperStuff.ino"

MAIN = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#define MAX_WEBSOCKET_CLIENTS 3
#define HEAP_WARNING_THRESHOLD @@WARN@@
@@FN@@
int main(int argc, char** argv) {
  if (argc < 3) return 2;
  std::printf("%d\n", (int)wsAdmitHandshake((uint32_t)atol(argv[1]), (uint16_t)atoi(argv[2]), 0u, 0u));
  return 0;
}
"""


def cut(src, signature):
    m = re.search(r"^" + re.escape(signature) + r".*?^\}\s*$", src, re.S | re.M)
    if not m:
        raise AssertionError(signature + " not found")
    return m.group(0)


class HandshakeGate(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.src = WS_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        warn = re.search(r"#define HEAP_WARNING_THRESHOLD\s+(\d+)", HELPER_INO.read_text(encoding="utf-8"))
        assert warn, "HEAP_WARNING_THRESHOLD not found in helperStuff.ino"
        cls.warn = int(warn.group(1))
        consts = "".join(m.group(0) + chr(10) for m in re.finditer(r"^static const uint32_t WS_RECOVERY_COOLDOWN_MS\s*=.*;", cls.src, re.M))
        fn = consts + cut(cls.src, "static uint32_t wsCooldownLeftMs(") + chr(10) + cut(cls.src, "static bool wsAdmitHandshake(")
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "gate.cpp"
        cpp.write_text(MAIN.replace("@@FN@@", fn).replace("@@WARN@@", str(cls.warn)), encoding="utf-8")
        cls.exe = compile_cpp(cpp, Path(cls.tmp.name) / ("gate.exe" if os.name == "nt" else "gate"))
        if cls.exe is None:
            raise unittest.SkipTest("no host C++ compiler")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def admit(self, free, count):
        r = subprocess.run([str(self.exe), str(free), str(count)], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        return r.stdout.strip() == "1"

    def test_threshold_is_the_warning_tier(self):
        self.assertEqual(self.warn, 5120)

    def test_low_heap_is_refused(self):
        self.assertFalse(self.admit(0, 0))
        self.assertFalse(self.admit(2048, 0))
        self.assertFalse(self.admit(self.warn - 1, 0))

    def test_enough_heap_is_admitted(self):
        self.assertTrue(self.admit(self.warn, 0))
        self.assertTrue(self.admit(60000, 2))

    def test_full_client_list_is_refused_before_a_client_exists(self):
        # the new client is not counted yet at the handshake: 3 connected (MAX) means a fourth is refused
        self.assertFalse(self.admit(60000, 3))
        self.assertFalse(self.admit(60000, 7))

    def test_handler_is_registered_before_the_websocket_is_attached(self):
        body = cut(self.src, "void startWebSocket()")
        self.assertIn("otLogWs.handleHandshake(wsHandshakeHandler)", body)
        self.assertLess(body.index("otLogWs.handleHandshake("), body.index("server.addHandler(&otLogWs)"))
        # the handler takes its decision from wsAdmitHandshake(), with the live heap and the connected count
        handler = cut(self.src, "static bool wsHandshakeHandler(")
        self.assertIn("wsAdmitHandshake(freeHeap, connected, now, g_wsRecoveryMs)", handler)
        self.assertIn("platformFreeHeap()", handler)
        self.assertIn("otLogWs.count()", handler)

    def test_event_branches_stay_as_the_second_line(self):
        ev = cut(self.src, "void webSocketEvent(")
        self.assertIn("Low heap", ev)
        self.assertIn("Max clients", ev)


if __name__ == "__main__":
    unittest.main()
