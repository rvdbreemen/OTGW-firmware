#!/usr/bin/env python3
"""TASK-1947: which port-80 connections the ADR-188 web connection cap counts, per TCP state.

    python tests/test_web_conn_cap_states.py

platformTcpTxHoldingOnPort() is sliced from src/libraries/Platform/src/platform_esp32.h and compiled
against a stub tcp_pcb/tcp_seg list. webConnCapExceeded() (restAPI.ino) refuses with 503 when
held - ws > WEB_MAX_TX_CONNECTIONS (4).

What it checks (ADR-188, amended by ADR-240 Proposed):
  - an ESTABLISHED connection counts even with empty queues, unless it is a waiter of the native file
    queue: an idle socket the browser opened ahead of a request (speculative preconnect) counts;
  - a connection in another state counts only while a queued segment carries payload. lwIP queues the
    FIN of a closing connection (FIN_WAIT_1, CLOSING, LAST_ACK) and the SYN-ACK of a half-open passive
    one (SYN_RCVD; tcp_listen_input, tcp_enqueue_flags) as zero-length segments: those do not count.
The breakdown names the reason, with closing-fin and syn-rcvd apart, for the bench.
ESPAsyncWebServer 3.11.2 answers with "Connection: close", so every request is a new socket that then
closes; a fresh load therefore leaves closing sockets behind for one round trip each.

The fresh_load row models 09's refusal "6 connections hold TX" (4 ESTABLISHED + 2 zero-length-only);
it is a model, not a measurement.
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
  else if (c == "syn_rcvd_synack") add(SYN_RCVD, nullptr, &g_fin);   // the SYN-ACK: also len 0
  else if (c == "close_wait_ctl") add(CLOSE_WAIT, nullptr, &g_fin);  // a zero-length segment, other state
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
  PlatformTcpTxBreakdown w;
  platformTcpTxBreakdownOnPort(80, idle, nidle, &w);
  printf("%s|%u|%u,%u,%u,%u,%u,%u|%u\n", c.c_str(), (unsigned)platformTcpTxHoldingOnPort(80, idle, nidle),
         w.estIdle, w.estData, w.closingData, w.closingFin, w.synRcvd, w.otherEmpty, (unsigned)w.payloadBytes);
  return 0;
}
"""

CASES = ("est_idle", "est_waiter", "est_data", "close_wait", "fin_wait_1_fin", "fin_wait_1_data",
         "fin_wait_2", "closing_fin", "last_ack_fin", "syn_rcvd_synack", "close_wait_ctl", "other_port",
         "fresh_load")
WEB_MAX_TX_CONNECTIONS = 4


class WebConnCapStates(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        src = SHIM.read_text(encoding="utf-8").replace("\r\n", "\n")
        a = src.find("struct PlatformTcpIdleConnection {")
        b = src.find("\n}\n", src.find("inline void platformTcpTxBreakdownOnPort(", a))
        if a < 0 or b < 0:
            raise AssertionError("platformTcpTxHoldingOnPort() not found in platform_esp32.h")
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text(HARNESS.replace("@@SLICE@@", src[a:b + 3]), encoding="utf-8")
        exe = compile_cpp(cpp, Path(cls.tmp.name) / "t.exe")
        if exe is None:
            raise unittest.SkipTest("no host C++ compiler")
        cls.n, cls.why, cls.bytes = {}, {}, {}
        for case in CASES:
            out = subprocess.run([str(exe), case], capture_output=True, text=True, check=True).stdout.strip()
            name, held, why, payload = out.split("|")
            cls.n[name] = int(held)
            cls.why[name] = tuple(int(x) for x in why.split(","))
            cls.bytes[name] = int(payload)
            print(out)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_established_counts_without_data_unless_a_queue_waiter(self):
        self.assertEqual((self.n["est_idle"], self.n["est_waiter"], self.n["est_data"]), (1, 0, 1))

    def test_not_established_with_only_zero_length_segments_does_not_count(self):
        # ADR-240 (Proposed, amends ADR-188): no payload queued, no request received. A closing connection
        # with only its FIN and a half-open SYN_RCVD one with only its SYN-ACK count 0.
        self.assertEqual((self.n["fin_wait_1_fin"], self.n["closing_fin"], self.n["last_ack_fin"]), (0, 0, 0))
        self.assertEqual((self.n["syn_rcvd_synack"], self.n["close_wait_ctl"]), (0, 0))

    def test_closing_with_payload_still_counts(self):
        self.assertEqual(self.n["fin_wait_1_data"], 1)

    def test_closing_without_segments_does_not_count(self):
        self.assertEqual((self.n["close_wait"], self.n["fin_wait_2"]), (0, 0))

    def test_other_ports_do_not_count(self):
        self.assertEqual(self.n["other_port"], 0)

    def test_modelled_fresh_load_with_2_in_flight_stays_under_the_cap(self):
        # the model of 09's refusal ("6 connections hold TX" = 4 ESTABLISHED + 2 zero-length-only) counts 4
        held = self.n["fresh_load"]
        self.assertEqual(held, 4)
        self.assertLessEqual(held, WEB_MAX_TX_CONNECTIONS)

    def test_breakdown_adds_up_to_the_count_plus_the_uncounted_zero_length_only(self):
        for case in CASES:
            idle, data, cdata, cfin, syn, other = self.why[case]
            self.assertEqual(idle + data + cdata, self.n[case], case)

    def test_breakdown_names_the_reason(self):
        # (est-idle, est-data, closing-data, closing-fin, syn-rcvd, other)
        self.assertEqual(self.why["est_idle"], (1, 0, 0, 0, 0, 0))
        self.assertEqual(self.why["est_data"], (0, 1, 0, 0, 0, 0))
        self.assertEqual(self.why["fin_wait_1_data"], (0, 0, 1, 0, 0, 0))
        self.assertEqual(self.why["last_ack_fin"], (0, 0, 0, 1, 0, 0))
        self.assertEqual(self.why["closing_fin"], (0, 0, 0, 1, 0, 0))
        self.assertEqual(self.why["syn_rcvd_synack"], (0, 0, 0, 0, 1, 0))
        self.assertEqual(self.why["close_wait_ctl"], (0, 0, 0, 0, 0, 1))
        self.assertEqual(self.why["fresh_load"], (3, 1, 0, 2, 0, 0))
        self.assertEqual(self.bytes["fresh_load"], 2 * 1436)


    def test_refusal_line_is_wired_to_the_breakdown(self):
        rest = (REPO / "src/OTGW-firmware/restAPI.ino").read_text(encoding="utf-8")
        compat = (REPO / "src/OTGW-firmware/webServerCompat.h").read_text(encoding="utf-8")
        a = rest.find("static bool webConnCapExceeded(")
        body = rest[a:rest.find("\n}\n", a)].replace("\r\n", "\n")
        self.assertIn("WebQueuedFileResponse::txBreakdown(&why);", body)
        self.assertIn("closing-fin %u syn-rcvd %u other %u", body)
        self.assertIn("why.closingFin, why.synRcvd, why.otherEmpty", body)
        self.assertIn("platformTcpTxBreakdownOnPort(WEB_HTTP_PORT, idle, count, out);", compat)


if __name__ == "__main__":
    unittest.main(verbosity=2)
