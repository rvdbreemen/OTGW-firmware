#!/usr/bin/env python3
"""TASK-1749: host test for the TT=/TC= remote override in GET /api/v2/otdirect/overrides.

The real sendOTDirectOverridesJSON() and otdActiveOverrideCount() are cut from
src/OTGW-firmware/OTDirect.ino, the real JsonEmit from jsonEmit.h and the remote-override
enum/struct from OTDirecttypes.h; all are compiled with a host C++ compiler against a small Print
stub. Nothing is re-implemented. OTGW_OTD_INO points at another copy of OTDirect.ino (e.g.
`git show 2fedae505:src/OTGW-firmware/OTDirect.ino`) for old-vs-fix runs. Without a compiler the
compiled cases are skipped.

Run: python tests/test_otd_overrides_remote_json.py
"""
import json
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

FW = REPO / "src" / "OTGW-firmware"
OTD_INO = Path(os.environ.get("OTGW_OTD_INO", FW / "OTDirect.ino"))
SRC = OTD_INO.read_text(encoding="utf-8")

PRELUDE = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#define PROGMEM
#define PSTR(s) (s)
#define F(s) (reinterpret_cast<const __FlashStringHelper*>(s))
#define FPSTR(s) (reinterpret_cast<const __FlashStringHelper*>(s))
#define snprintf_P snprintf
typedef const char* PGM_P;
class __FlashStringHelper;
struct String { const char* c_str() const { return ""; } };
struct Print {
  void print(char c) { fputc(c, stdout); }
  void print(const char* s) { fputs(s, stdout); }
  void print(const __FlashStringHelper* s) { fputs(reinterpret_cast<const char*>(s), stdout); }
};
struct AsyncResponseStream : Print {};
"""

MAIN = r"""
@TYPES@
#define OT_OVERRIDE_COUNT 3
#define OT_RESPONSE_OVERRIDE_MAX 2
#define OT_RESPONSE_MODIFY_MAX 2
#define OT_CMD_QUEUE_SIZE 8
struct WriteOv { bool active; uint8_t msgId; uint16_t overrideValue; };
struct RespOv  { bool active; uint8_t msgId; uint16_t value; };
static WriteOv otOverrides[OT_OVERRIDE_COUNT];
static RespOv  otResponseOverrides[OT_RESPONSE_OVERRIDE_MAX];
static RespOv  otResponseModifiers[OT_RESPONSE_MODIFY_MAX];
static uint8_t otUnknownIds[2] = {7, 9};
static uint8_t otUnknownIdCount = 0;
static uint32_t otCmdQueueHighWater = 0;
static uint32_t otCmdQueueDepth() { return 0; }
static bool satOwnsControlSetpoint() { return false; }
static AsyncResponseStream gStream;
static AsyncResponseStream* restBeginStream(const char*) { return &gStream; }
static void restFinalize() {}
static OTRemoteOverrideState otRemoteOverride = { OT_OVERRIDE_NONE, 0, 0, 0, 0, false, 0, false, false };
static OTDirectMode otCurrentMode = OTD_MODE_GATEWAY;
@JSONEMIT@
@LISTED@
@COUNT@
@BODY@
int main(int argc, char** argv) {
  // args: mode(0/1/2) f88(hex) answered accepted mismatches writeSlots
  otRemoteOverride.mode = (OTRemoteOverrideMode)atoi(argv[1]);
  otRemoteOverride.f88Value = (uint16_t)strtoul(argv[2], 0, 16);
  otRemoteOverride.bMsgId9Answered = atoi(argv[3]);
  otRemoteOverride.bAccepted = atoi(argv[4]);
  otRemoteOverride.mismatchCount = (uint8_t)atoi(argv[5]);
  int slots = atoi(argv[6]);
  if (argc > 7) otCurrentMode = (OTDirectMode)atoi(argv[7]);
  for (int i = 0; i < slots; i++) { otOverrides[i].active = true; otOverrides[i].msgId = 1 + i; otOverrides[i].overrideValue = 0x1234; }
  printf("COUNT=%u\n", (unsigned)otdActiveOverrideCount());
  sendOTDirectOverridesJSON();
  printf("\n");
  return 0;
}
"""


def fn(src, sig):
    m = re.search(r"^(?:static )?%s\{.*?^\}" % re.escape(sig), src, re.S | re.M)
    return m.group(0) if m else None


def types_block():
    t = (FW / "OTDirecttypes.h").read_text(encoding="utf-8")
    m = re.search(r"enum OTDirectMode : uint8_t \{.*?\};", t, re.S)
    e = re.search(r"enum OTRemoteOverrideMode : uint8_t \{.*?\};", t, re.S)
    s = re.search(r"struct OTRemoteOverrideState \{.*?\n\};", t, re.S)
    return m.group(0) + "\n" + e.group(0) + "\n" + s.group(0)


class Compiled(unittest.TestCase):
    exe = None
    err = None
    tmp = None

    @classmethod
    def setUpClass(cls):
        body = fn(SRC, "void sendOTDirectOverridesJSON() ")
        count = fn(SRC, "uint8_t otdActiveOverrideCount() ")
        listed = fn(SRC, "bool otdRemoteOverrideListed() ") or ""   # absent before the master-mode rule
        if body is None:
            cls.err = "sendOTDirectOverridesJSON not found"
            return
        if count is None:   # source before TASK-1749: no counting helper; the count assertions then fail
            count = "static uint8_t otdActiveOverrideCount() { return 255; }"
        je = (FW / "jsonEmit.h").read_text(encoding="utf-8")
        je = re.sub(r"#include <(Arduino|platform)\.h>.*", "", je)
        cls.tmp = tempfile.TemporaryDirectory()
        d = Path(cls.tmp.name)
        cpp = d / "t.cpp"
        cpp.write_text(PRELUDE + MAIN.replace("@TYPES@", types_block()).replace("@JSONEMIT@", je)
                       .replace("@COUNT@", "static " + count if not count.startswith("static") else count)
                       .replace("@LISTED@", listed)
                       .replace("@BODY@", body), encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / ("t.exe" if os.name == "nt" else "t"))
        if cls.exe is None:
            cls.err = "SKIP no host compiler"

    @classmethod
    def tearDownClass(cls):
        if cls.tmp:
            cls.tmp.cleanup()

    def setUp(self):
        if self.err and self.err.startswith("SKIP"):
            self.skipTest(self.err)
        if self.err:
            self.fail(self.err)

    def run_h(self, mode=0, f88=0, answered=0, accepted=0, mismatches=0, slots=0, otd_mode=1):
        out = subprocess.run([str(self.exe), str(mode), "%X" % f88, str(answered), str(accepted),
                              str(mismatches), str(slots), str(otd_mode)], capture_output=True, text=True, check=True).stdout
        first, _, rest = out.partition("\n")
        return int(first.split("=")[1]), json.loads(rest)

    def test_no_override_remote_is_null_and_count_unchanged(self):
        count, doc = self.run_h(slots=2)
        self.assertEqual(count, 2)
        self.assertIn("remote", doc["overrides"])
        self.assertIsNone(doc["overrides"]["remote"])
        self.assertEqual(len(doc["overrides"]["write"]), 2)

    def test_tt_active_listed_as_temporary_and_counted(self):
        count, doc = self.run_h(mode=1, f88=0x1580, answered=1, accepted=0, mismatches=2, slots=1)   # 21.5 C
        self.assertEqual(count, 2, "1 write slot + 1 remote override")
        r = doc["overrides"]["remote"]
        self.assertEqual(r, {"mode": "temporary", "value": 21.5, "msgid9_answered": True,
                             "accepted": False, "mismatches": 2})
        self.assertEqual(len(doc["overrides"]["write"]), 1, "remote override is not a write entry")

    def test_tc_active_listed_as_constant_accepted(self):
        count, doc = self.run_h(mode=2, f88=0x1400, answered=1, accepted=1)   # 20.0 C
        self.assertEqual(count, 1)
        r = doc["overrides"]["remote"]
        self.assertEqual(r["mode"], "constant")
        self.assertEqual(r["value"], 20.0)
        self.assertTrue(r["accepted"])

    def test_master_mode_lists_tt_only_as_the_write_slot(self):
        # Master mode (OTD_MODE_MASTER = 3): TT= still writes MsgID 16, so its write slot shows it;
        # the remote entry is not listed and not counted twice.
        count, doc = self.run_h(mode=1, f88=0x1580, slots=1, otd_mode=3)
        self.assertIsNone(doc["overrides"]["remote"])
        self.assertEqual(count, 1, "master mode: only the write slot counts")

    def test_existing_fields_and_order_kept(self):
        _, doc = self.run_h(mode=1, f88=0x1580)
        self.assertEqual(list(doc["overrides"])[:6], ["write", "response", "modify", "unknown", "sat_owns", "remote"])
        self.assertEqual(list(doc), ["overrides", "queue"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
