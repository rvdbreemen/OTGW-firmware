#!/usr/bin/env python3
"""TASK-1992: no write queued during a simulation, or cleared since, reaches the real boiler after the simulation ends.

Bench (09, .61 OT-Direct master, beta.331, 2026-11-16, 09 scratchpad b331/): with satsimulation=true no OT frame goes out
(satSimulationBlocksBusTx() makes sendMasterRequestAsync() return false), the command ring kept filling, and at
satsimulation=false it sent R90013D4D (CS 61.30) after CS=0, R900E6400 (MM 100) after MM=T and R90101480 (TT 20.5) after
TT=0, 6 minutes old. Two defects: a clear (clearWriteOverride) left the pending WRITE of its MsgID in the ring, and the
ring kept its entries across the simulation's TX block.

REAL code (cut from src/OTGW-firmware/OTDirect.ino, the ring harness of tests/test_otd_mode_switch_queue_reset.py):
the ring statics and helpers, otCmdEnqueue, scheduleMasterRequest (the consumer), otdFlushCmdQueue, clearWriteOverride
and, when present, the otdSimExitFlush block of loopOTDirect. STUBBED: sendMasterRequestAsync records the frame and
refuses while the simulation is on (as satSimulationBlocksBusTx), clearOverride, the schedule table (all disabled).
Old-vs-fix: OTGW_OTD_INO=<older OTDirect.ino> python tests/test_otd_sim_exit_stale_writes.py
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TESTS = Path(__file__).resolve().parent
sys.path.insert(0, str(TESTS))
import test_otd_mode_switch_queue_reset as msq  # noqa: E402
from test_sat_pwm_override import compile_cpp  # noqa: E402

SRC = msq.OTD_INO.read_text(encoding="utf-8").replace("\r\n", "\n")

OLD_SETTINGS = "struct { struct { bool bSummerMode = false; bool bEnableSlave = false; } otd; } settings;"
NEW_SETTINGS = ("struct { struct { bool bSummerMode = false; bool bEnableSlave = false; } otd;\n"
                "         struct { bool bSimulation = false; } sat; } settings;")
OLD_STATE_TAIL = "         struct { bool bOnline = true; } otBus; } state;"
NEW_STATE_TAIL = ("         struct { bool bOnline = true; } otBus;\n"
                  "         struct { bool bOTGWSimulation = false; } debug; } state;")
OLD_SEND = """static bool sendMasterRequestAsync(unsigned long request, OTDirectRequestOrigin) {
  g_sentFrames[g_sent++] = (uint32_t)request;"""
NEW_SEND = """static bool sendMasterRequestAsync(unsigned long request, OTDirectRequestOrigin) {
  if (settings.sat.bSimulation) return false;   // satSimulationBlocksBusTx(): no bus traffic during the simulation
  g_sentFrames[g_sent++] = (uint32_t)request;"""

MAIN = r"""
static void clearOverride(uint8_t) {}
@@CLEAR@@
@@SIMEXIT@@
static void simExitCheck() { @@SIMCALL@@ }
static uint32_t CS613 = 0x10000000UL | (1UL  << 16) | 0x3D4D;   // WRITE_DATA MsgID 1  CS 61.30
static uint32_t MM100 = 0x10000000UL | (14UL << 16) | 0x6400;   // WRITE_DATA MsgID 14 MM 100
static uint32_t TT205 = 0x10000000UL | (16UL << 16) | 0x1480;   // WRITE_DATA MsgID 16 TT 20.5
static uint32_t SW50  = 0x10000000UL | (56UL << 16) | 0x3200;   // WRITE_DATA MsgID 56 SW 50
static void pass(int n) {
  for (int i = 0; i < n; i++) {
    g_now += 1000;
    for (int k = 0; k < OT_SCHEDULE_SIZE; k++) otSchedule[k].disabled = true;
    simExitCheck();
    scheduleMasterRequest();
  }
}
static void report() {
  printf("sent=%d", g_sent);
  for (int i = 0; i < g_sent; i++) printf(" %08lX", (unsigned long)g_sentFrames[i]);
  printf("\n");
}
int main(int argc, char** argv) {
  const char* c = argv[1];
  otCurrentMode = OTD_MODE_MASTER;
  for (int k = 0; k < OT_SCHEDULE_SIZE; k++) otSchedule[k].disabled = true;
  if (!strcmp(c, "b331")) {           // 09's sequence: writes queued under the simulation, cleared, simulation off
    settings.sat.bSimulation = true; pass(2);
    otCmdEnqueue(TT205); pass(3);
    otCmdEnqueue(CS613); otCmdEnqueue(MM100); pass(5);
    clearWriteOverride(16); clearWriteOverride(1); clearWriteOverride(14);   // TT=0 (master), CS=0, MM=T
    settings.sat.bSimulation = false; pass(6);
  } else if (!strcmp(c, "simonly")) { // not cleared, but queued under the simulation: still not sent after it
    settings.sat.bSimulation = true; pass(2);
    otCmdEnqueue(CS613); pass(3);
    settings.sat.bSimulation = false; pass(6);
  } else if (!strcmp(c, "clear")) {   // no simulation: a write still in the ring is dropped by its clear
    otCmdEnqueue(SW50); otCmdEnqueue(CS613);
    clearWriteOverride(1);
    pass(6);
  } else if (!strcmp(c, "after")) {   // a write queued after the simulation goes out normally, once
    settings.sat.bSimulation = true; pass(2);
    settings.sat.bSimulation = false; pass(1);
    otCmdEnqueue(SW50); pass(6);
  }
  report();
  return 0;
}
"""

TMP = None
EXE = None


def build():
    pre = msq.PRELUDE
    for old, new in ((OLD_SETTINGS, NEW_SETTINGS), (OLD_STATE_TAIL, NEW_STATE_TAIL), (OLD_SEND, NEW_SEND)):
        assert old in pre, old[:60]
        pre = pre.replace(old, new)
    ring_at = SRC.index("static constexpr uint8_t OT_CMD_QUEUE_SIZE")
    ring = SRC[ring_at:SRC.index("// TASK-494: enqueue with coalesce-by-MsgID.", ring_at)]
    parts = [ring, msq.cut_braced(SRC, "static bool otCmdEnqueue(uint32_t frame)")]
    if "static void otdNoteCmdSent(" in SRC:
        parts.append(msq.cut_braced(SRC, "static void otdNoteCmdSent(uint32_t frame, uint32_t now)"))
    parts += [msq.cut_braced(SRC, "static void scheduleMasterRequest()"), msq.cut_braced(SRC, "static void otdFlushCmdQueue()")]
    m = re.search(r"// --- otdSimExitFlush begin\n(.*?)// --- otdSimExitFlush end", SRC, re.S)
    main = MAIN.replace("@@CLEAR@@", msq.cut_braced(SRC, "static void clearWriteOverride(uint8_t msgId)")).replace(
        "@@SIMEXIT@@", m.group(1) if m else "").replace("@@SIMCALL@@", "otdSimExitFlush();" if m else "")
    return pre + "\n".join(parts) + "\n" + main


def setUpModule():
    global TMP, EXE
    TMP = tempfile.mkdtemp(prefix="otd_simexit_")
    cpp = Path(TMP) / "t.cpp"
    cpp.write_text(build(), encoding="utf-8")
    EXE = compile_cpp(cpp, Path(TMP) / ("t.exe" if os.name == "nt" else "t"))
    if EXE is None:
        raise unittest.SkipTest("no host compiler")


def tearDownModule():
    if TMP:
        shutil.rmtree(TMP, ignore_errors=True)


def sent(case):
    out = subprocess.run([str(EXE), case], capture_output=True, text=True, check=True).stdout.splitlines()
    line = [l for l in out if l.startswith("sent=")][-1]
    return line.split()[1:]


class SimExitStaleWrites(unittest.TestCase):
    def test_09s_sequence_sends_no_stale_write(self):
        self.assertEqual(sent("b331"), [], "R90013D4D / R900E6400 / R90101480 after the simulation (bench b331)")

    def test_a_write_queued_under_the_simulation_is_not_sent_after_it(self):
        self.assertEqual(sent("simonly"), [])

    def test_a_clear_drops_the_pending_write_without_a_simulation(self):
        self.assertEqual(sent("clear"), ["10383200"], "SW 50 stays, the cleared CS does not go out")

    def test_a_write_after_the_simulation_goes_out_once(self):
        self.assertEqual(sent("after"), ["10383200"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
