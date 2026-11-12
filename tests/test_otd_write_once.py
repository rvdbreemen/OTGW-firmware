#!/usr/bin/env python3
"""TASK-1855: a write command (VS=, CS=, CL=, ...) puts exactly one WRITE frame on the bus, as the PIC does.

Defect (09's bench of beta.197 on .61, loopback): enqueueWriteCommand() puts the one-shot WRITE in the command ring and
updateWriteCache() marks the matching write schedule entry valueSet. That entry never ran, so its lastSentMs is 0 and
the scheduler finds it due at once: the same WRITE goes out a second time right after the first.

REAL code, cut from OTDirect.ino and compiled on the host: the command ring, otCmdEnqueue, updateWriteCache,
otdNoteCmdSent (when present), enqueueWriteCommand, scheduleMasterRequest (master / loopback consumer) and
otdAltScheduleIndex + otdAltSlotTaken (gateway consumer, the slot of a thermostat request). STUBBED: everything the mode-switch harness
tests/test_otd_mode_switch_queue_reset.py stubs (its prelude is reused), plus a real OpenTherm::buildRequest, one
enabled write schedule entry (MsgID 71, 30 s) and setOverride().
Old-vs-fix: OTGW_OTD_INO=<older OTDirect.ino> python tests/test_otd_write_once.py
Run: python tests/test_otd_write_once.py
"""
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TESTS = Path(__file__).resolve().parent
sys.path.insert(0, str(TESTS))
from test_sat_pwm_override import compile_cpp  # noqa: E402
import test_otd_mode_switch_queue_reset as msq  # noqa: E402

OTD_INO = Path(os.environ.get("OTGW_OTD_INO", msq.REPO / "src" / "OTGW-firmware" / "OTDirect.ino"))

OLD_BUILD = ("struct OpenTherm { static unsigned long buildRequest(OpenThermMessageType, OpenThermMessageID, uint16_t) "
             "{ return 0; } };")
NEW_BUILD = ("struct OpenTherm { static unsigned long buildRequest(OpenThermMessageType t, OpenThermMessageID id, uint16_t v) "
             "{ return ((unsigned long)t << 28) | ((unsigned long)id << 16) | v; } };")

EXTRA = r"""
#define DebugTf(...) do {} while (0)
static void setOverride(uint8_t, uint16_t) {}
"""

MAIN = r"""
static int sentWrites(uint8_t id) {
  int n = 0;
  for (int i = 0; i < g_sent; i++) if (((g_sentFrames[i] >> 28) & 7) == 1 && ((g_sentFrames[i] >> 16) & 0xFF) == id) n++;
  return n;
}
int main(int argc, char** argv) {
  const bool alt = argv[1][0] == 'a';
  otCurrentMode = alt ? OTD_MODE_GATEWAY : OTD_MODE_MASTER;
  for (int i = 0; i < OT_SCHEDULE_SIZE; i++) otSchedule[i] = { 0, 60000, 0, true, false, false, 0 };
  otSchedule[0] = { 71, 30000, 0, false, true, false, 0 };      // the VS= write entry: never sent, not yet valued
  if (alt) otSchedule[1] = { 25, 30000, 0, false, false, false, 0 };   // gateway: a read entry competes for the slots
  g_now = 100000;
  enqueueWriteCommand(71, 15, "VS");                              // VS=15
  // 10 s of consumer passes, one per 100 ms (the MI= gap); gateway: one thermostat request per pass is the slot
  for (int k = 0; k < 100; k++) {
    g_now += 100;
    if (alt) otdAltSlotTaken(0x00120000UL); else scheduleMasterRequest();
  }
  const int first10s = sentWrites(71);
  for (int k = 0; k < 300; k++) {                                 // up to 40 s: the 30 s refresh comes once
    g_now += 100;
    if (alt) otdAltSlotTaken(0x00120000UL); else scheduleMasterRequest();
  }
  int firstTwo71 = 0;
  for (int i = 0; i < 2 && i < g_sent; i++) if (((g_sentFrames[i] >> 16) & 0xFF) == 71) firstTwo71++;
  printf("first10s=%d upto40s=%d firsttwo71=%d\n", first10s, sentWrites(71), firstTwo71);
  return 0;
}
"""


def build_source():
    src = OTD_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
    ring_at = src.index("static constexpr uint8_t OT_CMD_QUEUE_SIZE")
    ring = src[ring_at:src.index("// TASK-494: enqueue with coalesce-by-MsgID.", ring_at)]
    # TASK-1840: otdAltSlotTaken() answers a MsgID 20 slot with otdClockReply(); its state and helper come along
    clock = ""
    if "// --- otdClockReply end" in src:
        a = src.index("static uint32_t otSCLastCommandMs")
        e = src.index("// --- otdClockReply end")
        if "static uint16_t otdUpperBoundF88(" in src:   # TASK-1844: the slot answer also runs otdPicAnswer()
            e = src.index("\n}\n", src.index("static uint16_t otdUpperBoundF88(")) + 3
            clock = "static uint16_t otBoilerCache[128];\nstatic bool otBoilerCacheValid[128];\n"   # empty: default bounds
        clock += src[a:e]
    parts = [ring, clock, msq.cut_braced(src, "static bool otCmdEnqueue(uint32_t frame)"),
             msq.cut_braced(src, "static void updateWriteCache(uint8_t msgId, uint16_t value)")]
    if "static void otdNoteCmdSent(" in src:
        parts.append(msq.cut_braced(src, "static void otdNoteCmdSent(uint32_t frame, uint32_t now)"))
    parts.append("static bool otdWriteRefused = false;")
    parts += [msq.cut_braced(src, "static bool enqueueWriteCommand(uint8_t msgId, uint16_t dataValue, const char* label)"),
              msq.cut_braced(src, "static void scheduleMasterRequest()"),
              msq.cut_braced(src, "static uint8_t otdAltScheduleIndex(uint32_t now)"),
              msq.cut_braced(src, "static bool otdAltSlotTaken(unsigned long thermostatFrame)"),
              "static void resetTransientState() {}"]
    pre = msq.PRELUDE
    assert OLD_BUILD in pre
    stub = "static uint8_t otdAltScheduleIndex(uint32_t) { return OT_SCHEDULE_SIZE; }"
    assert stub in pre
    pre = pre.replace(stub, "static bool otdThermostatWritesEntry[OT_SCHEDULE_SIZE];")
    pre = pre.replace(OLD_BUILD, NEW_BUILD).replace('printf("  sent 0x%08lX\\n", (unsigned long)request);', "")
    return pre + EXTRA + "\n".join(parts) + "\n" + MAIN


class WriteOnce(unittest.TestCase):
    tmp = None
    exe = None

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="otd_write_once_")
        cpp = Path(cls.tmp) / "t.cpp"
        cpp.write_text(build_source(), encoding="utf-8")
        cls.exe = compile_cpp(cpp, Path(cls.tmp) / ("t.exe" if os.name == "nt" else "t"))

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def run_case(self, consumer):
        if not self.exe:
            self.skipTest("no host compiler")
        out = subprocess.run([str(self.exe), consumer], capture_output=True, text=True, check=True).stdout
        return dict(kv.split("=") for kv in out.split())

    def test_master_one_write_then_refresh_after_30s(self):
        r = self.run_case("sched")
        self.assertEqual(r["first10s"], "1", "VS=15 puts one WRITE 71 on the bus, not two")
        self.assertEqual(r["upto40s"], "2", "the schedule refresh follows after its 30 s interval")

    def test_gateway_alt_slot_next_slot_goes_to_another_entry(self):
        # Gateway mode fills every alternative slot with the most overdue schedule entry (otdAltScheduleIndex), as the
        # PIC cycles its alternatives; there is no interval wait. The one-shot WRITE must count as the write entry's
        # send, so the next slot goes to the read entry instead of the same WRITE 71 again.
        r = self.run_case("alt")
        self.assertEqual(r["firsttwo71"], "1", "WRITE 71 once, then READ 25, not WRITE 71 twice")


if __name__ == "__main__":
    unittest.main(verbosity=2)
