#!/usr/bin/env python3
"""ADR-217 / TASK-1695 part B: host test for the Classic port 25238 inbound bridge.

The real handlePICSerial() is cut out of src/OTGW-firmware/OTGW-Core.ino (from "void handlePICSerial()" up to
"}// END of handlePICSerial") and satBlocksExternalCommand() out of OTGW-firmware.h; both are compiled with a host
C++ compiler, nothing is re-implemented. The TCP stream, the PIC transmit queue, the command queue and the SAT
predicate are stubs. OTGW_CORE_INO points at another copy of OTGW-Core.ino and OTGW_CORE_REV at a git revision
(for example HEAD, which holds the code before ADR-217) for old-vs-fix runs. Without a compiler the tests are skipped.

Old behaviour (before ADR-217): every byte went to the PIC at once, whatever SAT was doing, so the cases that expect a
drop or a hold while SAT is enabled fail; the SAT-off cases pass on both.

Run: python tests/test_bridge25238_sat_filter.py
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

CORE_INO = Path(os.environ.get("OTGW_CORE_INO", REPO / "src" / "OTGW-firmware" / "OTGW-Core.ino"))
CORE_REV = os.environ.get("OTGW_CORE_REV")
HEADER = REPO / "src" / "OTGW-firmware" / "OTGW-firmware.h"
CORE_H = REPO / "src" / "OTGW-firmware" / "OTGW-Core.h"


def core_text() -> str:
    if CORE_REV:
        return subprocess.run(["git", "show", f"{CORE_REV}:src/OTGW-firmware/OTGW-Core.ino"], cwd=REPO, check=True,
                              capture_output=True, text=True, encoding="utf-8").stdout
    return CORE_INO.read_text(encoding="utf-8")


def cut(text: str, start: str, end: str, include_end: bool = False) -> str:
    i = text.index(start)
    j = text.index(end, i)
    return text[i:j + (len(end) if include_end else 0)]


STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstddef>
#include <deque>
#include <string>
#define PSTR(x) x
#define F(x) x
#define snprintf_P snprintf
#define strcmp_P strcmp
#define OTDebugTln(...)
#define OTDebugTf(...)
#define HAS_PIC 1
#define MAX_BUFFER_WRITE @MAXBUF@
#ifdef _MSC_VER
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif
#define strcasecmp_P strcasecmp
struct OT_cmd_t { char cmd[20]; int cmdlen; };
static OT_cmd_t cmdqueue[8];
static int cmdQueueSize = 0;
static void removeFromCmdQueue(int i) { for (int k = i; k + 1 < cmdQueueSize; k++) cmdqueue[k] = cmdqueue[k + 1]; cmdQueueSize--; }
struct CmdQueueLock { bool locked = true; };
struct DebugS { bool bOTGWSimulation = false; };
struct StateS { DebugS debug; };
static StateS state;
struct MqttS { bool bLegacyPort25238Enabled = true; };
struct SettingsS { MqttS mqtt; };
static SettingsS settings;
static char cMsg[128];
static bool g_satOwns = false;
static bool satOwnsControlSetpoint() { return g_satOwns; }
static bool isOTDirectEnabled() { return false; }
static bool isPICEnabled() { return true; }
static uint32_t lastSer2netCmdMs = 0;
static int g_readoutTriggers = 0, g_refused = 0, g_dropped = 0, g_resets = 0, g_ws = 0;
static volatile uint32_t g_bridgeConnectGen = 0;   // bumped by the telnet onConnect callback in the firmware
static bool g_txFail = false;
static void triggerReadoutForSettingsCmd(const char*) { g_readoutTriggers++; }
static void sendEventToWebSocket(char, const char*) { g_ws++; }
static void sendEventToWebSocket_P(char, const char*) { g_ws++; }
static void resetOTGW() { g_resets++; }
static void enterPSMode(const char*, const char*, bool) {}
static void leavePSMode(const char*, const char*) {}
static uint32_t millis() { return 1; }
static void otgwStreamRxNote() {}
static std::string g_pic;            // every byte that reached the PIC transmit queue, in order
static bool enqueuePICTx(const uint8_t* b, size_t n) { if (g_txFail) return false; g_pic.append((const char*)b, n); return true; }
struct Stream {
  std::deque<uint8_t> rx; bool connected = true;
  int available() { return (int)rx.size(); }
  int read() { if (rx.empty()) return -1; int v = rx.front(); rx.pop_front(); return v; }
  bool isConnected() const { return connected; }
};
static Stream OTGWstream;
@HEADER_FN@
static void satNoteExternalRefused(const char*, int) { g_refused++; }
static void satNoteBridgeDropped(const char*) { g_dropped++; }
@BODY@
static std::string unhex(const std::string& h) {
  std::string o; for (size_t i = 0; i + 1 < h.size(); i += 2) o += (char)strtol(h.substr(i, 2).c_str(), nullptr, 16); return o;
}
int main() {
  std::string line;
  static char buf[8192];
  while (fgets(buf, sizeof buf, stdin)) {
    line = buf; while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
    if (line.rfind("SAT ", 0) == 0) g_satOwns = line[4] == '1';
    else if (line.rfind("SIM ", 0) == 0) state.debug.bOTGWSimulation = line[4] == '1';
    else if (line.rfind("FEED ", 0) == 0) { std::string d = unhex(line.substr(5)); for (unsigned char c : d) OTGWstream.rx.push_back(c); }
    else if (line.rfind("QUEUE ", 0) == 0) { std::string c = line.substr(6); strcpy(cmdqueue[cmdQueueSize].cmd, c.c_str()); cmdqueue[cmdQueueSize].cmdlen = (int)c.size(); cmdQueueSize++; }
    else if (line.rfind("TXFAIL ", 0) == 0) g_txFail = line[7] == '1';
    else if (line == "RECONNECT") g_bridgeConnectGen++;
    else if (line == "RUN") handlePICSerial();
    else if (line == "DISCONNECT") { OTGWstream.connected = false; handlePICSerial(); OTGWstream.connected = true; }
    else if (line == "PRINT") {
      printf("pic="); for (unsigned char c : g_pic) printf("%02x", c);
      printf("\nqueue="); for (int i = 0; i < cmdQueueSize; i++) printf("%s;", cmdqueue[i].cmd);
      printf("\nrefused=%d dropped=%d triggers=%d ws=%d resets=%d\n", g_refused, g_dropped, g_readoutTriggers, g_ws, g_resets);
    }
  }
  return 0;
}
"""


def build_source() -> str:
    core = core_text()
    start = "static inline bool bridgeByteIsText" if "static inline bool bridgeByteIsText" in core else "void handlePICSerial()"
    body = cut(core, start, "}// END of handlePICSerial", include_end=True)
    hdr = HEADER.read_text(encoding="utf-8")
    fn = cut(hdr, "inline bool satBlocksExternalCommand", "\n}\n", include_end=True)
    return STUBS.replace("@HEADER_FN@", fn).replace("@BODY@", body).replace("@MAXBUF@", str(MAXBUF))


MAXBUF = int(re.search(r"#define\s+MAX_BUFFER_WRITE\s+(\d+)", CORE_H.read_text(encoding="utf-8")).group(1))
EXE = None
WORK = None


def setUpModule():
    global EXE, WORK
    WORK = tempfile.TemporaryDirectory()
    cpp = Path(WORK.name) / "bridge.cpp"
    cpp.write_text(build_source(), encoding="utf-8")
    EXE = compile_cpp(cpp, Path(WORK.name) / "bridge.exe")


def tearDownModule():
    WORK.cleanup()


def run(*steps: str) -> dict:
    r = subprocess.run([str(EXE)], input="\n".join(list(steps) + ["PRINT"]) + "\n", capture_output=True, text=True, timeout=30)
    assert r.returncode == 0, r.stderr
    out = {}
    for ln in r.stdout.splitlines():
        for part in ln.split(" "):
            k, _, v = part.partition("=")
            out[k] = v
    return out


def feed(b: bytes) -> list:
    return [f"FEED {b.hex()}", "RUN"]


class Bridge25238SatFilter(unittest.TestCase):
    def setUp(self):
        if EXE is None:
            self.skipTest("no host C++ compiler")

    def test_1_sat_off_is_byte_transparent(self):
        blob = bytes([0x00, 0x0D, 0x01, 0xFF, 0x0D, 0x0A, 0x80]) + bytes(range(256))
        text = b"CH=0\r"
        out = run("SAT 0", *feed(text), *feed(blob))
        self.assertEqual(out["pic"], (text + blob).hex())

    def test_1b_sat_off_forwards_without_waiting_for_cr(self):
        self.assertEqual(run("SAT 0", *feed(b"CS=6"))["pic"], b"CS=6".hex())

    def test_2_sat_on_blocked_dropped_allowed_forwarded(self):
        out = run("SAT 1", *feed(b"CH=0\r"))
        self.assertEqual(out["pic"], "", "CH=0 must not reach the PIC while SAT is enabled")
        self.assertEqual(out["refused"], "1")
        out = run("SAT 1", *feed(b"PR=A\r"), *feed(b"HW=1\r"))
        self.assertEqual(out["pic"], b"PR=A\rHW=1\r".hex())
        for cmd in (b"CS=60\r", b"cs=60\r", b"MM=50\r", b"C2=1\r", b"H2=1\r", b"TT=20\r", b"TC=20\r"):
            self.assertEqual(run("SAT 1", *feed(cmd))["pic"], "", cmd)
        for cmd in (b"HW=1\r", b"SW=60\r", b"SH=60\r"):
            self.assertEqual(run("SAT 1", *feed(cmd))["pic"], cmd.hex(), cmd)

    def test_2b_sat_on_holds_the_line_until_cr(self):
        self.assertEqual(run("SAT 1", *feed(b"PR=A"))["pic"], "")
        self.assertEqual(run("SAT 1", *feed(b"PR=A"), *feed(b"\r"))["pic"], b"PR=A\r".hex())

    def test_3_sat_on_non_text_line_dropped_whole(self):
        for line in (b"PR=\x01A\r", b"\x00\r", b"HW=1\xff\r", b"\x1b[A\r"):
            out = run("SAT 1", *feed(line))
            self.assertEqual(out["pic"], "", line)
            self.assertEqual(out["dropped"], "1", line)
        blob = bytes([0x55, 0xAA, 0x0D, 0x01, 0x02, 0x0D, 0xFF, 0x0D])   # a binary stream with CR bytes inside
        self.assertEqual(run("SAT 1", *feed(blob))["pic"], "")

    def test_4_sat_on_overlong_line_dropped_whole_next_line_forwarded(self):
        out = run("SAT 1", *feed(b"A" * (MAXBUF + 72) + b"\r"), *feed(b"HW=1\r"))
        self.assertEqual(out["pic"], b"HW=1\r".hex())
        self.assertEqual(out["dropped"], "1")
        ok = b"X" * (MAXBUF - 1) + b"\r"   # the longest line that fits is still forwarded
        self.assertEqual(run("SAT 1", *feed(ok))["pic"], ok.hex())

    def test_5_dropped_line_does_not_run_the_adr059_override(self):
        out = run("SAT 1", "QUEUE CH=1", *feed(b"CH=0\r"))
        self.assertEqual(out["queue"], "CH=1;", "a dropped CH=0 must not remove SAT's queued CH=1")
        self.assertEqual(out["triggers"], "0")
        out = run("SAT 1", "QUEUE PR=A", *feed(b"PR=A\r"))   # an allowed line still overrides the queue
        self.assertEqual(out["queue"], "")
        self.assertEqual(out["pic"], b"PR=A\r".hex())

    def test_6_mode_switch_takes_effect_at_the_next_line(self):
        # SAT turns on in the middle of a transparent line: that line is already on its way, the next one is filtered
        out = run("SAT 0", *feed(b"CH="), "SAT 1", *feed(b"0\r"), *feed(b"CH=0\r"))
        self.assertEqual(out["pic"], b"CH=0\r".hex())
        # SAT turns off in the middle of a filtered line: the line is judged whole, the next one is transparent
        out = run("SAT 1", *feed(b"CH="), "SAT 0", *feed(b"0\r"), *feed(b"CH=0\r"))
        self.assertEqual(out["pic"], b"CH=0\r".hex())
        self.assertEqual(out["refused"], "1")

    def test_7_client_disconnect_forgets_a_held_partial_line(self):
        out = run("SAT 1", *feed(b"HW=1"), "DISCONNECT", *feed(b"PR=A\r"))
        self.assertEqual(out["pic"], b"PR=A\r".hex())

    def test_8_simulation_mode_sends_nothing_in_either_mode(self):
        for sat in ("SAT 0", "SAT 1"):
            self.assertEqual(run(sat, "SIM 1", *feed(b"HW=1\r"))["pic"], "")

    def test_9_crlf_lines_keep_the_mode_sampling_on_the_cr_line(self):
        # pyotgw sends CMD=VAL CR LF. The LF is forwarded at once and never starts a line or samples the mode.
        out = run("SAT 0", *feed(b"HW=1\r\n"), "SAT 1", *feed(b"CH=0\r\n"))
        self.assertEqual(out["pic"], (b"HW=1\r\n" + b"\n").hex(), "CH=0 must be refused, both LFs forwarded")
        self.assertEqual(out["refused"], "1")
        out = run("SAT 1", *feed(b"HW=1\r\n"), "SAT 0", *feed(b"CH=0\r\n"))
        self.assertEqual(out["pic"], b"HW=1\r\nCH=0\r\n".hex(), "SAT off: the next CR LF command passes")
        self.assertEqual(out["refused"], "0")

    def test_10_binary_latch_while_sat_is_on(self):
        blob = b"\x01\x02\r0A\r\x00\x03\rGW=R\r"   # printable fragments between CR bytes
        out = run("SAT 1", *feed(blob))
        self.assertEqual(out["pic"], "")
        self.assertEqual(out["resets"], "0")
        self.assertEqual(out["dropped"], "1", "counted once")
        out = run("SAT 1", *feed(blob), "RECONNECT", *feed(b"HW=1\r"))   # the latch ends with the connection
        self.assertEqual(out["pic"], b"HW=1\r".hex())

    def test_11_transparent_latch_after_binary_data_with_sat_off(self):
        out = run("SAT 0", *feed(b"\x01\x02"), "SAT 1", *feed(b"\r\x03CH=0\r"))
        self.assertEqual(out["pic"], (b"\x01\x02" + b"\r\x03CH=0\r").hex(), "an ongoing upgrade is not cut into pieces")
        self.assertEqual(out["dropped"], "0")
        out = run("SAT 0", *feed(b"\x01\x02"), "RECONNECT", "SAT 1", *feed(b"CH=0\r"))   # a new connection filters again
        self.assertEqual(out["pic"], b"\x01\x02".hex())

    def test_12_reconnect_between_passes_forgets_stale_state(self):
        out = run("SAT 0", *feed(b"X"), "RECONNECT", "SAT 1", *feed(b"CS=60\r"))
        self.assertEqual(out["pic"], b"X".hex(), "CS=60 must not be glued to the old X or ride the old transparent line")
        self.assertEqual(out["refused"], "1")

    def test_13_tx_queue_full_counts_a_drop_and_skips_the_side_effects(self):
        out = run("SAT 1", "QUEUE PR=A", "TXFAIL 1", *feed(b"PR=A\r"))
        self.assertEqual(out["pic"], "")
        self.assertEqual(out["dropped"], "1")
        self.assertEqual(out["ws"], "0")
        self.assertEqual(out["triggers"], "0")
        self.assertEqual(out["queue"], "PR=A;", "no ADR-059 override for a line the PIC never got")
        out = run("SAT 1", "TXFAIL 1", *feed(b"GW=R\r"))
        self.assertEqual(out["resets"], "0")


if __name__ == "__main__":
    unittest.main(verbosity=2)
