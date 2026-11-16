#!/usr/bin/env python3
"""TASK-2004: the 26 hourly otgw-firmware/stats/* topics go out paced, not back to back.

    python tests/test_mqtt_stats_pacer.py                        # the working tree
    OTGW_MQTT_REV=<sha> python tests/test_mqtt_stats_pacer.py    # another revision (old vs fix)

Bench 09 on .61 (no PSRAM), 2026-11-16 16:00: the free heap fell from 63,396 to 6,484 B inside the hourly burst of
26 retained publishes, the only publishes in that second. The fix starts a round in sendMQTTheapdiag() and sends one
stat per loop pass, at least 100 ms apart, from loopMQTTStatsPacer(); a stat waits while the heap is CRITICAL.

publishStatU32(), sendMQTTheapdiag() and (on the fix) publishStatAt() / loopMQTTStatsPacer() are sliced from the
revision's MQTTstuff.ino and compiled against stubs: sendMQTTData() records (time, topic, payload), getHeapHealth()
and the connection state are set by the driver, millis() is a fake clock.
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_published_curve import compile_cpp  # noqa: E402

REPO = Path(__file__).resolve().parent.parent
MQ = "src/OTGW-firmware/MQTTstuff.ino"
REV = os.environ.get("OTGW_MQTT_REV")


def read():
    if REV is None:
        return (REPO / MQ).read_text(encoding="utf-8").replace("\r\n", "\n")
    return subprocess.run(["git", "-C", str(REPO), "show", f"{REV}:{MQ}"], capture_output=True, text=True,
                          encoding="utf-8", check=True).stdout.replace("\r\n", "\n")


def slice_code(src):
    a = src.index("static void publishStatU32(")
    b = src.index("\n}\n", a) + 3
    stat = src[a:b]
    start = src.find("// TASK-2004: the 26 hourly stats")
    if start < 0:
        start = src.index("void sendMQTTheapdiag(){")
    end = src.index("// ADR-084: OT-bus presence values", start)
    body = src[start:end]
    if "loopMQTTStatsPacer" not in body:
        body += "\nvoid loopMQTTStatsPacer() {}   // not in this revision\n"
    return stat + "\n" + body


HARNESS = r"""
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#define PSTR(s) (s)
#define F(s) (s)
typedef const char __FlashStringHelper;
#define snprintf_P snprintf
static uint32_t g_now = 0;
static uint32_t millis() { return g_now; }
enum HeapHealthLevel { HEAP_HEALTHY, HEAP_LOW, HEAP_WARNING, HEAP_CRITICAL };
static HeapHealthLevel g_health = HEAP_HEALTHY;
static HeapHealthLevel getHeapHealth() { return g_health; }
struct Pub { uint32_t t; std::string topic; std::string payload; };
static std::vector<Pub> g_pubs;
static int g_dropped = 0;
static bool sendMQTTData(const char *topic, const char *json, bool retain) {
  if (g_health == HEAP_CRITICAL) { g_dropped++; return false; }   // canPublishMQTT() inside sendMQTTData
  (void)retain; g_pubs.push_back({g_now, topic, json}); return true;
}
static uint32_t platformFreeHeap() { return 60000; }
static uint32_t platformMaxFreeBlock() { return 30000; }
static uint32_t getHeapFragmentation() { return 10; }
static uint32_t getMinFreeHeap() { return 20000; }
struct { struct { bool bEnable = true; } mqtt; } settings;
struct {
  struct { bool bConnected = true; } mqtt;
  struct { uint32_t iWsDropsTotal = 1, iMqttDropsTotal = 2, iEnteredLowCount = 3, iEnteredWarningCount = 4,
           iEnteredCriticalCount = 5, iDripActiveBurstSkipCount = 6, iDripCooldownSkipCount = 7, iDripSlowModeCount = 8,
           iMinMaxBlock = 9, iMaxLoopGapMs = 10; uint32_t aMaxBlockBucket[5] = {11, 12, 13, 14, 15}; } heapdiag;
  struct { uint32_t iVerifyRunCount = 16, iRepublishTriggeredCount = 17, iLastMissingCount = 18, iLastOrphanCount = 19,
           iPublishedTopicCount = 20, iLastVerifyEpoch = 21, iLastDailyHealEpoch = 22; } discovery;
} state;
"""

MAIN = r"""
static void dump(const char *tag) {
  printf("%s %d %d\n", tag, (int)g_pubs.size(), g_dropped);
  for (auto &p : g_pubs) printf("P %u %s %s\n", (unsigned)p.t, p.topic.c_str(), p.payload.c_str());
  printf("END\n");
}
// Loop passes every 10 ms for `ms` milliseconds.
static void run(uint32_t ms) { for (uint32_t t = 0; t < ms; t += 10) { loopMQTTStatsPacer(); g_now += 10; } }
int main(int argc, char **argv) {
  std::string c = argc > 1 ? argv[1] : "";
  g_now = 3600000;
  if (c == "start") { sendMQTTheapdiag(); dump("call"); return 0; }
  if (c == "round") { sendMQTTheapdiag(); run(5000); dump("round"); return 0; }
  if (c == "critical") {                       // CRITICAL from the start for 1 s, then healthy
    g_health = HEAP_CRITICAL; sendMQTTheapdiag(); run(1000); dump("crit");
    g_health = HEAP_HEALTHY; run(5000); dump("after"); return 0;
  }
  if (c == "disconnect") {                     // the broker drops mid-round: the rest waits for the next hour
    sendMQTTheapdiag(); run(500); state.mqtt.bConnected = false; run(1000);
    state.mqtt.bConnected = true; run(5000); dump("disc"); return 0;
  }
  if (c == "idle") { run(5000); dump("idle"); return 0; }
  return 2;
}
"""


def parse(out):
    blocks, cur = {}, None
    for line in out.splitlines():
        if line == "END":
            cur = None
            continue
        if line.startswith("P ") and cur is not None:
            _, t, topic, payload = line.split(" ", 3)
            blocks[cur]["pubs"].append((int(t), topic, payload))
            continue
        tag, n, dropped = line.split()
        cur = tag
        blocks[tag] = {"n": int(n), "dropped": int(dropped), "pubs": []}
    return blocks


class StatsPacer(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        src = read()
        cls.expected = re.findall(r'F\("(otgw-firmware/stats/[a-z0-9_]+)"\)', src[src.index("static void publishStatU32("):
                                                                             src.index("// ADR-084: OT-bus presence values")])
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text(HARNESS + slice_code(src) + MAIN, encoding="utf-8")
        cls.exe = compile_cpp(cpp, Path(cls.tmp.name) / "t.exe")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_case(self, case):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        r = subprocess.run([str(self.exe), case], capture_output=True, text=True, timeout=60)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        return parse(r.stdout)

    def test_all_26_topics_exist(self):
        self.assertEqual(len(self.expected), 26)
        self.assertEqual(len(set(self.expected)), 26)

    def test_the_hourly_call_sends_at_most_one_stat(self):
        b = self.run_case("start")["call"]
        self.assertLessEqual(b["n"], 1, f"sendMQTTheapdiag() published {b['n']} topics in one call (burst)")

    def test_a_round_sends_every_topic_once_in_order_at_least_100_ms_apart(self):
        pubs = self.run_case("round")["round"]["pubs"]
        self.assertEqual([p[1] for p in pubs], self.expected)
        gaps = [b[0] - a[0] for a, b in zip(pubs, pubs[1:])]
        self.assertTrue(all(g >= 100 for g in gaps), f"gaps {gaps}")
        self.assertLessEqual(pubs[-1][0] - pubs[0][0], 3000, "the round ends within ~3 s")
        self.assertEqual(dict((p[1], p[2]) for p in pubs)["otgw-firmware/stats/maxblock_ge16k"], "15")

    def test_critical_heap_makes_the_round_wait_instead_of_dropping(self):
        r = self.run_case("critical")
        self.assertEqual(r["crit"]["n"], 0)
        self.assertEqual(r["crit"]["dropped"], 0, "a stat must wait, not be dropped by canPublishMQTT()")
        self.assertEqual([p[1] for p in r["after"]["pubs"]], self.expected)

    def test_a_disconnect_ends_the_round(self):
        d = self.run_case("disconnect")["disc"]
        self.assertGreater(d["n"], 0)
        self.assertLess(d["n"], 26, "the rest of the round is not sent after a reconnect")
        self.assertEqual([p[1] for p in d["pubs"]], self.expected[:d["n"]])

    def test_nothing_is_sent_without_an_hourly_call(self):
        self.assertEqual(self.run_case("idle")["idle"]["n"], 0)


if __name__ == "__main__":
    unittest.main()
