#!/usr/bin/env python3
"""TASK-1759 option A (ADR-224): OT-Direct gateway mode reads MsgID 28 in a supported thermostat slot.

Sergeant D entries 287 and 291, Robert entry 290, ADR-224 (Accepted, scope OT-Direct only). With a thermostat
that never reads MsgID 28 and asks only IDs the boiler supports (tjfs 2026-11-09: 0, 1, 16, 17, 24, 25), the
TASK-1707 alternative slots never come, so the gateway never read the return temperature. Now, every 30 s,
while SAT is on and no valid MsgID 28 arrived for 30 s, the gateway takes the slot of one supported thermostat
READ (the candidate), reads MsgID 28 there and answers the thermostat READ_ACK from the boiler cache.

The real code is cut from src/OTGW-firmware/OTDirect.ino through the TASK-1707 harness
(tests/test_otd_gateway_alt_slots.py: same slices, fake bus, boiler model). The boiler model stamps feedTret
on a MsgID 28 READ_ACK, as processOT() does for a 'B' frame (OTGW-Core.ino, TASK-1412.5 feed block).

Cases (OTGW_OTD_INO = another copy for old-vs-fix; the old code has no slot, so R1, R6 and R8 fail there):
  R1  tjfs stream, SAT on, 6 min: boiler frames == thermostat frames; MsgID 28 about every 30 s (11-13);
      every thermostat frame answered; replaced frames are READ_DATA of the candidate only, answered
      READ_ACK with the cached value; never MsgID 0, a write, 9 or 100.
  R2  SAT off: no MsgID 28.            R3  MsgID 28 fresh (the first 25 s): nothing replaced.
  R4  MsgID 28 unsupported: no slot.   R5  SAT simulation on: no slot.
  R6  the only eligible read comes every 40 s (slower than the interval): never replaced.
  R7  an unsupported thermostat ID gives TASK-1707 slots: they still happen (checked first); the cache slot
      only fills in on candidate reads, at most once per 30 s.
  R8  ID 25's boiler reads keep their gap under SAT_FLOW_VALID_MS (60 s).
  R9  candidates 17/25 turn Data-Invalid at 100 s: no cached READ_ACK carries the invalid value (audit 1795 E).
Not checked: the real bus timing, processOT() itself, a real thermostat's reaction to a cached answer
(field capture, tjfs; the bench has no thermostat in gateway mode, TASK-1707 AC4).
"""
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_otd_gateway_alt_slots as h  # noqa: E402
from test_sat_pwm_override import compile_cpp  # noqa: E402

MAIN = r"""
static void reset28(bool satOn) {
  resetAll();
  state.otBus.feedTret.sampleMs = 0; state.sat.bActive = satOn; settings.sat.bSimulation = false;
  g_satEnabled = satOn; g_ret28Unsupported = false; g_invalidFrom = 0;
#ifdef HAS_RET
  otdRetCandidateId = 0; otdRetCandidateMs = 0; otdRetLastSlotMs = 0;
#endif
}
static void tjfsStream() {   // tjfs 2026-11-09: 0, 1 (write), 16 (write), 17, 24 (write), 25; all supported, no 28
  const unsigned ids[][3] = { {0,0,0x0200}, {1,1,0x2800}, {0,0,0x0200}, {0,25,0}, {0,0,0x0200}, {0,17,0},
                              {0,0,0x0200}, {1,16,0x1400}, {0,0,0x0200}, {1,24,0x1499}, {0,0,0x0200}, {0,25,0} };
  g_stream.clear(); for (auto& e : ids) g_stream.push_back(frameOf(e[0], e[1], (uint16_t)e[2]));
}
static void report(const char* name, const Run& r) {
  int r28 = 0, cached = 0, cachedBad = 0, first28 = -1;
  for (unsigned long q : otMaster.sent) if (idOf(q) == 28) r28++;
  // a cached answer is logged 'A' right after its 'T' frame; the relayed boiler path logs 'B' for that ID
  for (size_t i = 1; i < g_log.size(); i++) {
    if (g_log[i].prefix != 'A' || g_log[i - 1].prefix != 'T') continue;
    unsigned long t = g_log[i - 1].frame, a = g_log[i].frame;
    if (typeOf(a) != 4) continue;                       // TASK-1707 slots answer Unknown-DataID (7)
    cached++;
    bool ok = typeOf(t) == 0 && idOf(t) == idOf(a) && idOf(t) != 0 && idOf(t) != 9 && idOf(t) != 100
              && (a & 0xFFFF) == (uint16_t)(0x1000 + idOf(t)) && !OpenTherm::parity(a);
    if (!ok) cachedBad++;
  }
  // R8: the largest gap between two boiler reads of ID 25 (ms)
  long last25 = -1, gap25 = 0; uint32_t t0 = 0;
  (void)t0;
  printf("%s tframes=%d bframes=%d replies=%d r28=%d cached=%d cachedbad=%d slots=%d\n", name, r.tframes, r.bframes,
         (int)otSlave.sent.size(), r28, cached, cachedBad, r.altSlots);
  (void)last25; (void)gap25; (void)first28;
}
// R8: the largest gap between two boiler reads of ID 25 over the whole run (ms), from the send times
static long maxGap25() {
  long last = -1, gap = 0;
  for (size_t i = 0; i < otMaster.sent.size(); i++) if (idOf(otMaster.sent[i]) == 25) {
    long at = (long)otMaster.sentAt[i];
    if (last >= 0 && at - last > gap) gap = at - last;
    last = at;
  }
  return gap;
}
int main() {
  reset28(true);  tjfsStream(); report("R1", simulate(true, 360000));
  reset28(false); tjfsStream(); report("R2", simulate(true, 360000));
  reset28(true);  tjfsStream(); state.otBus.feedTret.sampleMs = g_now; report("R3", simulate(true, 25000));
  reset28(true);  tjfsStream(); g_ret28Unsupported = true; report("R4", simulate(true, 360000));
  reset28(true);  tjfsStream(); settings.sat.bSimulation = true; report("R5", simulate(true, 360000));
  { reset28(true);   // R6: the one eligible read (17) once per 40 frames = 40 s
    g_stream.clear(); g_stream.push_back(frameOf(0, 17, 0));
    for (int i = 0; i < 39; i++) g_stream.push_back(i % 2 ? frameOf(1, 1, 0x2800) : frameOf(0, 0, 0x0200));
    report("R6", simulate(true, 360000)); }
  { reset28(true);   // R7: tjfs stream plus MsgID 127 (unsupported by the boiler model): TASK-1707 slots
    tjfsStream(); g_stream.push_back(frameOf(0, 127, 0)); report("R7", simulate(true, 360000)); }
  { reset28(true); tjfsStream(); simulate(true, 360000); printf("R8 gap25=%ld\n", maxGap25()); }
  { reset28(true); tjfsStream(); g_invalidFrom = 100000;   // R9: candidates 17/25 turn Data-Invalid at 100 s
    report("R9", simulate(true, 360000)); }
  return 0;
}
"""


def build_and_run(ino_path):
    src = Path(ino_path).read_text(encoding="utf-8")
    enums, statics, types = h.lib_and_types()
    pre = h.PRE.replace("@LIB_ENUMS@", enums).replace("@LIB_STATICS@", statics).replace("@TYPES@", types)
    harness_main = h.MAIN[:h.MAIN.index("int main() {")]
    flag = "#define HAS_RET 1\n" if "// --- otdRetSlot begin" in src else ""
    with tempfile.TemporaryDirectory() as d:
        cpp = Path(d) / "r.cpp"
        cpp.write_text(flag + pre + h.slices(src) + harness_main + MAIN, encoding="utf-8")
        exe = compile_cpp(cpp, Path(d) / ("r.exe" if os.name == "nt" else "r"))
        if exe is None:
            return None
        return subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout


class ReturnTempSlot(unittest.TestCase):
    rows = None

    @classmethod
    def setUpClass(cls):
        out = build_and_run(h.OTD_INO)
        if out is None:
            return
        print(out, end="")
        cls.rows = {}
        for line in out.splitlines():
            name, _, rest = line.partition(" ")
            cls.rows[name] = {k: int(v) for k, v in (x.split("=") for x in rest.split())}

    def setUp(self):
        if self.rows is None:
            self.skipTest("no host C++ compiler")

    def test_R1_one_frame_per_thermostat_frame_and_28_every_30s(self):
        r = self.rows["R1"]
        self.assertEqual(r["bframes"], r["tframes"], r)
        self.assertEqual(r["replies"], r["tframes"], "every thermostat frame gets one answer: %r" % r)
        # 30 s interval, then the next candidate read (ID 25 every ~6 s in this stream): about every 30-36 s
        self.assertGreaterEqual(r["r28"], 9, r)
        self.assertLessEqual(r["r28"], 12, r)

    def test_R1_replaced_frames_are_candidate_reads_answered_from_the_cache(self):
        r = self.rows["R1"]
        self.assertGreaterEqual(r["cached"], 9, "the cache slot must be used about every 30 s: %r" % r)
        self.assertEqual(r["cached"], r["r28"], "one cached answer per MsgID 28 read: %r" % r)
        self.assertEqual(r["cachedbad"], 0, "a cached answer for MsgID 0, a write, 9/100, or wrong data: %r" % r)

    def test_R2_sat_off_reads_nothing(self):
        self.assertEqual((self.rows["R2"]["r28"], self.rows["R2"]["cached"]), (0, 0))

    def test_R3_fresh_return_temperature_reads_nothing(self):
        self.assertEqual((self.rows["R3"]["r28"], self.rows["R3"]["cached"]), (0, 0))

    def test_R4_unsupported_28_reads_nothing(self):
        self.assertEqual((self.rows["R4"]["r28"], self.rows["R4"]["cached"]), (0, 0))

    def test_R5_sat_simulation_reads_nothing(self):
        self.assertEqual((self.rows["R5"]["r28"], self.rows["R5"]["cached"]), (0, 0))

    def test_R6_a_read_slower_than_the_interval_is_never_replaced(self):
        self.assertEqual((self.rows["R6"]["r28"], self.rows["R6"]["cached"]), (0, 0))
        self.assertEqual(self.rows["R6"]["bframes"], self.rows["R6"]["tframes"])

    def test_R7_unsupported_slots_stay_slots_and_the_cache_slot_only_fills_in(self):
        # The TASK-1707 slots are checked first and still happen; the schedule shares them with ~20 entries,
        # so MsgID 28 is not fresh from them alone and the cache slot fills in, on candidate reads only.
        r = self.rows["R7"]
        self.assertGreater(r["slots"], 0, r)
        self.assertEqual(r["bframes"], r["tframes"], r)
        self.assertEqual(r["cachedbad"], 0, r)
        self.assertLessEqual(r["cached"], 12, r)

    def test_R9_a_data_invalid_reply_stops_cached_answers_for_that_id(self):
        # Audit 1795 area E, note 3: a Data-Invalid reply overwrites the cached value; only the age stamp
        # protects the thermostat. After 100 s the boiler answers 17 and 25 Data-Invalid 0xDEAD: no cached
        # READ_ACK may carry that value (cachedbad counts answers whose value is not the boiler's real one).
        r = self.rows["R9"]
        self.assertEqual(r["cachedbad"], 0, r)
        self.assertGreater(r["cached"], 0, r)   # the slot did work during the first 100 s

    def test_R8_flow_reads_keep_their_gap_under_60s(self):
        self.assertLess(self.rows["R8"]["gap25"], 60000, self.rows["R8"])
        self.assertGreater(self.rows["R8"]["gap25"], 0, self.rows["R8"])


if __name__ == "__main__":
    unittest.main()
