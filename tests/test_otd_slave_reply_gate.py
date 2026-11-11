#!/usr/bin/env python3
"""TASK-1798: an answer OT-Direct makes itself reaches the thermostat although the library's slave gate refuses it at first.

The vendored OpenTherm library refuses a slave send for RESPONSE_TIME (50 ms) after a received frame: process() sets
delayTimestamp = ts + RESPONSE_TIME * 1000 on DATA_READY (OpenTherm.cpp:443), isReady() is false until micros() passes
it (:134-137) and sendResponse() then returns false without sending (:294-312). OT-Direct makes some answers itself in
the same loop pass as the DATA_READY: master mode (handleMasterModeSlaveFrame), UI= Unknown-DataID, SR= READ_ACK and the
alternative-slot Unknown-DataID. Before the fix the refusal was ignored and the thermostat got no answer.

The code under test is REAL: the gateway loop, the alternative slots and the UI=/SR= branches come from the slices of
tests/test_otd_gateway_alt_slots.py, handleMasterModeSlaveFrame() is cut from src/OTGW-firmware/OTDirect.ino, and the
slave side's isReady() / sendResponse() bodies, RESPONSE_TIME and the DATA_READY delay statement are cut from
src/libraries/OpenTherm/src/OpenTherm.cpp and run against a fake micros(). Only the bus wire is a double.

A thermostat frame counts as answered when a reply with its Data-ID leaves within 800 ms (the OpenTherm slave window,
which the library comment at OpenTherm.cpp:8 quotes) and before the next thermostat frame.

Old-vs-fix: OTGW_OTD_INO points at another copy of OTDirect.ino, e.g.
  git show <old-rev>:src/OTGW-firmware/OTDirect.ino > $TMP/OTDirect.ino
  OTGW_OTD_INO=$TMP/OTDirect.ino python tests/test_otd_slave_reply_gate.py
Run: python tests/test_otd_slave_reply_gate.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_otd_gateway_alt_slots as alt  # noqa: E402
from test_sat_pwm_override import compile_cpp  # noqa: E402

OTD_INO = alt.OTD_INO


def gate_class():
    """GateOT: FakeOT plus the library's real slave gate."""
    h = alt.LIB_H.read_text(encoding="utf-8")
    c = alt.LIB_CPP.read_text(encoding="utf-8")
    enums = "".join(alt.construct(h, rf"^enum class {n}\b") + ";\n" for n in ("OpenThermRxStatus", "OpenThermTxStatus"))
    rt = re.search(r"^const uint16_t RESPONSE_TIME = \d+;", c, re.M).group(0)
    delay = re.search(r"^\s*delayTimestamp = ts \+ RESPONSE_TIME \* 1000UL;", c, re.M).group(0).strip()
    is_ready = alt.construct(c, r"^bool IRAM_ATTR OpenTherm::isReady\(\)")
    send = alt.construct(c, r"^bool OpenTherm::sendResponse\(unsigned long request\)")
    body = lambda s: s[s.index("{"):]
    return enums + rt + r"""
static unsigned long micros() { return (unsigned long)g_now * 1000UL; }
static void noInterrupts() {}
static void interrupts() {}
class GateOT : public FakeOT {
public:
  OpenThermRxStatus rxStatus = OpenThermRxStatus::IDLE;
  OpenThermTxStatus txStatus = OpenThermTxStatus::IDLE;
  unsigned long delayTimestamp = 0;
  OpenThermResponseStatus responseStatus = OpenThermResponseStatus::NONE;
  std::vector<uint32_t> sentAt;
  int refused = 0;
  void sendFrame(unsigned long f) { sent.push_back(f); sentAt.push_back(g_now); }
  // the DATA_READY step of OpenTherm::process() for a received thermostat frame
  void received() { const unsigned long ts = micros(); """ + delay + r""" }
  bool isReady() """ + body(is_ready) + r"""
  bool sendResponseLib(unsigned long request) """ + body(send) + r"""
  bool sendResponse(unsigned long request) { bool ok = sendResponseLib(request); if (!ok) refused++; return ok; }
};
"""


MAIN = r"""
@BOILER@
static uint8_t idOf(unsigned long f) { return (uint8_t)((f >> 16) & 0xFF); }

static void resetAll(OTDirectMode mode) {
  otCurrentMode = mode;
  state.otBus.bOnline = true; state.otd.bThermostatConnected = false;
  otMaster = FakeOT(); otSlave = GateOT(); g_log.clear();
  otMasterRequestActive = false; otSlaveFramePending = false; otLastAnySendMs = 0; otScheduleIdx = 0;
  for (uint8_t i = 0; i < OT_SCHEDULE_SIZE; i++) { otSchedule[i].lastSentMs = 0; otSchedule[i].disabled = false; }
  memset(otUnknownCounters, 0, sizeof(otUnknownCounters));
  for (uint8_t i = 0; i < OT_RESPONSE_OVERRIDE_MAX; i++) otResponseOverrides[i].active = false;
  otUnknownIdCount = 0;
  g_now = 1000; g_schedLast = 0;
}

// One thermostat frame per second for `ms`; reports frames of Data-ID `id` and how many got an answer in time.
static void run(const char* name, const std::vector<unsigned long>& stream, uint32_t ms, int watchId) {
  int frames = 0, answered = 0, late = 0, dup = 0;
  int repliesThisFrame = 0;
  size_t pos = 0, seen = 0;
  long pendingAt = -1; int pendingId = -1;
  for (uint32_t end = g_now + ms; g_now < end; g_now += 2) {
    if (g_now % 1000 == 0) {
      if (pendingAt >= 0 && pendingId == watchId) {}   // previous watched frame got no answer: counted below
      pendingAt = -1;
      repliesThisFrame = 0;
      unsigned long f = stream[pos++ % stream.size()];
      otSlave.received();
      handleSlaveRequest(f, OpenThermResponseStatus::SUCCESS);
      state.otd.bThermostatConnected = true;
      if (idOf(f) == watchId) { frames++; pendingAt = (long)g_now; pendingId = watchId; }
    }
    boilerTick();
    loopOTDirect();
    while (seen < otSlave.sent.size()) {
      unsigned long a = otSlave.sent[seen]; uint32_t at = otSlave.sentAt[seen]; seen++;
      if (++repliesThisFrame > 1) dup++;   // one answer per thermostat frame
      if (pendingAt >= 0 && idOf(a) == pendingId) {
        if ((long)at - pendingAt <= 800) answered++; else late++;
        pendingAt = -1;
      }
    }
  }
  printf("%s frames=%d answered=%d late=%d dup=%d refused=%d%c", name, frames, answered, late, dup, otSlave.refused, 10);
}

int main() {
  // G1 alternative slot: the boiler does not support MsgID 127; after three strikes the gateway answers it itself
  { resetAll(OTD_MODE_GATEWAY);
    std::vector<unsigned long> s = { frameOf(0, 0, 0x0200), frameOf(0, 127, 0), frameOf(0, 0, 0x0200), frameOf(0, 25, 0) };
    run("G1", s, 240000, 127); }
  // G2 UI=: MsgID 33 is in the unknown-ID table
  { resetAll(OTD_MODE_GATEWAY); otUnknownIds[otUnknownIdCount++] = 33;
    std::vector<unsigned long> s = { frameOf(0, 0, 0x0200), frameOf(0, 33, 0) };
    run("G2", s, 60000, 33); }
  // G3 SR=: MsgID 26 answered with a stored value
  { resetAll(OTD_MODE_GATEWAY);
    otResponseOverrides[0].active = true; otResponseOverrides[0].msgId = 26; otResponseOverrides[0].value = 0x2A00;
    std::vector<unsigned long> s = { frameOf(0, 0, 0x0200), frameOf(0, 26, 0) };
    run("G3", s, 60000, 26); }
  // G4 master mode: the gateway is the boiler for the thermostat
  { resetAll(OTD_MODE_MASTER);
    std::vector<unsigned long> s = { frameOf(0, 0, 0x0200), frameOf(0, 25, 0) };
    run("G4", s, 60000, 0); }
  // G0 relayed path, for contrast: MsgID 25 goes to the boiler and back (replyToThermostat)
  { resetAll(OTD_MODE_GATEWAY); g_boilerMs = 90;
    std::vector<unsigned long> s = { frameOf(0, 0, 0x0200), frameOf(0, 25, 0) };
    run("G0", s, 60000, 25); g_boilerMs = 40; }
  // G5 stall: a UI= answer is held at the gate, then the loop stalls 900 ms; the thermostat no longer
  // waits, so the answer must not go out late (OT slave window 800 ms)
  { resetAll(OTD_MODE_GATEWAY); otUnknownIds[otUnknownIdCount++] = 33;
    g_now = 2000; otSlave.received(); handleSlaveRequest(frameOf(0, 33, 0), OpenThermResponseStatus::SUCCESS);
    loopOTDirect();
    size_t before = otSlave.sent.size();
    g_now += 900; loopOTDirect(); loopOTDirect();
    printf("G5 held_sent=%d late_sent=%d%c", (int)before, (int)(otSlave.sent.size() - before), 10); }
  return 0;
}
"""


def build_and_run(ino_path):
    src = Path(ino_path).read_text(encoding="utf-8")
    enums, statics, types = alt.lib_and_types()
    pre = alt.PRE.replace("@LIB_ENUMS@", enums).replace("@LIB_STATICS@", statics).replace("@TYPES@", types)
    old = "static FakeOT otMaster, otSlave;"
    assert pre.count(old) == 1
    pre = pre.replace(old, "static FakeOT otMaster;\n" + gate_class() + "static GateOT otSlave;")
    # the boiler model's answer time: alt's 40 ms by default; G0 uses a real round trip (2 x 34 ms frames + 20 ms)
    old_at = "answerAt = g_now + 40;"
    assert pre.count(old_at) == 1
    pre = pre.replace(old_at, "answerAt = g_now + g_boilerMs;").replace(
        "class FakeOT {", "static uint32_t g_boilerMs = 40;\nclass FakeOT {", 1)
    stub = "static void handleMasterModeSlaveFrame(unsigned long) {}\n"
    assert pre.count(stub) == 1
    pre = pre.replace(stub, "static void handleMasterModeSlaveFrame(unsigned long frame);\n")
    boiler = alt.MAIN[alt.MAIN.index("// ---- boiler model"):alt.MAIN.index("// ---- thermostat stream")]
    master = alt.construct(src, r"^static void handleMasterModeSlaveFrame\(unsigned long frame\)\s*\{")
    with tempfile.TemporaryDirectory() as d:
        cpp = Path(d) / "t.cpp"
        cpp.write_text(pre + alt.slices(src) + "\n" + master + "\n" + MAIN.replace("@BOILER@", boiler), encoding="utf-8")
        exe = compile_cpp(cpp, Path(d) / ("t.exe" if os.name == "nt" else "t"))
        if exe is None:
            return None
        return subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout


class SlaveReplyGate(unittest.TestCase):
    rows = None

    @classmethod
    def setUpClass(cls):
        out = build_and_run(OTD_INO)
        if out is None:
            raise unittest.SkipTest("no host compiler")
        print(out, end="")
        cls.rows = {}
        for line in out.splitlines():
            name, _, rest = line.partition(" ")
            cls.rows[name] = {k: int(v) for k, v in (x.split("=") for x in rest.split())}

    def check(self, name, min_frames):
        r = self.rows[name]
        self.assertGreaterEqual(r["frames"], min_frames, r)
        self.assertEqual(r["answered"], r["frames"], f"{name}: a thermostat frame got no answer within 800 ms: {r}")
        self.assertEqual((r["late"], r["dup"]), (0, 0), f"{name}: a late or second answer: {r}")

    def test_G0_relayed_reply_answered(self):
        self.check("G0", 25)

    def test_G1_alternative_slot_answered(self):
        r = self.rows["G1"]
        # the first three 127 reads are relayed (strikes); every later one is the gateway's own answer
        self.check("G1", 50)
        self.assertGreater(r["refused"], 0, "non-vacuous: the gate must refuse the first send in the same pass")

    def test_G2_ui_answered(self):
        self.check("G2", 25)

    def test_G3_sr_answered(self):
        self.check("G3", 25)

    def test_G4_master_mode_answered(self):
        self.check("G4", 25)

    def test_G5_no_answer_after_a_stall(self):
        r = self.rows["G5"]
        self.assertEqual(r["held_sent"], 0, "the answer should have been held at the gate")
        self.assertEqual(r["late_sent"], 0, "a held answer went out 900 ms after the request")


if __name__ == "__main__":
    unittest.main(verbosity=2)
