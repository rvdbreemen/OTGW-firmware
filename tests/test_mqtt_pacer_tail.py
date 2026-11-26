#!/usr/bin/env python3
"""ADR-265 cut 4 (TASK-2949.9): the stale override sweep and the hourly stats ask the shared pacer.

Compiles the REAL gate (PaceGate / paceGateStep()), the REAL staleOverrideClearTick() and the REAL loopMQTTStatsPacer() /
sendMQTTheapdiag() cut out of MQTTstuff.ino, and the REAL mqttPaceAllows(), against stubs for the clock, the MQTT outbox, the
largest free block, state.debug and the sweep / stat publishers. The driver ticks every 10 ms:
  S1  sweep, outbox 4 (above the old limit 2, below the pacer limit 6), 1 s: the pacer lets steps through (about 10, one per 100 ms),
      the old gate none
  S2  sweep, outbox 10 stuck, 12 s: the old gate none; the pacer lets one step per MQTT_PACE_MAX_WAIT_MS (2 in 12 s)
  S3  stats, outbox 10 stuck, 12 s: the old code has no outbox gate (all 26 stats go within 3 s); the pacer lets one per
      MQTT_PACE_MAX_WAIT_MS (2 in 12 s)
  S4  runtime switch off (state.debug.bPaceTail = false): S1 and S3 behave as the old code, in the same binary
  S5  counters: one 'no' per refused step and one 'live' per step the bound let go; nothing counts with the switch off
Red on the base (OTGW_SRC_REV, e.g. e37a6da87): no pacer in the tail producers: S1 sees 0 steps, S3 sees 26 stats.
Mutants (each must turn a test red): pacer always yes; bound removed; switch ignored; counted per call.
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

STATE = r"""
static struct { struct { bool bPaceTail = true; } debug; struct { bool bConnected = true; } mqtt; } state;
static struct { struct { bool bEnable = true; } mqtt; } settings;
enum HeapHealthLevel { HEAP_HEALTHY, HEAP_LOW, HEAP_WARNING, HEAP_CRITICAL };
static HeapHealthLevel getHeapHealth() { return HEAP_HEALTHY; }
static uint8_t sConnStage = 0;
static int g_steps = 0, g_stats = 0;
static bool staleOverrideClearStep() { ++g_steps; return true; }
constexpr uint32_t CONN_STAGE_MAX_OUTBOX = 2;
"""

BASE_STUBS = r"""
static uint32_t ovrPaceNoCount() { return 0; }
static uint32_t ovrPaceLiveCount() { return 0; }
static uint32_t statsPaceNoCount() { return 0; }
static uint32_t statsPaceLiveCount() { return 0; }
"""

MAIN = r"""
static void tick_for(uint32_t ms, bool sweep, bool stats) {
  for (uint32_t t = 0; t < ms; t += 10) {
    if (sweep) staleOverrideClearTick();
    if (stats) loopMQTTStatsPacer();
    g_now += 10;
  }
}
int main() {
  g_maxblk = 100000;
  // S1
  g_outbox = 4; g_steps = 0; g_now = 100000;
  tick_for(1000, true, false);
  printf("s1 steps=%d\n", g_steps);
  // S2: stuck outbox of 10 (counters measured around it)
  g_outbox = 10; g_steps = 0; g_now = 200000;
  { const uint32_t no0 = ovrPaceNoCount(), live0 = ovrPaceLiveCount();
    tick_for(12000, true, false);
    printf("s2 steps=%d\n", g_steps);
    printf("s5_ovr no=%u live=%u\n", (unsigned)(ovrPaceNoCount() - no0), (unsigned)(ovrPaceLiveCount() - live0)); }
  // S3: stats round with the outbox stuck at 10
  g_outbox = 10; g_now = 300000;
  { const uint32_t no0 = statsPaceNoCount(), live0 = statsPaceLiveCount();
    sendMQTTheapdiag();
    tick_for(12000, false, true);
    printf("s3 stats=%d\n", g_stats);
    printf("s5_stats no=%u live=%u\n", (unsigned)(statsPaceNoCount() - no0), (unsigned)(statsPaceLiveCount() - live0)); }
  // S4: the switch off, same binary
  state.debug.bPaceTail = false;
  g_outbox = 4; g_steps = 0; g_now = 400000;
  tick_for(1000, true, false);
  printf("s4_sweep steps=%d\n", g_steps);
  g_outbox = 10; g_stats = 0; g_now = 500000;
  { const uint32_t no0 = statsPaceNoCount(), live0 = statsPaceLiveCount();
    sendMQTTheapdiag();
    tick_for(3000, false, true);
    printf("s4_stats stats=%d\n", g_stats);
    printf("s5_off no=%u live=%u\n", (unsigned)(statsPaceNoCount() - no0), (unsigned)(statsPaceLiveCount() - live0)); }
  return 0;
}
"""


def build(transform=None, tag="x"):
    h, ino = read("SATmqttPublish.h"), read("MQTTstuff.ino")
    a = ino.index("constexpr uint32_t OVR_CLEAR_GAP_MS")
    b = ino.index("// Post-connect pace window (TASK-1757", a)
    sweep = ino[a:b]
    decl = re.search(r"static constexpr uint8_t  STATS_COUNT.*?statsPaceLastMs[^\n]*\n", ino, re.S).group(0)
    stats = (decl + "\nstatic bool publishStatAt(uint8_t) { ++g_stats; return true; }\n"
             + "void sendMQTTheapdiag() " + body(ino, "void sendMQTTheapdiag()") + "\n"
             + "void loopMQTTStatsPacer() " + body(ino, "void loopMQTTStatsPacer()") + "\n")
    has_pacer = "paceGateStep" in sweep
    pacer = "bool mqttPaceAllows() " + body(ino, "bool mqttPaceAllows()") if "mqttPaceAllows" in h else ""
    if "kMqttPaceTailOn" not in h:
        h += "\nstatic constexpr bool kMqttPaceTailOn = false;\n"
    if "MQTT_PACE_MAX_OUTBOX" not in h:
        h += "\nstatic constexpr uint32_t MQTT_PACE_MAX_OUTBOX = 6;\nstatic constexpr uint32_t MQTT_PACE_MIN_BLOCK = 20480;\n"
    text = (strip_includes(h) + "\n" + STATE + pacer + "\n" + ("" if has_pacer else BASE_STUBS) + sweep + "\n" + stats)
    if transform:
        new = transform(text)
        assert new != text, "mutant changed nothing"
        text = new
    tmp = tempfile.TemporaryDirectory()
    src = Path(tmp.name) / f"tail_{tag}.cpp"
    src.write_text(PRELUDE + text + "\n" + MAIN, encoding="utf-8")
    exe = compile_cpp(src, Path(tmp.name) / f"tail_{tag}.exe")
    out = {}
    if exe:
        res = subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout
        for line in res.splitlines():
            name, *rest = line.split()
            out[name] = dict(x.split("=", 1) for x in rest)
    return tmp, out


class TailPacer(unittest.TestCase):
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

    def test_s1_sweep_goes_where_the_old_limit_blocked(self):
        self.need()
        self.assertGreaterEqual(int(self.out["s1"]["steps"]), 8)

    def test_s2_stuck_outbox_sweep_is_let_through_by_the_bound(self):
        self.need()
        self.assertEqual(self.out["s2"]["steps"], "2")

    def test_s3_stats_wait_on_a_stuck_outbox_then_go_by_the_bound(self):
        self.need()
        self.assertEqual(self.out["s3"]["stats"], "2")

    def test_s4_switch_off_is_the_old_behaviour_in_the_same_binary(self):
        self.need()
        self.assertEqual(self.out["s4_sweep"]["steps"], "0", "outbox 4 is above the old limit of 2")
        self.assertEqual(self.out["s4_stats"]["stats"], "26", "the old stats pacer has no outbox gate")

    def test_s5_counters(self):
        self.need()
        if "paceGateStep" not in read("MQTTstuff.ino"):
            self.skipTest("the base has no counters")
        # 12 s with the outbox stuck: two steps let through by the bound (live 2), and the third step is already waiting and
        # counted once (no 3) when the run ends
        self.assertEqual(self.out["s5_ovr"], {"no": "3", "live": "2"})
        self.assertEqual(self.out["s5_stats"], {"no": "3", "live": "2"})
        self.assertEqual(self.out["s5_off"], {"no": "0", "live": "0"})

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
        self.mutant("alwaysyes", tf, lambda o: o["s2"]["steps"] != "2" and o["s3"]["stats"] != "2")

    def test_mutant_no_bound(self):
        self.mutant("nobound", lambda t: t.replace("if ((uint32_t)(now - g.since) >= MQTT_PACE_MAX_WAIT_MS)", "if (false)"),
                    lambda o: o["s2"]["steps"] != "2" and o["s3"]["stats"] != "2")

    def test_mutant_switch_ignored(self):
        self.mutant("switchignored", lambda t: t.replace("kMqttPaceTailOn && state.debug.bPaceTail", "kMqttPaceTailOn"),
                    lambda o: o["s4_sweep"]["steps"] != "0")

    def test_mutant_counted_per_call(self):
        self.mutant("percall", lambda t: t.replace("if (!g.counted) { g.counted = true; ++g.no; }", "++g.no;"),
                    lambda o: o["s5_ovr"]["no"] != "3")


if __name__ == "__main__":
    unittest.main(verbosity=2)
