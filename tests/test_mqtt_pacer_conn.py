#!/usr/bin/env python3
"""ADR-265 cut 3 (TASK-2949.8): the connect portions ask the shared pacer.

Compiles the REAL connect-stage gate cut out of MQTTstuff.ino (CONN_STAGE_* constants, connStageReady(), the counters and
connStagePaceReady()) and the REAL mqttPaceAllows(), against stubs for the clock, the MQTT outbox, the largest free block and
state.debug. connStagePaceReady(now) says whether the next portion may go; the harness advances the clock in 50 ms steps from
the connect and reports after how many ms a portion goes:
  C1  outbox 0, block ok: ready after the 200 ms gap
  C2  outbox 4 (above the old limit 2, below the pacer limit 6): the old condition stays a hard AND (architect review of cut 3:
      no gate lets more through than before), so the portion waits for the 5 s bound, as the old gate did; the pacer only holds back
  C3  outbox 10: both wait for the 5 s bound, then publish anyway (counters: 1 'no', 1 'live' per portion)
  C4  largest free block 16000, outbox 0: both wait for the 5 s bound
  C5  runtime switch off (state.debug.bPaceConn = false): the old gate in the same binary: C2 waits 5 s
  C6  the counters count once per portion, not per call, and nothing counts with the switch off
Red on fa28bac49 (the first cut 3, before the review): C2 saw 200 ms because the pacer replaced the old outbox limit.
Mutants (each must turn a test red): pacer always yes (counters); bound removed; old condition removed.
"""
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from test_mqtt_pacer import PRELUDE  # noqa: E402
from test_sat_mqtt_state import body, read, strip_includes  # noqa: E402
from test_sat_published_curve import compile_cpp  # noqa: E402

STATE = ("static struct { struct { bool bPaceConn = true; } debug; } state;\n"
         "static void startStaleOverrideClear() {}\n")

MAIN = r"""
// ms after the connect at which the portion may go (cap 6000)
static int ready_after_ms(bool useGate = true) {
  g_now = 1000;
  connStageStart();
  for (int ms = 0; ms <= 6000; ms += 50) {
    g_now = 1000 + ms;
    const bool r = useGate ? connStagePaceReady(g_now) : connStageReady(g_now, g_outbox, g_maxblk);
    if (r) return ms;
  }
  return 9999;
}
int main() {
  g_maxblk = 100000; g_outbox = 0;
  printf("c1 ms=%d\n", ready_after_ms());
  g_outbox = 4;
  printf("c2 ms=%d\n", ready_after_ms());
  state.debug.bPaceConn = false;   // the switch off: the old gate decides (what connStageTick() does)
  printf("c5 ms=%d\n", ready_after_ms(false));
  state.debug.bPaceConn = true;
  g_outbox = 10;
  const uint32_t no0 = connPaceNoCount(), live0 = connPaceLiveCount();
  printf("c3 ms=%d\n", ready_after_ms());
  printf("c6_one no=%u live=%u\n", (unsigned)(connPaceNoCount() - no0), (unsigned)(connPaceLiveCount() - live0));
  g_outbox = 0; g_maxblk = 16000;
  printf("c4 ms=%d\n", ready_after_ms());
  return 0;
}
"""

BASE_STUBS = """
static uint32_t connPaceNoCount() { return 0; }
static uint32_t connPaceLiveCount() { return 0; }
static bool connStagePaceReady(uint32_t nowMs) { return connStageReady(nowMs, g_outbox, g_maxblk); }
"""


def build(transform=None, tag="x"):
    h, ino = read("SATmqttPublish.h"), read("MQTTstuff.ino")
    start = ino.index("constexpr uint32_t CONN_STAGE_MAX_OUTBOX")
    end = ino.index("static void connStageTick()", start)
    conn = ino[start:end]
    has_pacer = "connStagePaceReady" in conn
    pacer = "bool mqttPaceAllows() " + body(ino, "bool mqttPaceAllows()") if "mqttPaceAllows" in h else ""
    if "MQTT_PACE_MAX_OUTBOX" not in h:
        h += "\nstatic constexpr uint32_t MQTT_PACE_MAX_OUTBOX = 6;\nstatic constexpr uint32_t MQTT_PACE_MIN_BLOCK = 20480;\n"
    text = strip_includes(h) + "\n" + STATE + pacer + "\n" + conn + ("" if has_pacer else BASE_STUBS)
    if transform:
        new = transform(text)
        assert new != text, "mutant changed nothing"
        text = new
    tmp = tempfile.TemporaryDirectory()
    src = Path(tmp.name) / f"conn_{tag}.cpp"
    src.write_text(PRELUDE + text + "\n" + MAIN, encoding="utf-8")
    exe = compile_cpp(src, Path(tmp.name) / f"conn_{tag}.exe")
    out = {}
    if exe:
        res = subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout
        for line in res.splitlines():
            name, *rest = line.split()
            out[name] = dict(x.split("=", 1) for x in rest)
    return tmp, out


class ConnPacer(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp, cls.out = build(tag="fix")
        print("\n" + "\n".join(f"{k} {v}" for k, v in cls.out.items()))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def need(self):
        if not self.out:
            self.skipTest("no host C++ compiler")

    def test_c1_empty_outbox_goes_after_the_gap(self):
        self.need()
        self.assertEqual(self.out["c1"]["ms"], "200")

    def test_c2_the_old_condition_stays_a_hard_and(self):
        self.need()
        self.assertEqual(self.out["c2"]["ms"], "5000", "outbox 4 is above the old limit of 2: not earlier than before")

    def test_c3_deep_outbox_waits_for_the_bound(self):
        self.need()
        self.assertEqual(self.out["c3"]["ms"], "5000")

    def test_c4_small_block_waits_for_the_bound(self):
        self.need()
        self.assertEqual(self.out["c4"]["ms"], "5000")

    def test_c5_switch_off_is_the_old_gate(self):
        self.need()
        self.assertEqual(self.out["c5"]["ms"], "5000", "outbox 4 is above the old limit of 2")

    def test_c6_counters_once_per_portion(self):
        self.need()
        if "connStagePaceReady" not in read("MQTTstuff.ino"):
            self.skipTest("the base has no counters")
        self.assertEqual(self.out["c6_one"], {"no": "1", "live": "1"})

    def mutant(self, name, transform, check):
        tmp, out = build(transform, name)
        try:
            if not out:
                self.skipTest("no host C++ compiler")
            self.assertTrue(check(out), f"mutant {name} still passes: {out}")
        finally:
            tmp.cleanup()

    def test_mutant_pacer_always_yes(self):
        def tf(t):
            return re.sub(r"(bool mqttPaceAllows\(\)\s*\{).*?\n\}", r"\1 return true; }", t, count=1, flags=re.S)
        self.mutant("alwaysyes", tf, lambda o: o["c6_one"] != {"no": "1", "live": "1"})

    def test_mutant_no_bound(self):
        self.mutant("nobound", lambda t: t.replace("if (waited >= CONN_STAGE_MAX_WAIT_MS) {\n    if (!mqttPaceAllows())",
                                                   "if (false) {\n    if (!mqttPaceAllows())"),
                    lambda o: o["c3"]["ms"] != "5000")

    def test_mutant_old_condition_removed(self):
        self.mutant("noold", lambda t: t.replace("if (!connStageReady(nowMs, (uint32_t)MQTTclient.queueSize(), platformMaxFreeBlock())) return false;", ""),
                    lambda o: o["c2"]["ms"] != "5000")


if __name__ == "__main__":
    unittest.main(verbosity=2)
