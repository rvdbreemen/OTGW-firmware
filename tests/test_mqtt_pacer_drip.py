#!/usr/bin/env python3
"""ADR-265 cut 2 (TASK-2949.7): the discovery drip asks the shared pacer.

Compiles the REAL drip gate cut out of MQTTstuff.ino (DRIP_OUTBOX_* constants, dripOutboxWait(), the counters and
dripPaceWait()) and the REAL mqttPaceAllows(), against stubs for the MQTT client queue, the largest free block and
state.debug. dripPaceWait(waits) returns true while the drip tick must be skipped; the harness counts the skipped ticks
before a publish goes out:
  D1  outbox 7 (above the pacer limit 6, below the old limit 8): old code publishes at once (0 waits), the pacer waits 5 ticks
  D2  outbox 10 (above both): both wait 5 ticks, then publish anyway (liveness in ticks, as before the cut)
  D3  largest free block 16000, outbox 0: old code publishes at once, the pacer waits 5 ticks (the new block condition)
  D4  runtime switch off (state.debug.bPaceDrip = false): the old gate, same binary: D1 waits 0 ticks
  D5  counters: D2 with the switch on counts 6 'no' and 1 'liveness'; with the switch off nothing counts
Red on the base (OTGW_SRC_REV, e.g. e37a6da87): the base has no pacer in the drip, D1 sees 0 waits.
Mutants (each must turn a test red): pacer always yes; liveness removed; switch ignored; liveness counter not counted.
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
from test_mqtt_pacer import PRELUDE  # noqa: E402
from test_sat_mqtt_state import body, read, strip_includes  # noqa: E402
from test_sat_published_curve import compile_cpp  # noqa: E402

STATE = "static struct { struct { bool bPaceDrip = true; } debug; } state;\n"

MAIN = r"""
static int waits_before_publish() {
  uint8_t w = 0;
  int ticks = 0;
  while (dripPaceWait(w) && ticks < 30) ++ticks;
  return ticks;
}
int main() {
  g_outbox = 7; g_maxblk = 100000;
  printf("d1 waits=%d\n", waits_before_publish());
  g_outbox = 10;
  const uint32_t no0 = dripPaceNoCount(), live0 = dripPaceLiveCount();
  printf("d2 waits=%d\n", waits_before_publish());
  printf("d5_on no=%u live=%u\n", (unsigned)(dripPaceNoCount() - no0), (unsigned)(dripPaceLiveCount() - live0));
  g_outbox = 0; g_maxblk = 16000;
  printf("d3 waits=%d\n", waits_before_publish());
  state.debug.bPaceDrip = false;
  g_outbox = 7; g_maxblk = 100000;
  printf("d4 waits=%d\n", waits_before_publish());
  g_outbox = 10;
  const uint32_t no1 = dripPaceNoCount(), live1 = dripPaceLiveCount();
  printf("d2_off waits=%d\n", waits_before_publish());
  printf("d5_off no=%u live=%u\n", (unsigned)(dripPaceNoCount() - no1), (unsigned)(dripPaceLiveCount() - live1));
  return 0;
}
"""

BASE_STUBS = """
static uint32_t dripPaceNoCount() { return 0; }
static uint32_t dripPaceLiveCount() { return 0; }
static bool dripPaceWait(uint8_t& w) { return dripOutboxWait(mqttOutboxDepth(), w); }
"""


def build(transform=None, tag="x"):
    h, ino = read("SATmqttPublish.h"), read("MQTTstuff.ino")
    start = ino.index("constexpr uint32_t DRIP_OUTBOX_MAX ")
    end = ino.index("void loopMQTTDiscovery()", start)
    drip = ino[start:end]
    has_pacer = "dripPaceWait" in drip
    pacer = "bool mqttPaceAllows() " + body(ino, "bool mqttPaceAllows()") if "mqttPaceAllows" in h else ""
    if "kMqttPaceDripOn" not in h:
        h += "\nstatic constexpr bool kMqttPaceDripOn = false;\n"
    if "MQTT_PACE_MAX_OUTBOX" not in h:
        h += "\nstatic constexpr uint32_t MQTT_PACE_MAX_OUTBOX = 6;\nstatic constexpr uint32_t MQTT_PACE_MIN_BLOCK = 20480;\n"
    text = strip_includes(h) + "\n" + STATE + pacer + "\n" + drip + ("" if has_pacer else BASE_STUBS)
    if transform:
        new = transform(text)
        assert new != text, "mutant changed nothing"
        text = new
    tmp = tempfile.TemporaryDirectory()
    src = Path(tmp.name) / f"drip_{tag}.cpp"
    src.write_text(PRELUDE + text + "\n" + MAIN, encoding="utf-8")
    exe = compile_cpp(src, Path(tmp.name) / f"drip_{tag}.exe")
    out = {}
    if exe:
        res = subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout
        for line in res.splitlines():
            name, *rest = line.split()
            out[name] = dict(x.split("=", 1) for x in rest)
    return tmp, out


class DripPacer(unittest.TestCase):
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

    def test_d1_pacer_waits_where_the_old_threshold_did_not(self):
        self.need()
        self.assertEqual(self.out["d1"]["waits"], "5")

    def test_d2_deep_outbox_waits_then_publishes_anyway(self):
        self.need()
        self.assertEqual(self.out["d2"]["waits"], "5")

    def test_d3_small_block_waits_the_new_condition(self):
        self.need()
        self.assertEqual(self.out["d3"]["waits"], "5")

    def test_d4_runtime_switch_off_is_the_old_gate_in_the_same_binary(self):
        self.need()
        self.assertEqual(self.out["d4"]["waits"], "0")
        self.assertEqual(self.out["d2_off"]["waits"], "5", "above the old limit of 8 the old gate also waits")

    def test_d5_counters(self):
        self.need()
        if int(self.out["d5_on"]["no"]) == 0 and "dripPaceWait" not in read("MQTTstuff.ino"):
            self.skipTest("the base has no counters")
        self.assertEqual(self.out["d5_on"], {"no": "6", "live": "1"})
        self.assertEqual(self.out["d5_off"], {"no": "0", "live": "0"})

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
        self.mutant("alwaysyes", tf, lambda o: o["d1"]["waits"] != "5" and o["d3"]["waits"] != "5")

    def test_mutant_no_liveness(self):
        self.mutant("noliveness", lambda t: t.replace("if (waits < DRIP_OUTBOX_MAX_WAITS) { waits++; return true; }\n  waits = 0;\n  ++sDripPaceLive;",
                                                      "{ waits++; return true; }\n  waits = 0;\n  ++sDripPaceLive;"),
                    lambda o: o["d2"]["waits"] != "5")

    def test_mutant_switch_ignored(self):
        self.mutant("switchignored", lambda t: t.replace("kMqttPaceDripOn && state.debug.bPaceDrip", "kMqttPaceDripOn"),
                    lambda o: o["d4"]["waits"] != "0")

    def test_mutant_live_counter_not_counted(self):
        self.mutant("nolivecount", lambda t: t.replace("  ++sDripPaceLive;\n", ""),
                    lambda o: o["d5_on"] != {"no": "6", "live": "1"})


if __name__ == "__main__":
    unittest.main(verbosity=2)
