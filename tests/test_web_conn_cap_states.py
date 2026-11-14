#!/usr/bin/env python3
"""TASK-1947: which port-80 connections the ADR-188 web connection cap counts, per TCP state.

    python tests/test_web_conn_cap_states.py

Characterisation test of the CURRENT counting (no fix yet): platformTcpTxHoldingOnPort() is sliced
from src/libraries/Platform/src/platform_esp32.h and compiled against a stub tcp_pcb/tcp_seg list.
webConnCapExceeded() (restAPI.ino) refuses with 503 when held - ws > WEB_MAX_TX_CONNECTIONS (4).

What it shows (ADR-188 Decision, by design):
  - an ESTABLISHED connection counts even with empty queues, unless it is a waiter of the native file
    queue: an idle socket the browser opened ahead of a request (speculative preconnect) counts;
  - a closing connection (FIN_WAIT_1, CLOSING, LAST_ACK) counts while any segment sits in unsent or
    unacked, also when that segment is only the zero-length FIN.
ESPAsyncWebServer 3.11.2 answers with "Connection: close", so every request is a new socket that then
closes; a fresh load therefore leaves closing sockets behind for one round trip each.

The scenario rows model a fresh load with 2 requests in flight, the counting the bench must confirm or
refute (AC2). They are a model, not a measurement.
"""
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_published_curve import compile_cpp  # noqa: E402

REPO = Path(__file__).resolve().parent.parent
SHIM = REPO / "src/libraries/Platform/src/platform_esp32.h"

HARNESS = r"""
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
enum tcp_state { CLOSED, LISTEN, SYN_SENT, SYN_RCVD, ESTABLISHED, FIN_WAIT_1, FIN_WAIT_2, CLOSE_WAIT,
                 CLOSING, LAST_ACK, TIME_WAIT };
struct tcp_seg { tcp_seg *next; uint16_t len; };
struct tcp_pcb { tcp_pcb *next; uint16_t local_port; tcp_state state; tcp_seg *unsent; tcp_seg *unacked;
                 void *callback_arg; };
static tcp_pcb *tcp_active_pcbs = nullptr;
#define LOCK_TCPIP_CORE() ((void)0)
#define UNLOCK_TCPIP_CORE() ((void)0)

@@SLICE@@

static tcp_seg g_fin = {nullptr, 0};     // a zero-length FIN segment
static tcp_seg g_data = {nullptr, 1436}; // one full payload segment
static tcp_pcb g_pcbs[16];
static int g_n = 0;
static int g_owner;                      // a stand-in AsyncClient address
static tcp_pcb *add(tcp_state s, tcp_seg *unsent, tcp_seg *unacked, uint16_t port = 80) {
  tcp_pcb *p = &g_pcbs[g_n++];
  *p = {tcp_active_pcbs, port, s, unsent, unacked, &g_owner};
  tcp_active_pcbs = p;
  return p;
}
static void reset() { tcp_active_pcbs = nullptr; g_n = 0; }

int main(int argc, char **argv) {
  const std::string c = argc > 1 ? argv[1] : "";
  PlatformTcpIdleConnection idle[4];
  uint8_t nidle = 0;
  reset();
  if (c == "est_idle")          add(ESTABLISHED, nullptr, nullptr);
  else if (c == "est_waiter") { tcp_pcb *p = add(ESTABLISHED, nullptr, nullptr); idle[nidle++] = {p, &g_owner}; }
  else if (c == "est_data")     add(ESTABLISHED, &g_data, nullptr);
  else if (c == "close_wait")   add(CLOSE_WAIT, nullptr, nullptr);
  else if (c == "fin_wait_1_fin") add(FIN_WAIT_1, nullptr, &g_fin);
  else if (c == "fin_wait_1_data") add(FIN_WAIT_1, nullptr, &g_data);
  else if (c == "fin_wait_2")   add(FIN_WAIT_2, nullptr, nullptr);
  else if (c == "closing_fin")  add(CLOSING, nullptr, &g_fin);
  else if (c == "last_ack_fin") add(LAST_ACK, nullptr, &g_fin);
  else if (c == "other_port")   add(ESTABLISHED, &g_data, nullptr, 25238);
  else if (c == "fresh_load") {
    // 2 requests in flight (one draining the index, one API call being received), 2 sockets the
    // browser opened ahead, 2 just-closed sockets whose FIN is not yet acked.
    add(ESTABLISHED, &g_data, &g_data);
    add(ESTABLISHED, nullptr, nullptr);
    add(ESTABLISHED, nullptr, nullptr);
    add(ESTABLISHED, nullptr, nullptr);
    add(LAST_ACK, nullptr, &g_fin);
    add(FIN_WAIT_1, nullptr, &g_fin);
  }
  printf("%s|%u\n", c.c_str(), (unsigned)platformTcpTxHoldingOnPort(80, idle, nidle));
  return 0;
}
"""

CASES = ("est_idle", "est_waiter", "est_data", "close_wait", "fin_wait_1_fin", "fin_wait_1_data",
         "fin_wait_2", "closing_fin", "last_ack_fin", "other_port", "fresh_load")
WEB_MAX_TX_CONNECTIONS = 4


class WebConnCapStates(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        src = SHIM.read_text(encoding="utf-8").replace("\r\n", "\n")
        a = src.find("struct PlatformTcpIdleConnection {")
        b = src.find("\n}\n", src.find("inline uint16_t platformTcpTxHoldingOnPort(", a))
        if a < 0 or b < 0:
            raise AssertionError("platformTcpTxHoldingOnPort() not found in platform_esp32.h")
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text(HARNESS.replace("@@SLICE@@", src[a:b + 3]), encoding="utf-8")
        exe = compile_cpp(cpp, Path(cls.tmp.name) / "t.exe")
        if exe is None:
            raise unittest.SkipTest("no host C++ compiler")
        cls.n = {}
        for case in CASES:
            out = subprocess.run([str(exe), case], capture_output=True, text=True, check=True).stdout.strip()
            name, held = out.split("|")
            cls.n[name] = int(held)
            print(out)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_established_counts_without_data_unless_a_queue_waiter(self):
        self.assertEqual((self.n["est_idle"], self.n["est_waiter"], self.n["est_data"]), (1, 0, 1))

    def test_closing_counts_on_a_zero_length_fin(self):
        self.assertEqual((self.n["fin_wait_1_fin"], self.n["closing_fin"], self.n["last_ack_fin"]), (1, 1, 1))
        self.assertEqual(self.n["fin_wait_1_data"], 1)

    def test_closing_without_segments_does_not_count(self):
        self.assertEqual((self.n["close_wait"], self.n["fin_wait_2"]), (0, 0))

    def test_other_ports_do_not_count(self):
        self.assertEqual(self.n["other_port"], 0)

    def test_modelled_fresh_load_with_2_in_flight_reaches_the_cap(self):
        held = self.n["fresh_load"]
        self.assertEqual(held, 6, "matches 09's 'WEB CONN CAP: 6 connections hold TX'")
        self.assertGreater(held, WEB_MAX_TX_CONNECTIONS)


if __name__ == "__main__":
    unittest.main(verbosity=2)
