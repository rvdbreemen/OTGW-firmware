#!/usr/bin/env python3
"""TASK-1778: a SUBSCRIBE that espMqttClient refuses at connect is retried while connected.

    python tests/test_mqtt_subscribe_retry.py                         # the working tree
    OTGW_MQTT_REV=<sha> python tests/test_mqtt_subscribe_retry.py     # another revision (old vs fix)

espMqttClient refuses every packet while max(largest heap block, largest PSRAM block) is below
EMC_MIN_FREE_MEMORY = 16384 B (Helpers.h, Packets/Packet.cpp); subscribe() then returns packet id 0. A board without
PSRAM reaches that in the connect burst (TASK-1757: 4.2 KB). onMqttConnect() subscribed the command topic and
homeassistant/status once; with a clean session a refusal left every MQTT command dead until the next reconnect.

Sliced from the revision's src/OTGW-firmware/MQTTstuff.ino: the subscribe block of onMqttConnect() (from
"//Subscribe to topics" to the post-subscribe heap line) and, when present, the TASK-1778 helper block. Code without
the helper (the old code) gets an empty retry: nothing retried there. MQTTclient.subscribe() is a stub that refuses
while the modelled heap is low and records what got subscribed.

Run: python tests/test_mqtt_subscribe_retry.py
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


HARNESS = r"""
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#define PSTR(s) (s)
#define F(s) (s)
#define MQTTDebugTf(...) ((void)0)
#define MQTT_TOPIC_MAX_LEN 128
#define strlcpy otgw_test_strlcpy
#define strlcat otgw_test_strlcat
static size_t strlcpy(char *d, const char *s, size_t n) { size_t l = strlen(s); if (n) { size_t c = l < n - 1 ? l : n - 1; memcpy(d, s, c); d[c] = 0; } return l; }
static size_t strlcat(char *d, const char *s, size_t n) { size_t l = strlen(d); return l + strlcpy(d + l, s, n > l ? n - l : 0); }
static char MQTTSubNamespace[64] = "OTGW/set/otgw-test";
static uint32_t g_now = 0;
static uint32_t millis() { return g_now; }
static bool g_lowHeap = false;
static int g_attempts = 0, g_errors = 0;
static std::set<std::string> g_subscribed;
struct ClientStub {
  uint16_t subscribe(const char *topic, int) {
    g_attempts++;
    if (g_lowHeap) return 0;            // espMqttClient: packet not allocated, id 0
    g_subscribed.insert(topic);
    return 1;
  }
} MQTTclient;
static void PrintMQTTError() { g_errors++; }

@@HELPER@@

static void connectSubscribe() {
@@BLOCK@@
}

static void run(bool lowAtConnect, int lowSecondsAfter, int seconds) {
  g_lowHeap = lowAtConnect;
  connectSubscribe();
  for (int t = 0; t < seconds * 10; t++) {   // the connected-state loop, every 100 ms
    g_now += 100;
    if (t / 10 >= lowSecondsAfter) g_lowHeap = false;
    mqttRetryPendingSubscribes();
  }
}

int main(int argc, char **argv) {
  const std::string c = argc > 1 ? argv[1] : "";
  if (c == "refused") run(true, 3, 10);        // refused at connect, heap back after 3 s
  else if (c == "ok") run(false, 0, 10);       // accepted at connect
  else if (c == "long") run(true, 1000, 10);   // refused for the whole 10 s
  printf("%s|%d|%d|%d|%d\n", c.c_str(), (int)g_subscribed.count("OTGW/set/otgw-test/#"),
         (int)g_subscribed.count("homeassistant/status"), g_attempts, g_errors);
  return 0;
}
"""


class MqttSubscribeRetry(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        src = read()
        a = src.find("  //Subscribe to topics")
        b = src.find("  DebugTf(PSTR(\"[HEAP] post-subscribe", a)
        if a < 0 or b < 0:
            raise AssertionError("subscribe block of onMqttConnect() not found")
        h1, h2 = src.find("// TASK-1778 begin"), src.find("// TASK-1778 end")
        helper = src[h1:h2] if h1 >= 0 and h2 > h1 else "static void mqttRetryPendingSubscribes() {}   // old: no retry"
        cls.src = src
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text(HARNESS.replace("@@HELPER@@", helper).replace("@@BLOCK@@", src[a:b]), encoding="utf-8")
        exe = compile_cpp(cpp, Path(cls.tmp.name) / "t.exe")
        if exe is None:
            raise unittest.SkipTest("no host C++ compiler")
        cls.rows = {}
        for case in ("refused", "ok", "long"):
            out = subprocess.run([str(exe), case], capture_output=True, text=True, check=True).stdout.strip()
            name, cmd, ha, attempts, errors = out.split("|")
            cls.rows[name] = (int(cmd), int(ha), int(attempts), int(errors))
            print(out)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_refused_at_connect_is_retried_once_the_heap_recovers(self):
        cmd, ha, _, _ = self.rows["refused"]
        self.assertEqual((cmd, ha), (1, 1), "command topic / homeassistant/status not subscribed after the heap came back")

    def test_accepted_at_connect_needs_no_retry(self):
        cmd, ha, attempts, errors = self.rows["ok"]
        self.assertEqual((cmd, ha, attempts, errors), (1, 1, 2, 0))

    def test_retry_is_bounded_to_one_attempt_per_second_per_topic(self):
        cmd, ha, attempts, _ = self.rows["long"]
        self.assertEqual((cmd, ha), (0, 0))
        self.assertLessEqual(attempts, 2 + 2 * 10, "more than one attempt per second per topic")

    @unittest.skipIf(REV is not None, "wiring is checked on the working tree")
    def test_retry_runs_in_the_connected_state(self):
        i = self.src.find("case MQTT_STATE_IS_CONNECTED:")
        j = self.src.find("case ", i + 10)
        self.assertIn("mqttRetryPendingSubscribes();", self.src[i:j])
        self.assertIn("static void mqttRetryPendingSubscribes();", self.src[:i], "no forward declaration before use")


if __name__ == "__main__":
    unittest.main(verbosity=2)
