#!/usr/bin/env python3
"""TASK-3017 (AC3 run, 2026-11-21): a /ws client stuck outside WS_CONNECTED with an open TCP connection is closed.

In the 3 h run on .61 (beta.417+56dd422) the client left WS_CONNECTED at 22:00:33 (otLogWs.count() = 0 while the TCP
connection stayed: hd_tcp_active_pcbs 3 until the tool closed at 00:55). count() then skips it, nothing is sent to it,
and cleanupClients() never removes it, so the reader waited 174 min.

The fix gives every accepted /ws connection AsyncTCP's rx timeout (wsTrackConnect() in webSocketStuff.ino). The test
cuts the real block between the '// --- wsTrack' markers and compiles it against stubs. The stub AsyncClient applies
AsyncTCP's own rule from AsyncClient::_poll (AsyncTCP.cpp:1106-1108: rx timeout set and nothing received for that long
-> _close()), where an ACK of our data also counts as received (AsyncClient::_sent, :1049). That rule is the library's,
modelled here, not the firmware's; the firmware code under test is the wiring (which timeout, on which connection) and
the device/info rows.

Case B (09's forced reproduction): the client sends a WS close right after the handshake and then neither reads nor
closes TCP: the library sets WS_DISCONNECTING, nothing more is sent or acknowledged. Red on the code before (no
wsTrack block, no rx timeout: never closed), green on the fix (closed within WS_RX_TIMEOUT_S).
Run: python tests/test_ws_rx_timeout_reap.py
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
#include <functional>
#define MAX_WEBSOCKET_CLIENTS 3
#define WEB_TCP_SEND_CAP 2872
enum AwsClientStatus { WS_DISCONNECTED = 0, WS_CONNECTED = 1, WS_DISCONNECTING = 2 };
static uint32_t g_now = 0;
// AsyncTCP's rx timeout rule (AsyncClient::_poll, AsyncTCP.cpp:1106-1108), with _sent() resetting the timer (:1049).
// The ack timeout rule too (AsyncClient::_poll, AsyncTCP.cpp:1094-1102): data sent after the last ACK and older than
// CONFIG_ASYNC_TCP_MAX_ACK_TIME (5000 ms, platformio.ini:74) calls the timeout callback. Every successful send()
// restamps the last transmit (AsyncTCP.cpp:959-964), so towards a dead peer the timer only runs once the send queue
// is full: unacked bytes count against the pcb's send buffer (TCP_SND_BUF 5744, or the ADR-216 cap).
struct tcp_pcb { uint16_t sndCap = 0; };
static void platformTcpCapSendBuffer(tcp_pcb* p, uint16_t cap) { if (p) p->sndCap = cap; }
struct AsyncClient {
  uint32_t rxTimeoutS = 0, lastRx = 0, lastTx = 0, lastAck = 0, unacked = 0;
  bool closed = false, aborted = false;
  tcp_pcb pcbObj;
  std::function<void(void*, AsyncClient*, uint32_t)> timeoutCb;
  void setRxTimeout(uint32_t s) { rxTimeoutS = s; }
  tcp_pcb* pcb() { return &pcbObj; }
  void onTimeout(std::function<void(void*, AsyncClient*, uint32_t)> cb, void*) { timeoutCb = cb; }
  void abort() { aborted = true; closed = true; }
  void received() { lastRx = g_now; lastAck = g_now; }   // data from the peer, or an ACK of our data
  void sent() { lastTx = g_now; }
  bool sendFrame(uint32_t len) {                       // one /ws frame; false when the send queue has no room
    const uint32_t cap = pcbObj.sndCap ? pcbObj.sndCap : 5744;
    if (unacked + len > cap) return false;
    unacked += len; lastTx = g_now; return true;
  }
  void poll() {
    if (closed) return;
    if (lastTx > lastAck && (g_now - lastTx) >= 5000UL) { if (timeoutCb) timeoutCb(nullptr, this, g_now - lastTx); return; }
    if (rxTimeoutS && (g_now - lastRx) >= rxTimeoutS * 1000UL) closed = true;
  }
};
struct { struct { uint32_t iWsAckAbortCount = 0; } heapdiag; } state;
struct IPAddress { uint32_t v = 0; operator uint32_t() const { return v; } };
// lwIP stand-in for platformTcpAbortWhere() (platform_esp32.h): the active pcbs and which ones were aborted
struct FakePcb { uint16_t localPort; uint32_t remoteIp; uint16_t remotePort; bool aborted; };
static FakePcb g_pcbs[4]; static uint8_t g_pcbCount = 0;
typedef bool (*PlatformTcpMatchFn)(uint16_t, uint32_t, uint16_t);
static uint8_t platformTcpAbortWhere(PlatformTcpMatchFn match) {
  uint8_t n = 0;
  for (uint8_t i = 0; i < g_pcbCount; i++)
    if (!g_pcbs[i].aborted && match(g_pcbs[i].localPort, g_pcbs[i].remoteIp, g_pcbs[i].remotePort)) { g_pcbs[i].aborted = true; n++; }
  return n;
}
struct AsyncWebSocketClient {
  uint32_t _id; AwsClientStatus _st = WS_CONNECTED; AsyncClient tcp; uint16_t q = 0;
  uint32_t ip = 0x2458A8C0; uint16_t port = 50000;
  IPAddress remoteIP() const { IPAddress a; a.v = ip; return a; }
  uint16_t remotePort() const { return port; }
  uint32_t id() const { return _id; }
  AwsClientStatus status() const { return _st; }
  AsyncClient* client() { return &tcp; }
  size_t queueLen() const { return q; }
};
static AsyncWebSocketClient g_clients[2];
struct AsyncWebSocket {
  // as the library: only a client in WS_CONNECTED is returned (AsyncWebSocket.cpp:1043-1053)
  AsyncWebSocketClient* client(uint32_t id) { for (auto& c : g_clients) if (c._id == id && c._st == WS_CONNECTED) return &c; return nullptr; }
};
static AsyncWebSocket otLogWs;
struct WsClientSnap { uint32_t id; const char* status; uint16_t queueLen; uint32_t statusAgeS; uint32_t ageS; };
@CODE@
int main(int argc, char** argv) {
  const char* sc = argv[1];
  AsyncWebSocketClient& c = g_clients[0];
  c._id = 2; g_now = 1000; c.tcp.lastRx = g_now;
  wsTrackConnect(&c, g_now);
  if (!strcmp(sc, "caseB")) {          // close frame received, then nothing read, nothing acknowledged
    c._st = WS_DISCONNECTING;
    long closedAt = -1;
    for (uint32_t t = 1; t <= 600; t++) { g_now = 1000 + t * 1000; c.tcp.poll(); if (c.tcp.closed) { closedAt = t; break; } }
    WsClientSnap s[4];
    g_now = 1000 + 30 * 1000;
    printf("closed_after_s=%ld\n", closedAt);
  } else if (!strcmp(sc, "caseC")) {   // half-open peer: frames go out every 250 ms, no ACK ever comes back
    long abortedAt = -1;
    for (uint32_t ms = 250; ms <= 120000; ms += 250) {
      g_now = 1000 + ms; c.tcp.sendFrame(60); c.tcp.poll();
      if (c.tcp.aborted) { abortedAt = (long)ms; break; }
    }
    printf("aborted_after_ms=%ld aborts=%u cap=%u\n", abortedAt, state.heapdiag.iWsAckAbortCount, c.tcp.pcbObj.sndCap);
  } else if (!strcmp(sc, "recovery")) {  // ADR-295: the heap recovery aborts the tracked /ws pcb, nothing else
    c._st = WS_DISCONNECTING;               // the AC3 state: count() would not see it
    g_pcbs[0] = {80, 0x2458A8C0, 50000, false};    // the /ws connection
    g_pcbs[1] = {80, 0x2458A8C0, 50001, false};    // a REST request from the same browser
    g_pcbs[2] = {54321, 0x2458A8C0, 1883, false};  // the MQTT connection to the broker
    g_pcbs[3] = {80, 0x0B58A8C0, 50000, false};    // another host, same remote port
    g_pcbCount = 4;
    const uint8_t tracked = webSocketTrackedCount();
    const uint8_t n = webSocketAbortAll();
    printf("tracked=%u aborted=%u ws=%d rest=%d mqtt=%d other=%d\n", tracked, n, g_pcbs[0].aborted, g_pcbs[1].aborted,
           g_pcbs[2].aborted, g_pcbs[3].aborted);
  } else if (!strcmp(sc, "healthy")) { // connected, the 30 s keepalive is acknowledged every time
    bool closed = false;
    for (uint32_t t = 1; t <= 3600; t++) { g_now = 1000 + t * 1000; if (t % 30 == 0) { c.tcp.sent(); g_now += 50; c.tcp.received(); } c.tcp.poll(); closed |= c.tcp.closed; }
    printf("closed=%d\n", closed ? 1 : 0);
  } else if (!strcmp(sc, "snapshot")) {
    WsClientSnap s[4];
    g_now = 11000;
    uint8_t n = webSocketSnapshotClients(s, 4, g_now);
    c._st = WS_DISCONNECTING; c.q = 32;
    g_now = 21000; webSocketSnapshotClients(s, 4, g_now);    // first read that sees DISCONNECTING
    g_now = 41000; n = webSocketSnapshotClients(s, 4, g_now);
    printf("n=%u id=%u status=%s q=%u status_age=%u age=%u\n", n, n ? s[0].id : 0, n ? s[0].status : "-",
           n ? s[0].queueLen : 0, n ? s[0].statusAgeS : 0, n ? s[0].ageS : 0);
  } else if (!strcmp(sc, "disconnect")) {
    uint32_t life = 0, lastAge = 0; uint8_t lastSt = 0;
    g_now = 61000;
    bool hit = wsTrackDisconnect(2, g_now, &life, &lastSt, &lastAge);
    bool again = wsTrackDisconnect(2, g_now, &life, &lastSt, &lastAge);
    printf("hit=%d again=%d life=%u\n", hit ? 1 : 0, again ? 1 : 0, life);
  }
  printf("rx_timeout_s=%u\n", c.tcp.rxTimeoutS);
  return 0;
}
"""


def build_cpp(src):
    src = src.replace("\r\n", "\n")
    a = src.index("// --- wsTrack begin")
    b = src.index("// --- wsTrack end", a)
    return PRE.replace("@CODE@", src[a:b])


class WsRxTimeoutReap(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.d = Path(tempfile.mkdtemp(prefix="ws_rx_reap_"))
        cpp = cls.d / "ws.cpp"
        cpp.write_text(build_cpp(WS_INO.read_text(encoding="utf-8")), encoding="utf-8")
        cls.exe = compile_cpp(cpp, cls.d / "ws.exe")

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.d, ignore_errors=True)

    def run_sc(self, sc):
        if self.exe is None:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe), sc], capture_output=True, text=True, check=True).stdout
        return dict(kv.split("=", 1) for line in out.split("\n") for kv in line.split() if "=" in kv)

    def test_case_b_stuck_disconnecting_is_closed(self):
        r = self.run_sc("caseB")
        closed = int(r["closed_after_s"])
        self.assertGreater(closed, 0, "a client stuck outside WS_CONNECTED with an open socket is never closed")
        self.assertLessEqual(closed, 75)

    def test_case_c_half_open_is_aborted_on_the_ack_timeout(self):
        # ADR-189 applied to /ws: a RST within ~5 s instead of the library's graceful close; ADR-216 cap on the pcb
        r = self.run_sc("caseC")
        at = int(r["aborted_after_ms"])
        self.assertGreater(at, 0, "a half-open /ws client is never aborted (graceful close keeps its queue)")
        # 2872 B of 60 B frames at 4/s fill the capped queue in ~12 s, then the 5 s ack timeout runs
        self.assertLessEqual(at, 20000)
        self.assertEqual(r["aborts"], "1")
        self.assertEqual(r["cap"], "2872")

    def test_healthy_client_with_keepalive_acks_stays(self):
        r = self.run_sc("healthy")
        self.assertEqual(r["closed"], "0")
        # more than two 30 s keepalive intervals, so one late ACK does not close a healthy client
        self.assertGreater(int(r["rx_timeout_s"]), 60)

    def test_device_info_row(self):
        r = self.run_sc("snapshot")
        # the AC3 state: in the list, outside WS_CONNECTED; client(id) does not return it, the row still shows it
        self.assertEqual((r["n"], r["id"], r["status"], r["q"]), ("1", "2", "not_connected", "65535"))
        self.assertEqual(r["status_age"], "20")
        self.assertEqual(r["age"], "40")

    def test_recovery_aborts_only_the_ws_connection(self):
        # TASK-3025 / ADR-295: also a client outside WS_CONNECTED (tracked, count() does not see it)
        r = self.run_sc("recovery")
        self.assertEqual((r["tracked"], r["aborted"], r["ws"]), ("1", "1", "1"))
        self.assertEqual((r["rest"], r["mqtt"], r["other"]), ("0", "0", "0"))

    def test_disconnect_frees_the_row(self):
        r = self.run_sc("disconnect")
        self.assertEqual((r["hit"], r["again"], r["life"]), ("1", "0", "60"))


class Wiring(unittest.TestCase):
    def setUp(self):
        self.ws = WS_INO.read_text(encoding="utf-8").replace("\r\n", "\n")

    def test_connect_and_disconnect_events_use_the_table(self):
        i = self.ws.index("void webSocketEvent(AsyncWebSocket *server")
        body = self.ws[i:self.ws.index("\n}\n", i)]
        self.assertIn("wsTrackConnect(client, millis());", body)
        self.assertIn("wsTrackDisconnect(client->id(), millis()", body)
        self.assertIn("state.heapdiag.iWsDisconnects++;", body)
        self.assertIn("state.heapdiag.iWsAckAbortCount++;", self.ws)   # separate from the HTTP iWebAckAbortCount

    def test_heap_recovery_aborts_instead_of_closing(self):
        i = self.ws.index("uint8_t doWebSocketDisconnectAll() {")
        body = self.ws[i:self.ws.index("\n}\n", i)]
        self.assertNotIn("closeAll", body)
        self.assertIn("webSocketAbortAll()", body)
        helper = (FW / "helperStuff.ino").read_text(encoding="utf-8")
        self.assertIn("if (webSocketTrackedCount() > 0 && doWebSocketDisconnectAll() > 0) {", helper)

    def test_device_info_shows_the_clients(self):
        rest = (FW / "restAPI.ino").read_text(encoding="utf-8")
        self.assertIn('je.beginArray(F("ws_clients"));', rest)
        self.assertIn('je.field(F("hd_ws_disconnects"),', rest)
        self.assertIn('je.field(F("hd_ws_ack_abort"),', rest)
        self.assertIn("snap->wsClientCount   = webSocketSnapshotClients(snap->wsClients, 4, millis());", rest)


if __name__ == "__main__":
    unittest.main(verbosity=2)
