#!/usr/bin/env python3
"""TASK-1780: a planned reboot tells the broker 'offline' and disconnects cleanly before the restart.

    python tests/test_mqtt_reboot_offline.py                         # the working tree
    OTGW_MQTT_REV=<sha> python tests/test_mqtt_reboot_offline.py     # another revision (old vs fix)

MQTTclient is espMqttClient with UseInternalTask::NO (MQTTstuff.ino), so only an explicit
MQTTclient.loop() sends queued packets. disconnect() only moves the state to disconnectingMqtt1; a later
loop() queues and sends DISCONNECT once the outbox is empty (libraries/espMqttClient/src/MqttClient.cpp,
disconnect() and the disconnectingMqtt1/2 states). doRestart() then sleeps and restarts without a loop()
pass (helperStuff.ino), so on the old code nothing reaches the broker.

doMqttDisconnect() is sliced from the revision's MQTTstuff.ino (from its signature to the closing brace)
and compiled against a stub client that models that state machine: publish() queues a packet,
disconnect() arms the DISCONNECT, loop() sends one queued packet per call and the DISCONNECT only after
the outbox is empty. The stub records what reaches the wire. A clean DISCONNECT suppresses the will, so
the fix must send a retained 'offline' first.
"""
import os
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
#include <string>
#include <vector>
#define PSTR(s) (s)
#define F(s) (s)
#define MQTTDebugTf(...) ((void)0)
#define DebugTf(...) ((void)0)
static uint32_t g_now = 0;
static uint32_t millis() { return g_now; }
static void delay(uint32_t ms) { g_now += ms; }
static char MQTTPubNamespace[64] = "OTGW/value/otgw-test";
static std::vector<std::string> g_wire;   // packets that reached the broker
static bool g_brokerAlive = true;         // false: the TCP send never completes
static int g_loops = 0;
struct ClientStub {
  enum S { CONNECTED, DISC1, DISC2, DISCONNECTED } s = CONNECTED;
  std::vector<std::string> outbox;
  bool connected() const { return s == CONNECTED; }
  bool disconnected() const { return s == DISCONNECTED; }
  uint16_t publish(const char *topic, uint8_t qos, bool retain, const char *payload) {
    if (s != CONNECTED) return 0;
    outbox.push_back(std::string("PUBLISH ") + topic + " " + payload + (retain ? " retain" : "") +
                     " qos" + std::to_string(qos));
    return 1;
  }
  bool disconnect(bool force = false) {
    if (force) { s = DISCONNECTED; return true; }
    if (s != CONNECTED) return false;
    s = DISC1;
    return true;
  }
  void loop() {
    g_loops++;
    g_now += 1;
    if (!g_brokerAlive) return;
    if (s == DISC1 && outbox.empty()) { outbox.push_back("DISCONNECT"); s = DISC2; }
    if (!outbox.empty()) {
      g_wire.push_back(outbox.front());
      if (outbox.front() == "DISCONNECT") s = DISCONNECTED;
      outbox.erase(outbox.begin());
    }
  }
} MQTTclient;

@@SLICE@@

int main(int argc, char **argv) {
  const std::string c = argc > 1 ? argv[1] : "";
  if (c == "dead") g_brokerAlive = false;
  if (c == "notconnected") MQTTclient.s = ClientStub::DISCONNECTED;
  const uint32_t t0 = g_now;
  doMqttDisconnect();
  printf("%s|%u|%d|", c.c_str(), (unsigned)(g_now - t0), g_loops);
  for (size_t i = 0; i < g_wire.size(); i++) printf("%s%s", i ? ";" : "", g_wire[i].c_str());
  printf("\n");
  return 0;
}
"""


class MqttRebootOffline(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        src = read()
        a = src.find("void doMqttDisconnect()")
        b = src.find("\n}\n", a)
        if a < 0 or b < 0:
            raise AssertionError("doMqttDisconnect() not found in MQTTstuff.ino")
        # a comment block directly above the function belongs to it
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text(HARNESS.replace("@@SLICE@@", src[a:b + 3]), encoding="utf-8")
        exe = compile_cpp(cpp, Path(cls.tmp.name) / "t.exe")
        if exe is None:
            raise unittest.SkipTest("no host C++ compiler")
        cls.rows = {}
        for case in ("alive", "dead", "notconnected"):
            out = subprocess.run([str(exe), case], capture_output=True, text=True, check=True).stdout.strip()
            name, ms, loops, wire = out.split("|")
            cls.rows[name] = (int(ms), int(loops), [x for x in wire.split(";") if x])
            print(out)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_offline_retained_then_disconnect_reach_the_broker(self):
        _, _, wire = self.rows["alive"]
        self.assertEqual(wire, ["PUBLISH OTGW/value/otgw-test offline retain qos0", "DISCONNECT"])

    def test_bounded_when_the_broker_never_takes_the_packets(self):
        ms, _, wire = self.rows["dead"]
        self.assertEqual(wire, [])
        self.assertLessEqual(ms, 600, "the pump must give up within about 500 ms")

    def test_nothing_sent_when_not_connected(self):
        ms, loops, wire = self.rows["notconnected"]
        self.assertEqual((wire, loops), ([], 0))
        self.assertLess(ms, 10)


if __name__ == "__main__":
    unittest.main(verbosity=2)
