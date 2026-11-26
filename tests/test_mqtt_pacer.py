#!/usr/bin/env python3
"""ADR-265 cut 1 (TASK-2949.6): the SAT shadow gate asks the shared pacer mqttPaceAllows().

Compiles the REAL SATmqttPublish.cpp (the real publishIfChangedF helpers and satOutboxFull()) and the REAL
mqttPaceAllows() cut out of MQTTstuff.ino, against stubs for the MQTT client queue, the largest free block and the clock.
Scenarios (each prints one line, the tests assert on them):
  T1  150 first-seen SAT topics, outbox drained between passes (the loop() flush), no connect window (a takeover):
      packets queued per pass at most MQTT_PACE_MAX_OUTBOX + 1 (old code: up to 25), and every topic still gets published
  T3  outbox stuck at 10 (above the pacer limit, below the old limit of 24): nothing leaves for MQTT_PACE_MAX_WAIT_MS,
      then exactly ONE helper per pass (liveness without a burst)
  T4  kMqttPaceOn = false: the same packets per pass as the code before cut 1 (25)
  T5  largest free block below MQTT_PACE_MIN_BLOCK: the gate defers, the shadow stays first-seen, the flag is set
Red on the base (OTGW_SRC_REV, e.g. origin/dev): the base has no pacer, T1 sees 25 per pass.
Mutants (each must turn a test red): pacer always yes; liveness removed; liveness publishing the whole set.
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
from test_sat_mqtt_state import body, read, strip_includes  # noqa: E402
from test_sat_published_curve import compile_cpp  # noqa: E402

PRELUDE = r"""
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdint>
class __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper*>(s))
#define PSTR(s) (s)
#define snprintf_P snprintf
static uint32_t g_now = 1000;
static uint32_t millis() { return g_now; }
static char* dtostrf(double v, signed char w, unsigned char p, char* b) { sprintf(b, "%*.*f", w, p, v); return b; }
static size_t test_strlcpy(char* d, const char* s, size_t n) {
  size_t l = strlen(s); if (n) { size_t c = l < n - 1 ? l : n - 1; memcpy(d, s, c); d[c] = 0; } return l; }
#define strlcpy test_strlcpy
static uint32_t platformHardwareRandom() { return 12345u; }
static uint32_t g_outbox = 0;
static uint32_t g_maxblk = 100000;
static int g_sent = 0;
static bool g_stuck = false;       // the outbox does not grow with our publishes (it is held up elsewhere)
struct { uint32_t queueSize() const { return g_outbox; } } MQTTclient;
static uint32_t platformMaxFreeBlock() { return g_maxblk; }
uint32_t mqttOutboxDepth() { return g_outbox; }
static bool g_paceHold = false;
bool mqttConnectPaceHold() { return g_paceHold; }
bool sendMQTTData(const char* topic, const char* json, const bool retain) {
  (void)topic; (void)json; (void)retain; ++g_sent; if (!g_stuck) ++g_outbox; return true; }
bool sendMQTTData(const __FlashStringHelper* topic, const char* json, const bool retain) {
  return sendMQTTData(reinterpret_cast<const char*>(topic), json, retain); }
"""

MAIN = r"""
#include <string>
static const int N = 150;
static SATShadowF g_sh[N];
static std::string g_topic[N];
// one pass = what satPublishMQTT() does: clear the flag, then offer every topic to its helper
static int one_pass() {
  satPublishDeferredClear();
  int sent = 0;
  for (int i = 0; i < N; ++i) if (publishIfChangedF(g_topic[i].c_str(), 1.0f, g_sh[i], 0.05f, 1, false)) ++sent;
  return sent;
}
int main() {
  for (int i = 0; i < N; ++i) g_topic[i] = "sat/t" + std::to_string(i);
  // T1
  int maxpp = 0, passes = 0, total = 0;
  while (passes < 400) {
    int s = one_pass(); ++passes; total += s; if (s > maxpp) maxpp = s;
    g_outbox = 0;          // loop() flushed the outbox
    g_now += 500;
    if (total >= N) break;
  }
  printf("t1 max_per_pass=%d passes=%d total=%d\n", maxpp, passes, total);
  // T3: stuck outbox of 10: above the pacer limit, below SAT_MQTT_OUTBOX_LIMIT
  for (int i = 0; i < N; ++i) g_sh[i] = SATShadowF{};
  g_outbox = 10; g_stuck = true; g_now = 1000;
  printf("t3");
  for (int p = 0; p < 14; ++p) { printf(" %d", one_pass()); g_now += 500; }
  printf("\n");
  g_stuck = false;
  // T5: free block below the pacer minimum
  for (int i = 0; i < N; ++i) g_sh[i] = SATShadowF{};
  g_outbox = 0; g_maxblk = MQTT_PACE_MIN_BLOCK - 1; g_now = 1000; g_sent = 0;
  int s5 = one_pass();
  printf("t5 sent=%d valid=%d deferred=%d\n", s5, (int)g_sh[0].valid, (int)satPublishDeferred());
  g_maxblk = 100000; g_outbox = 0;
  int s5b = one_pass();
  printf("t5b sent=%d valid=%d\n", s5b, (int)g_sh[0].valid);
  return 0;
}
"""


def build(transform=None, tag="x"):
    h, cpp, ino = read("SATmqttPublish.h"), read("SATmqttPublish.cpp"), read("MQTTstuff.ino")
    rev = os.environ.get("OTGW_SRC_REV")
    has_pacer = "mqttPaceAllows" in h
    if has_pacer:
        pacer = "bool mqttPaceAllows() " + body(ino, "bool mqttPaceAllows()")
    else:  # the base: no pacer. Define the constants the harness prints, unused by the code under test.
        h += ("\nstatic constexpr uint32_t MQTT_PACE_MAX_OUTBOX = 6;\nstatic constexpr uint32_t MQTT_PACE_MIN_BLOCK = 20480;\n")
        pacer = ""
    text = strip_includes(h) + "\n" + pacer + "\n" + strip_includes(cpp)
    if transform:
        new = transform(text)
        assert new != text, "mutant changed nothing"
        text = new
    tmp = tempfile.TemporaryDirectory()
    src = Path(tmp.name) / f"pacer_{tag}.cpp"
    src.write_text(PRELUDE + text + "\n" + MAIN, encoding="utf-8")
    exe = compile_cpp(src, Path(tmp.name) / f"pacer_{tag}.exe")
    out = {}
    if exe:
        res = subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout
        for line in res.splitlines():
            name, *rest = line.split()
            out[name] = rest
    return tmp, out


def kv(items):
    return dict(x.split("=", 1) for x in items)


class Pacer(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp, cls.out = build(tag="fix")
        print("\n" + "\n".join(f"{k} {' '.join(v)}" for k, v in cls.out.items()))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def need(self):
        if not self.out:
            self.skipTest("no host C++ compiler")

    def test_t1_packets_per_pass_are_bounded_and_nothing_is_dropped(self):
        self.need()
        t1 = kv(self.out["t1"])
        self.assertLessEqual(int(t1["max_per_pass"]), 6 + 1, "outbox limit of the pacer plus the one that crosses it")
        self.assertEqual(int(t1["total"]), 150, "every topic is published in the end")

    def test_t3_stuck_outbox_liveness_is_one_helper_per_pass(self):
        self.need()
        sent = [int(x) for x in self.out["t3"]]
        # the wait starts at the first pass (t=1000); 5000 ms later is the 11th pass: nothing before, then exactly one per pass
        self.assertEqual(sent[:10], [0] * 10, sent)
        self.assertTrue(all(s == 1 for s in sent[10:]), sent)

    def test_t5_small_block_defers_and_keeps_the_shadow(self):
        self.need()
        t5 = kv(self.out["t5"])
        self.assertEqual((t5["sent"], t5["valid"], t5["deferred"]), ("0", "0", "1"))
        t5b = kv(self.out["t5b"])
        self.assertGreater(int(t5b["sent"]), 0)

    # --- T4 and the mutants: each builds the code again -----------------------------------------
    def test_t4_switch_off_is_the_old_behaviour(self):
        tmp, out = build(lambda t: t.replace("kMqttPaceOn           = true", "kMqttPaceOn           = false"), "off")
        try:
            if not out:
                self.skipTest("no host C++ compiler")
            self.assertEqual(kv(out["t1"])["max_per_pass"], "25", "SAT_MQTT_OUTBOX_LIMIT 24 + the one that crosses it")
            sent = [int(x) for x in out["t3"]]
            self.assertEqual(sent[0], 150, "stuck outbox of 10 is under the old limit of 24: the old gate lets the whole set through")
        finally:
            tmp.cleanup()

    def mutant(self, name, transform, check):
        tmp, out = build(transform, name)
        try:
            if not out:
                self.skipTest("no host C++ compiler")
            self.assertTrue(check(out), f"mutant {name} still passes: {out}")
        finally:
            tmp.cleanup()

    def test_mutant_pacer_always_yes_breaks_t1_and_t5(self):
        def tf(t):
            return re.sub(r"(bool mqttPaceAllows\(\)\s*\{).*?\n\}", r"\1 return true; }", t, count=1, flags=re.S)
        self.mutant("alwaysyes", tf, lambda o: int(kv(o["t1"])["max_per_pass"]) > 7 and kv(o["t5"])["sent"] != "0")

    def test_mutant_no_liveness_breaks_t3(self):
        self.mutant("noliveness", lambda t: t.replace("!s_satPaceForcedThisPass && ", "false && "),
                    lambda o: all(int(x) == 0 for x in o["t3"]))

    def test_mutant_liveness_publishes_the_whole_set_breaks_t3(self):
        self.mutant("wholeset", lambda t: t.replace("      s_satPaceForcedThisPass = true;\n", ""),
                    lambda o: any(int(x) > 1 for x in o["t3"]))


if __name__ == "__main__":
    unittest.main(verbosity=2)
