#!/usr/bin/env python3
"""TASK-3025 / ADR-295: the REAL platformTcpAbortWhere() (src/libraries/Platform/src/platform_esp32.h) against a fake lwIP pcb list.

The heap-recovery abort of the /ws connections runs on the loop task while the async_tcp task serves the same sockets. What can
go wrong in this function, and what this test pins on the real code (the function text is cut out of the header, not re-implemented):

  - it must run under the lwIP core lock: LOCK_TCPIP_CORE() before the first pcb is read, UNLOCK_TCPIP_CORE() after the last
    tcp_abort() (a bare walk of tcp_active_pcbs from another task is a data race);
  - it must collect the matches first and abort them after the walk: tcp_abort() unlinks the pcb from tcp_active_pcbs and runs the error
    callback, so aborting inside the walk would read pcb->next of a freed pcb (the fake tcp_abort poisons next and counts any later read);
  - it aborts exactly the pcbs for which match(localPort, remoteIp, remotePort) is true (nothing else: not REST, not MQTT, not an IPv6 pcb);
  - at most 8 pcbs per call (the hit[] array), never more;
  - a null match function aborts nothing and does not even take the lock.
The second part (real wsAbortMatch() cut out of webSocketStuff.ino): only local port 80 and only a tracked remote ip+port.

NOT covered here (needs the real device): what AsyncTCP's error callback does with its AsyncClient when tcp_abort() fires it in this
locked context. ADR-295 argues no AsyncClient pointer is touched afterwards; the bench (heap CRITICAL with WS clients) is the proof.

Mutants (see the TASK-3025 notes): abort inside the walk, no lock, match ignored, unbounded hit[] are red.
Run: python tests/test_platform_tcp_abort_where.py
"""
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from test_sat_pwm_override import compile_cpp  # noqa: E402

ROOT = HERE.parent
HDR = ROOT / "src" / "libraries" / "Platform" / "src" / "platform_esp32.h"
WS = ROOT / "src" / "OTGW-firmware" / "webSocketStuff.ino"


def cut_abort_where(src: str) -> str:
    a = src.index("typedef bool (*PlatformTcpMatchFn)")
    m = re.compile(r"^inline uint8_t platformTcpAbortWhere\(.*?^\}\n", re.S | re.M).search(src, a)
    assert m, "platformTcpAbortWhere() not found in platform_esp32.h"
    return src[a:m.end()]


LWIP = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
struct ip_addr_t_ { uint32_t addr; int v4; };
struct tcp_pcb { tcp_pcb* next; uint16_t local_port; uint16_t remote_port; ip_addr_t_ remote_ip; int freed; };
static tcp_pcb* tcp_active_pcbs = nullptr;
static int g_locks = 0, g_unlocks = 0, g_locked = 0, g_abortsOutsideLock = 0, g_readsOfFreed = 0, g_aborts = 0;
#define LOCK_TCPIP_CORE() do { g_locks++; g_locked++; } while (0)
#define UNLOCK_TCPIP_CORE() do { g_unlocks++; g_locked--; } while (0)
#define IP_IS_V4(p) ((p)->v4)
#define ip_2_ip4(p) (p)
#define ip4_addr_get_u32(p) ((p)->addr)
static void tcp_abort(tcp_pcb* p) {
  g_aborts++;
  if (!g_locked) g_abortsOutsideLock++;
  // lwIP: the pcb leaves tcp_active_pcbs and is freed; any later read of p->next is a use after free
  if (tcp_active_pcbs == p) tcp_active_pcbs = p->next;
  else for (tcp_pcb* q = tcp_active_pcbs; q; q = q->next) if (q->next == p) { q->next = p->next; break; }
  p->freed = 1; p->next = (tcp_pcb*)(uintptr_t)1;   // poison: a walk that continues through it ends in the guard below
}
"""

MAIN = r"""
static tcp_pcb pool[12];
static int g_matchCalls = 0, g_matchMask = 0;
static bool matchPort80Ip1(uint16_t lp, uint32_t ip, uint16_t rp) { g_matchCalls++; return lp == 80 && ip == 0x01020304 && rp >= 50000; }
static bool matchAll(uint16_t, uint32_t, uint16_t) { g_matchCalls++; return true; }
static void build(int n) {   // n pcbs in a list, pool[i].next = pool[i+1]
  memset(pool, 0, sizeof(pool)); tcp_active_pcbs = n ? &pool[0] : nullptr;
  for (int i = 0; i < n; i++) { pool[i].next = (i + 1 < n) ? &pool[i + 1] : nullptr; pool[i].local_port = 80; pool[i].remote_ip = {0x01020304, 1}; pool[i].remote_port = 50000 + i; }
}
int main(int argc, char** argv) {
  const char* sc = argv[1];
  int ret = 0;
  if (!strcmp(sc, "select")) {            // 6 pcbs: [0] match, [1] other port, [2] other ip, [3] low remote port, [4] ipv6, [5] match
    build(6);
    pool[1].local_port = 1883; pool[2].remote_ip.addr = 0x05060708; pool[3].remote_port = 100; pool[4].remote_ip.v4 = 0;
    ret = platformTcpAbortWhere(matchPort80Ip1);
    std::printf("%d freed=%d%d%d%d%d%d locks=%d unlocks=%d outside=%d matchcalls=%d\n", ret, pool[0].freed, pool[1].freed, pool[2].freed,
                pool[3].freed, pool[4].freed, pool[5].freed, g_locks, g_unlocks, g_abortsOutsideLock, g_matchCalls);
  } else if (!strcmp(sc, "all_adjacent")) {   // 5 adjacent matching pcbs: aborting inside the walk would skip the rest
    build(5);
    ret = platformTcpAbortWhere(matchAll);
    std::printf("%d freed=%d%d%d%d%d aborts=%d locks=%d unlocks=%d outside=%d left=%d\n", ret, pool[0].freed, pool[1].freed, pool[2].freed,
                pool[3].freed, pool[4].freed, g_aborts, g_locks, g_unlocks, g_abortsOutsideLock, tcp_active_pcbs ? 1 : 0);
  } else if (!strcmp(sc, "cap8")) {       // 12 matching pcbs: at most 8 are aborted
    build(12);
    ret = platformTcpAbortWhere(matchAll);
    int fr = 0; for (int i = 0; i < 12; i++) fr += pool[i].freed;
    std::printf("%d freed=%d aborts=%d locks=%d unlocks=%d\n", ret, fr, g_aborts, g_locks, g_unlocks);
  } else if (!strcmp(sc, "nullmatch")) {
    build(3);
    ret = platformTcpAbortWhere(nullptr);
    std::printf("%d aborts=%d locks=%d\n", ret, g_aborts, g_locks);
  } else if (!strcmp(sc, "empty")) {
    build(0);
    ret = platformTcpAbortWhere(matchAll);
    std::printf("%d aborts=%d locks=%d unlocks=%d\n", ret, g_aborts, g_locks, g_unlocks);
  }
  return 0;
}
"""


class RealAbortWhere(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.d = Path(tempfile.mkdtemp(prefix="tcp_abort_"))
        cls.code = cut_abort_where(HDR.read_text(encoding="utf-8").replace("\r\n", "\n"))
        cpp = cls.d / "abort.cpp"
        cpp.write_text(LWIP + cls.code + MAIN, encoding="utf-8")
        cls.exe = compile_cpp(cpp, cls.d / "abort.exe")

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.d, ignore_errors=True)

    def run_case(self, name):
        if self.exe is None:
            self.skipTest("no host C++ compiler")
        r = subprocess.run([str(self.exe), name], capture_output=True, text=True, timeout=30)
        self.assertEqual(r.returncode, 0, f"crashed (use after free?) rc={r.returncode} {r.stderr[:200]}")
        return r.stdout.strip()

    def test_aborts_only_what_the_match_function_selects(self):
        out = self.run_case("select")
        # pcb 0 and 5 match; other port, other ip, low remote port and the IPv6 pcb stay
        self.assertEqual(out, "2 freed=100001 locks=1 unlocks=1 outside=0 matchcalls=5", out)

    def test_every_matched_pcb_is_aborted_even_when_they_are_adjacent(self):
        out = self.run_case("all_adjacent")
        self.assertEqual(out, "5 freed=11111 aborts=5 locks=1 unlocks=1 outside=0 left=0", out)

    def test_the_core_lock_is_taken_once_and_every_abort_runs_inside_it(self):
        for name in ("select", "all_adjacent"):
            out = self.run_case(name)
            self.assertIn("locks=1 unlocks=1", out)
            self.assertIn("outside=0", out)

    def test_at_most_eight_pcbs_per_call(self):
        self.assertEqual(self.run_case("cap8"), "8 freed=8 aborts=8 locks=1 unlocks=1")

    def test_a_null_match_function_aborts_nothing_and_takes_no_lock(self):
        self.assertEqual(self.run_case("nullmatch"), "0 aborts=0 locks=0")

    def test_an_empty_pcb_list_is_a_no_op_under_the_lock(self):
        self.assertEqual(self.run_case("empty"), "0 aborts=0 locks=1 unlocks=1")


WSMAIN = r"""
#include <cstdint>
#include <cstdio>
#include <cstring>
#define MAX_WEBSOCKET_CLIENTS 3
@ROW@
@CODE@
int main() {
  memset(wsTrack, 0, sizeof(wsTrack));
  wsTrack[0].id = 7; wsTrack[0].remoteIp = 0x0A0B0C0D; wsTrack[0].remotePort = 51000;
  wsTrack[1].id = 0; wsTrack[1].remoteIp = 0x01010101; wsTrack[1].remotePort = 52000;   // free slot: must not match
  std::printf("%d %d %d %d %d\n",
    (int)wsAbortMatch(80, 0x0A0B0C0D, 51000),    // tracked client: yes
    (int)wsAbortMatch(1883, 0x0A0B0C0D, 51000),  // same peer on another local port (MQTT/REST): no
    (int)wsAbortMatch(80, 0x0A0B0C0D, 51001),    // same host, other remote port: no
    (int)wsAbortMatch(80, 0x01010101, 52000),    // slot with id 0: no
    (int)wsAbortMatch(80, 0x09090909, 51000));   // other host: no
  return 0;
}
"""


class RealWsMatch(unittest.TestCase):
    def test_ws_abort_match_is_port_80_and_a_tracked_remote_only(self):
        src = WS.read_text(encoding="utf-8").replace("\r\n", "\n")
        slots = re.search(r"^#define WS_TRACK_SLOTS.*$", src, re.M).group(0)
        row = re.search(r"^struct WsTrackRow \{.*?\};$", src, re.M).group(0)
        arr = re.search(r"^static WsTrackRow wsTrack\[WS_TRACK_SLOTS\];$", src, re.M).group(0)
        a = src.index("static const uint16_t WS_LOCAL_PORT")
        m = re.compile(r"^static bool wsAbortMatch\(.*?^\}\n", re.S | re.M).search(src, a)
        code = src[a:m.end()]
        d = Path(tempfile.mkdtemp(prefix="ws_match_"))
        try:
            cpp = d / "wsm.cpp"
            cpp.write_text(WSMAIN.replace("@ROW@", slots + "\n" + row + "\n" + arr).replace("@CODE@", code), encoding="utf-8")
            exe = compile_cpp(cpp, d / "wsm.exe")
            if exe is None:
                self.skipTest("no host C++ compiler")
            out = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30, check=True).stdout.strip()
            self.assertEqual(out, "1 0 0 0 0")
        finally:
            shutil.rmtree(d, ignore_errors=True)

    def test_the_track_row_holds_plain_data_and_no_pointer_to_the_async_client(self):
        src = WS.read_text(encoding="utf-8")
        row = re.search(r"^struct WsTrackRow \{(.*?)\};$", src, re.M).group(1)
        self.assertNotIn("*", row, "a pointer in the track row could be used after tcp_abort() freed the client")
        self.assertNotIn("AsyncWebSocketClient", row)
        self.assertNotIn("AsyncClient", row)


class WiringIsStatic(unittest.TestCase):
    def setUp(self):
        self.ws = WS.read_text(encoding="utf-8").replace("\r\n", "\n")
        self.helper = (ROOT / "src" / "OTGW-firmware" / "helperStuff.ino").read_text(encoding="utf-8").replace("\r\n", "\n")

    def test_the_recovery_path_has_no_websocket_close(self):
        i = self.helper.index("void emergencyHeapRecovery()")
        body = self.helper[i:self.helper.index("\n}\n", i)]
        self.assertIn("doWebSocketDisconnectAll", body)
        self.assertNotIn("closeAll", body)
        j = self.ws.index("uint8_t doWebSocketDisconnectAll()")
        wbody = self.ws[j:self.ws.index("\n}\n", j)]
        self.assertIn("webSocketAbortAll()", wbody)
        self.assertNotIn("closeAll", wbody)

    def test_the_abort_walk_is_the_platform_helper_and_nothing_touches_the_client_afterwards(self):
        j = self.ws.index("uint8_t webSocketAbortAll()")
        body = self.ws[j:self.ws.index("\n}\n", j)]
        self.assertEqual(body.count("platformTcpAbortWhere(wsAbortMatch)"), 1)
        for forbidden in ("otLogWs", "client(", "->close", "closeAll", "AsyncWebSocketClient"):
            self.assertNotIn(forbidden, body, f"{forbidden} after the abort would touch a client tcp_abort() just freed")


class ByteOrderSourcePins(unittest.TestCase):
    """The IP byte-order assumption of ADR-295, pinned to the real library and core sources (skipped when they are not on this machine).

    The track row stores (uint32_t)client->remoteIP(); the walk compares it with ip4_addr_get_u32(pcb->remote_ip), the raw lwIP word.
    They are equal only while (1) AsyncClient::remoteIP() builds the IPAddress with from_ip_addr_t(&_pcb->remote_ip), (2) from_ip_addr_t
    copies the raw ip4.addr word into the IPv4 dword and (3) operator uint32_t returns that dword. A known address, 192.168.1.10, is the
    lwIP word 0x0A01A8C0 on this little-endian target; the row would hold the same word. Remote port: AsyncClient::getRemotePort() returns
    _pcb->remote_port unchanged, the field the walk reads.
    Paths: OTGW_ASYNCTCP_SRC (AsyncTCP.cpp), OTGW_ARDUINO_CORE (cores/esp32); defaults: .pio/libdeps/esp32-combo and D:/DevData/platformio.
    """

    @staticmethod
    def find(env, *defaults):
        import os
        for p in ([os.environ[env]] if os.environ.get(env) else []) + [str(d) for d in defaults]:
            if Path(p).exists():
                return Path(p)
        return None

    def test_asynctcp_remote_ip_and_port_come_straight_from_the_pcb(self):
        p = self.find("OTGW_ASYNCTCP_SRC", ROOT / ".pio/libdeps/esp32-combo/AsyncTCP/src/AsyncTCP.cpp",
                      ROOT / ".pio/libdeps/esp32/AsyncTCP/src/AsyncTCP.cpp")
        if p is None:
            self.skipTest("AsyncTCP.cpp not on this machine (set OTGW_ASYNCTCP_SRC)")
        src = p.read_text(encoding="utf-8", errors="replace")
        self.assertRegex(src, r"(?s)IPAddress AsyncClient::remoteIP\(\) const \{.*?ip\.from_ip_addr_t\(&\(_pcb->remote_ip\)\);")
        self.assertRegex(src, r"uint16_t AsyncClient::getRemotePort\(\) const \{\s*if \(!_pcb\) \{\s*return 0;\s*\}\s*return _pcb->remote_port;")

    def test_arduino_ipaddress_keeps_the_raw_lwip_word(self):
        core = self.find("OTGW_ARDUINO_CORE", "D:/DevData/platformio/packages/framework-arduinoespressif32/cores/esp32")
        if core is None or not (core / "IPAddress.cpp").exists():
            self.skipTest("Arduino core IPAddress.cpp not on this machine (set OTGW_ARDUINO_CORE)")
        cpp = (core / "IPAddress.cpp").read_text(encoding="utf-8", errors="replace")
        hdr = (core / "IPAddress.h").read_text(encoding="utf-8", errors="replace")
        i = cpp.index("IPAddress &IPAddress::from_ip_addr_t")
        body = cpp[i:cpp.index("\n}\n", i)]
        rhs = re.findall(r"_address\.dword\[IPADDRESS_V4_DWORD_INDEX\]\s*=\s*([^;]+);", body)
        self.assertTrue(rhs, "no IPv4 assignment found in from_ip_addr_t()")
        for r in rhs:   # every branch (with and without IPv6) must keep the raw lwIP word: no byte swap
            self.assertIn(r.strip(), ("addr->u_addr.ip4.addr", "addr->addr"), "from_ip_addr_t() converts the lwIP word: " + r)
        self.assertRegex(hdr, r"operator uint32_t\(\) const \{\s*return _type == IPv4 \? _address\.dword\[IPADDRESS_V4_DWORD_INDEX\] : 0;")

    def test_known_address_is_the_same_word_on_both_sides(self):
        # 192.168.1.10 as the four bytes lwIP stores (network order); the same four bytes are the dword IPAddress returns
        raw = bytes([192, 168, 1, 10])
        self.assertEqual(int.from_bytes(raw, "little"), 0x0A01A8C0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
