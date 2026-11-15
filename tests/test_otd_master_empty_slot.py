#!/usr/bin/env python3
"""TASK-1808: in OT-Direct master mode a thermostat READ of an id without a cached boiler value is answered Data-Invalid.

ADR-066 (mitigation: "respond with DataInvalid flag until cache is populated") and OpenTherm spec v4.2 section 4.3:
Unknown-DataID means "id not supported" (a thermostat may stop asking it), Data-Invalid means "supported, no value now".
After a master-mode boot the 60 s schedule tier (MsgID 3, 5, 48, 49 ...) has no cached value for up to 60 s.
An id the boiler itself answered Unknown-DataID keeps the Unknown-DataID answer; OEM ids 128-255 are never cached.

REAL code, cut from src/OTGW-firmware/OTDirect.ino: the otUnknownCounters block (getUnknownCount, incUnknownCount,
clearUnknownCount), otBoilerCacheStore(), buildOTResponse() and handleMasterModeSlaveFrame().
STUBBED: OpenTherm::getMessageType, setOTParityBit, the override/write-cache sinks, satOwnsControlSetpoint, millis,
the frame log; otdSendReply records the reply. A boiler answer is fed as handleMasterResponse() does for it:
otBoilerCacheStore(response), then incUnknownCount(id) for Unknown-DataID or clearUnknownCount(id) otherwise
(OTDirect.ino handleMasterResponse); that sequence is copied here, not cut, so it is checked by reading only.
Old-vs-fix: OTGW_OTD_INO=<older OTDirect.ino> python tests/test_otd_master_empty_slot.py
Run: python tests/test_otd_master_empty_slot.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TESTS = Path(__file__).resolve().parent
sys.path.insert(0, str(TESTS))
from test_sat_pwm_override import compile_cpp  # noqa: E402

REPO = TESTS.parent
OTD_INO = Path(os.environ.get("OTGW_OTD_INO", REPO / "src" / "OTGW-firmware" / "OTDirect.ino"))

STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define F(x) x
#define OTDDebugTln(x) do {} while (0)
enum class OpenThermMessageType { READ_DATA, WRITE_DATA, INVALID_DATA, RESERVED, READ_ACK, WRITE_ACK, DATA_INVALID,
                                  UNKNOWN_DATA_ID };
struct OpenTherm {
  static OpenThermMessageType getMessageType(unsigned long m) { return (OpenThermMessageType)((m >> 28) & 7); }
};
static uint32_t millis() { return 1000; }
static bool satOwnsControlSetpoint() { return false; }
static void setOverride(uint8_t, uint16_t) {}
static void clearWriteOverride(uint8_t) {}
static void updateWriteCache(uint8_t, uint16_t) {}
static void setOTParityBit(unsigned long&) {}
static void bridgeFrameToParser(char, unsigned long) {}
static unsigned long g_reply = 0;
static void otdSendReply(unsigned long r) { g_reply = r; }
static bool otBoilerCacheValid[128];
static uint16_t otBoilerCache[128];
static uint32_t otCSLastCommandMs = 0;
static struct { struct { bool bOnline = true; } otBus; } state;   // TASK-1807: no answer while offline
"""

MAIN = r"""
static unsigned long frame(uint8_t type, uint8_t id, uint16_t v) {
  return ((unsigned long)type << 28) | ((unsigned long)id << 16) | v;
}
// A boiler answer, handled as handleMasterResponse() does for the cache and the unknown counter.
static void boiler(uint8_t type, uint8_t id, uint16_t v) {
  unsigned long r = frame(type, id, v);
  otBoilerCacheStore(r);
  if (type == 7) { if (id != 0) incUnknownCount(id); }
  else if (id != 0) clearUnknownCount(id);
}
static void ask(uint8_t id) {
  g_reply = 0xFFFFFFFFUL;   // TASK-1807: no answer leaves the sentinel
  handleMasterModeSlaveFrame(frame(0, id, 0));
  if (g_reply == 0xFFFFFFFFUL) { printf("id=%u none\n", id); return; }
  printf("id=%u type=%lu data=%04lX\n", id, (g_reply >> 28) & 7, g_reply & 0xFFFF);
}
// argv: a script of steps: "Bt:id:hex" = boiler answer of type t, "Rid" = thermostat READ of id
int main(int argc, char** argv) {
  for (int i = 1; i < argc; i++) {
    const char* a = argv[i];
    if (a[0] == 'B') {
      unsigned t, id, v; sscanf(a + 1, "%u:%u:%x", &t, &id, &v); boiler((uint8_t)t, (uint8_t)id, (uint16_t)v);
    } else if (a[0] == 'O') {
      state.otBus.bOnline = false;   // the boiler's last MsgID 0 failed
    } else if (a[0] == 'N') {
      state.otBus.bOnline = true;
    } else if (a[0] == 'R') {
      ask((uint8_t)atoi(a + 1));
    }
  }
  return 0;
}
"""


def cut(src, start, end_marker):
    a = src.index(start)
    b = src.index(end_marker, a)
    return src[a:b]


def build_source():
    src = OTD_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
    counters = cut(src, "static uint8_t otUnknownCounters[32]", "\n// Apply an active override") + "\n"
    store = cut(src, "static void otBoilerCacheStore(unsigned long response) {", "\n// ----") + "\n"
    resp = cut(src, "static unsigned long buildOTResponse(uint8_t type, uint8_t msgId, uint16_t data) {",
               "\nstatic void handleMasterModeSlaveFrame(") + "\n"
    handler = cut(src, "static void handleMasterModeSlaveFrame(unsigned long frame) {", "\n// ----") + "\n"
    return STUBS + counters + store + resp + handler + MAIN


class MasterEmptySlot(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text(build_source(), encoding="utf-8")
        cls.exe = compile_cpp(cpp, Path(cls.tmp.name) / ("t.exe" if os.name == "nt" else "t"))
        if cls.exe is None:
            raise unittest.SkipTest("no C++ compiler")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_script(self, *steps):
        out = subprocess.run([str(self.exe), *steps], capture_output=True, text=True, check=True).stdout
        return [(int(m.group(1)), int(m.group(2)), m.group(3))
                for m in re.finditer(r"id=(\d+) type=(\d) data=([0-9A-F]{4})", out)]

    def test_no_answer_from_the_cache_while_the_boiler_is_offline(self):
        # TASK-1807 AC1: the boiler answered (flame on), then its MsgID 0 failed: the thermostat gets no answer, as from
        # a boiler that is gone, not READ_ACK with the last status. Back online: the cache answers again.
        out = subprocess.run([str(self.exe), "B4:0:0308", "B4:25:3C00", "O", "R0", "R25", "N", "R0"],
                             capture_output=True, text=True, check=True).stdout.splitlines()
        self.assertEqual(out[:3], ["id=0 none", "id=25 none", "id=0 type=4 data=0308"], out)

    def test_empty_slot_after_boot_is_data_invalid(self):
        # AC1: right after a master-mode boot MsgID 3, 5, 48, 49 have no value yet: Data-Invalid (6), not 7
        r = self.run_script("R3", "R5", "R48", "R49")
        self.assertEqual([t for _, t, _ in r], [6, 6, 6, 6], r)

    def test_cached_value_is_read_ack(self):
        # unchanged: once the boiler answered, the thermostat gets its value as READ_ACK
        r = self.run_script("B4:3:0301", "R3")
        self.assertEqual(r, [(3, 4, "0301")])

    def test_boiler_unknown_keeps_unknown(self):
        # AC1: an id the boiler answered Unknown-DataID keeps the Unknown-DataID answer (one answer is enough)
        r = self.run_script("B7:49:0000", "R49")
        self.assertEqual(r, [(49, 7, "0000")])

    def test_boiler_supports_again_after_unknown(self):
        # a later boiler READ_ACK clears the unknown count and fills the cache: READ_ACK again
        r = self.run_script("B7:48:0000", "B4:48:3C14", "R48")
        self.assertEqual(r, [(48, 4, "3C14")])

    def test_data_invalid_is_not_cached_on_an_empty_slot(self):
        # TASK-1925: a Data-Invalid reply carries no value; the old store kept its data bytes and the thermostat got
        # READ_ACK 0000. The empty slot stays empty: Data-Invalid (6), as right after boot.
        r = self.run_script("B6:5:0000", "R5")
        self.assertEqual(r, [(5, 6, "0000")])

    def test_data_invalid_keeps_the_previous_value(self):
        # TASK-1925 rule: the previous value stays, as handleMasterResponse already does for a real boiler
        r = self.run_script("B4:25:3C00", "B6:25:1234", "R25")
        self.assertEqual(r, [(25, 4, "3C00")])

    def test_oem_id_stays_unknown(self):
        # ids 128-255 are never cached (otBoilerCacheStore): Unknown-DataID, as before
        r = self.run_script("R131")
        self.assertEqual(r, [(131, 7, "0000")])


if __name__ == "__main__":
    unittest.main(verbosity=2)
