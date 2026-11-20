#!/usr/bin/env python3
"""TASK-1847: OT-Direct PM= reads a whole TSP / fault-history table or string, as the PIC 6.8.

PIC ground truth (OTGW-other-projects/otgw-6.8, repo 770aa45): SetPrioMessage gateway.asm:5026-5038 (0-255),
TSPBufferSize :3363-3378, TSPReadEntry :3389-3397, StringCharacter :3399-3407, SendPriorityMsg :3464-3470. The
expected request frames are the PIC's own test scripts, letter for letter:
  tests/tsp-1.stc  Boiler.TSP = "10: 42, 88, 17", PM=10 -> R000A0000, R800B0000, R000B0100, R000B0200, then none
  tests/str-1.stc  Boiler.string = "93: Test",    PM=93 -> R805D0000, R005D0100, R005D0200, R805D0300, then none

The code under test is REAL: the "// --- otdPmChain" block and the PM= branch of handleOTDirectCommand(), cut from
src/OTGW-firmware/OTDirect.ino. The doubles: otCmdEnqueue() records the frame, a stand-in boiler answers each
recorded request, and the answer goes to otdPmOnAnswer() as handleMasterResponse() passes it (gateway origin).

Old-vs-fix: git show <old-rev>:src/OTGW-firmware/OTDirect.ino > $TMP/OTDirect.ino
            OTGW_OTD_INO=$TMP/OTDirect.ino python tests/test_otd_pm_chain_tsp.py
Run: python tests/test_otd_pm_chain_tsp.py
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402

OTD_INO = Path(os.environ.get("OTGW_OTD_INO", REPO / "src" / "OTGW-firmware" / "OTDirect.ino"))
SRC = OTD_INO.read_text(encoding="utf-8").replace("\r\n", "\n")


def braced(src, start_pat):
    m = re.search(start_pat, src, re.M)
    if not m:
        raise AssertionError("not found: " + start_pat)
    i, depth = src.index("{", m.start()), 0
    for j in range(i, len(src)):
        if src[j] == "{":
            depth += 1
        elif src[j] == "}":
            depth -= 1
            if depth == 0:
                return src[m.start():j + 1]
    raise AssertionError("unbalanced")


PRE = r"""
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <vector>
#define PSTR(s) (s)
#define snprintf_P snprintf
#define OTDDebugTf(...) do {} while (0)
enum class OpenThermMessageType : uint8_t { READ_DATA = 0, WRITE_DATA = 1, READ_ACK = 4, WRITE_ACK = 5, DATA_INVALID = 6, UNKNOWN_DATA_ID = 7 };
typedef uint8_t OpenThermMessageID;
struct OpenTherm {
  static unsigned long buildRequest(OpenThermMessageType t, OpenThermMessageID id, uint16_t data) {
    unsigned long f = ((unsigned long)t << 28) | ((unsigned long)id << 16) | data;
    int ones = 0; for (unsigned long x = f; x; x >>= 1) ones += (int)(x & 1);
    if (ones & 1) f |= 0x80000000UL;
    return f;
  }
};
struct OTScheduleEntry { uint8_t msgId; bool disabled; uint32_t lastSentMs; };
static OTScheduleEntry otSchedule[] = { {0, false, 5}, {25, false, 5} };
static constexpr uint8_t OT_SCHEDULE_SIZE = sizeof(otSchedule) / sizeof(otSchedule[0]);
static std::vector<uint32_t> g_q;
static bool otCmdEnqueue(uint32_t f) { g_q.push_back(f); return true; }
static bool otdWriteRefused = false;   // TASK-1804 B3e: set on a refused enqueue (never here: the stub accepts)
static char g_rsp[32], g_status[8];
static void synthesizeResponse(const char*, const char* r) { snprintf(g_rsp, sizeof g_rsp, "%s", r); }
static void otDirectBridgeProcessStatus(const char* s) { snprintf(g_status, sizeof g_status, "%s", s); }
"""

MAIN = r"""
#ifndef HAVE_PMCHAIN
static void otdPmStart(uint8_t) {}
static uint32_t otdPmOnAnswer(uint8_t, uint8_t, uint16_t) { return 0; }
#endif
static bool cmd(const char* buf, const char* value) {
  const char cmd0 = buf[0], cmd1 = buf[1];
  char rspBuf[16];
  if (false) {}
  @BRANCH@
  return true;
}
// The stand-in boiler: TSP 10 = {42, 88, 17}; string 93 = "Test" (count in the high byte, as the PIC reads it).
static const uint8_t kTsp[] = {42, 88, 17};
static const char* kStr = "Test";
static uint32_t answer(uint32_t req) {
  const uint8_t id = (req >> 16) & 0xFF, idx = (req >> 8) & 0xFF;
  uint16_t data = 0; uint8_t type = 4;
  if (id == 10) data = (uint16_t)(sizeof kTsp << 8);
  else if (id == 11) data = (uint16_t)((idx << 8) | (idx < sizeof kTsp ? kTsp[idx] : 0));
  else if (id == 93) data = (uint16_t)((strlen(kStr) << 8) | (idx < strlen(kStr) ? (uint8_t)kStr[idx] : 0));
  else if (id == 12) data = 0;          // an empty table
  else type = 7;                        // Unknown-DataID
  return ((uint32_t)type << 28) | ((uint32_t)id << 16) | data;
}
int main(int argc, char** argv) {
  cmd("PM", argv[1]);
  printf("rsp=%s st=%s\n", g_rsp, g_status);
  for (size_t i = 0; i < g_q.size() && i < 20; i++) {
    printf("R%08X\n", (unsigned)g_q[i]);
    const uint32_t a = answer(g_q[i]);
    const uint32_t next = otdPmOnAnswer((a >> 16) & 0xFF, (a >> 28) & 7, a & 0xFFFF);
    if (next) otCmdEnqueue(next);
  }
  return 0;
}
"""

_tmp = None
_exe = None


def setUpModule():
    global _tmp, _exe
    m = re.search(r"// --- otdPmChain begin\n(.*?)// --- otdPmChain end", SRC, re.S)
    chain = ("#define HAVE_PMCHAIN 1\n" + m.group(1)) if m else ""
    branch = braced(SRC, r"else if \(cmd0 == 'P' && cmd1 == 'M'\) \{")
    _tmp = Path(tempfile.mkdtemp(prefix="otd_pm_"))
    cpp = _tmp / "t.cpp"
    cpp.write_text(PRE + chain + MAIN.replace("@BRANCH@", branch), encoding="utf-8")
    _exe = compile_cpp(cpp, _tmp / ("t.exe" if os.name == "nt" else "t"))
    if _exe is None:
        raise unittest.SkipTest("no C++ compiler")


def tearDownModule():
    if _tmp:
        shutil.rmtree(_tmp, ignore_errors=True)


def run(value):
    out = subprocess.run([str(_exe), value], capture_output=True, text=True, check=True).stdout.splitlines()
    return out[0], out[1:]


class PmChain(unittest.TestCase):
    def test_pm10_reads_every_tsp_like_tsp_1_stc(self):
        head, frames = run("10")
        self.assertEqual(head, "rsp=10 st=")
        self.assertEqual(frames, ["R000A0000", "R800B0000", "R000B0100", "R000B0200"])

    def test_pm93_reads_every_character_like_str_1_stc(self):
        _, frames = run("93")
        self.assertEqual(frames, ["R805D0000", "R005D0100", "R005D0200", "R805D0300"])

    def test_pm131_is_accepted(self):
        head, frames = run("131")
        self.assertEqual(head, "rsp=131 st=")
        self.assertEqual(len(frames), 1)          # one read; the stand-in boiler answers Unknown-DataID

    def test_an_empty_table_reads_nothing_more(self):
        _, frames = run("12")
        self.assertEqual(len(frames), 1)

    def test_refusals_like_set_prio_message(self):
        self.assertEqual(run("256")[0], "rsp= st=OR")
        self.assertEqual(run("x")[0], "rsp= st=SE")
        self.assertEqual(run("10x")[0], "rsp= st=SE")


class Wiring(unittest.TestCase):
    def test_handle_master_response_feeds_the_chain_from_gateway_requests_only(self):
        fn = braced(SRC, r"^static void handleMasterResponse\(\) \{")
        self.assertIn("otLastRequestOrigin == OT_DIRECT_ORIGIN_GATEWAY", fn)
        self.assertIn("otdPmOnAnswer(", fn)
        self.assertLess(fn.index("otdPmOnAnswer("), fn.index("otLastRequestOrigin = OT_DIRECT_ORIGIN_GATEWAY;"))
        self.assertIn("otCmdEnqueue(next)", fn)


if __name__ == "__main__":
    unittest.main(verbosity=2)
