"""TASK-1970: REST POST /api/v2/otgw/commands answers a SAT refusal with 409, not with 503 "queue full".

09 on .63 (beta.306): with SAT enabled, POST {"command":"CS=40"} answered 503 "Command not queued (queue
full)" while telnet showed "CS=40 refused, SAT is enabled". handleCommandSubmit() mapped every Dropped
result of addCommandToQueue() to 503, so a client could not tell a refusal that lasts while SAT controls
the boiler from a full queue that clears by itself. On OT-Direct the command was even answered 202 and
refused later on the loop task.

This test compiles the REAL handleCommandSubmit() (restAPI.ino) with the REAL otgwCommandFormatOk() and
satBlocksExternalCommand() (OTGW-firmware.h). Only I/O and the queue are doubled; the queue double refuses a
blocked command while SAT owns the setpoint, as addCommandToQueue() does (OTGW-Core.ino, ADR-179).
Red on the old code (the SAT case answers 503), green on the fix.
Run: python tests/test_rest_command_sat_refusal.py   (OTGW_SRC_DIR=<dir> runs it against other sources)
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from test_sat_pwm_override import compile_cpp  # noqa: E402

SRC = Path(os.environ.get("OTGW_SRC_DIR", HERE.parent / "src" / "OTGW-firmware"))
HDR = SRC / "OTGW-firmware.h"
REST = SRC / "restAPI.ino"


def inline_fn(text, signature):
    m = re.search(re.escape(signature) + r"\s*\{", text)
    assert m, f"{signature} not found"
    i, depth = m.end(), 1
    while depth:
        depth += {"{": 1, "}": -1}.get(text[i], 0)
        i += 1
    return text[m.start():i]


def handler(text):
    start = text.index("static void handleCommandSubmit(const char* cmdStr) {")
    return text[start:text.index("\n}\n", start) + 3]


PRE = r"""
#include <cstdio>
#include <cstring>
#include <cstddef>
#include <cstdint>
#include <string>
class __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper*>(s))
enum class CmdQueueResult : uint8_t { Queued, Replaced, Sent, Dropped };
struct OT_cmd_t { char cmd[32]; };
static OT_cmd_t cmdqueue[1];
static bool g_sat = false, g_full = false;
static int g_status = 0, g_queued = 0, g_refusedNoted = 0;
static std::string g_msg;
static bool hasOTCommandInterface() { return true; }
static bool satOwnsControlSetpoint() { return g_sat; }
static void sendApiError(int code, const __FlashStringHelper* m) { g_status = code; g_msg = reinterpret_cast<const char*>(m); }
static void sendCorsOriginHeader() {}
static void webSend(int code, const __FlashStringHelper*, const __FlashStringHelper*) { g_status = code; g_msg = "queued"; }
static void satNoteExternalRefused(const char*, int) { g_refusedNoted++; }
"""

POST = r"""
// The queue double: refuses a blocked command while SAT owns the setpoint (ADR-179), like addCommandToQueue().
static CmdQueueResult addCommandToQueue(const char* buf, int len) {
  g_queued++;
  if (g_sat && satBlocksExternalCommand(buf, len)) { satNoteExternalRefused(buf, len); return CmdQueueResult::Dropped; }
  return g_full ? CmdQueueResult::Dropped : CmdQueueResult::Queued;
}
"""

MAIN = r"""
static void run(const char* tag, const char* cmd, bool sat, bool full) {
  g_sat = sat; g_full = full; g_status = 0; g_queued = 0; g_refusedNoted = 0; g_msg.clear();
  handleCommandSubmit(cmd);
  std::printf("%s status=%d queued=%d noted=%d msg=%s\n", tag, g_status, g_queued, g_refusedNoted, g_msg.c_str());
}
int main() {
  run("satcs", "CS=40", true, false);
  run("satmm", "MM=50", true, false);
  run("sathw", "HW=1", true, false);
  run("satpr", "PR=A", true, false);
  run("freecs", "CS=40", false, false);
  run("full", "CS=40", false, true);
  return 0;
}
"""


class SatRefusal(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        h = HDR.read_text(encoding="utf-8")
        body = (PRE + inline_fn(h, "inline bool otgwCommandFormatOk(const char* s, size_t len)") + "\n"
                + inline_fn(h, "inline bool satBlocksExternalCommand(const char* buf, int len)") + "\n"
                + POST + handler(REST.read_text(encoding="utf-8")) + MAIN)
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text(body, encoding="utf-8")
        exe = compile_cpp(cpp, Path(cls.tmp.name) / ("t" + (".exe" if os.name == "nt" else "")))
        if exe is None:
            raise unittest.SkipTest("no C++ compiler on this host")
        out = subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout
        cls.r = {l.split()[0]: l for l in out.strip().splitlines()}

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_sat_refusal_is_409_and_never_queued(self):
        for tag in ("satcs", "satmm"):
            line = self.r[tag]
            self.assertIn("status=409", line)
            self.assertIn("queued=0", line)
            self.assertIn("noted=1", line)
            self.assertIn("SAT is in control", line)

    def test_free_commands_pass_while_sat_runs(self):
        self.assertIn("status=202", self.r["sathw"])
        self.assertIn("status=202", self.r["satpr"])

    def test_without_sat_cs_is_queued(self):
        self.assertIn("status=202", self.r["freecs"])

    def test_full_queue_stays_503(self):
        self.assertIn("status=503", self.r["full"])
        self.assertIn("queue full", self.r["full"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
