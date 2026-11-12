"""TASK-1757: the MQTT connect burst leaves in portions, and the discovery drip waits on a deep outbox.

The boot connect trace (beta.171) showed the heap fall 30-50 KB while the ~17 packets queued inside
onMqttConnect() were transmitted in one go; with a browser loading the UI at the same time it reached
2.9 KB, below CRITICAL. This test cuts the REAL onMqttConnect(), the connStage block and
dripOutboxWait() out of src/OTGW-firmware/MQTTstuff.ino, compiles them on the host with stubs that
count queued packets, and checks:
  1. onMqttConnect() queues only the birth and the two command subscriptions (3 packets);
  2. the rest follows in four portions, each only after the previous one left the outbox, the gap
     passed and the largest block is clear of espMqttClient's 16 KB floor;
  3. a stuck outbox or low heap delays a portion, but after the max wait it goes anyway;
  4. a disconnect stops the portions and a new connect starts them again from the first;
  5. unsupported_msgids is published before its dirty flag is cleared;
  6. the drip skips at most 5 ticks on an outbox deeper than 8, then publishes.
On the code from before TASK-1757 check 1 fails (all ~33 packets are queued inside onMqttConnect).
Set OTGW_MQTT_INO to test another copy of MQTTstuff.ino.
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
import test_sat_pwm_override as base  # noqa: E402  (compile_cpp: g++, clang++ or MSVC)

INO = Path(os.environ.get("OTGW_MQTT_INO", HERE.parent / "src" / "OTGW-firmware" / "MQTTstuff.ino"))

STUBS = r"""
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#define F(s) s
#define PSTR(s) s
static uint32_t nowMs = 1000;
static uint32_t millis() { return nowMs; }
static uint32_t maxBlock = 60000;
static uint32_t platformFreeHeap() { return 100000; }
static uint32_t platformMaxFreeBlock() { return maxBlock; }
static void DebugTf(const char*, ...) {}
static void MQTTDebugTf(const char*, ...) {}
static void MQTTDebugln(const char*) {}
static void MQTTDebugTln(const char*) {}
static void Debugln(const char*) {}
static void DebugTln(const char*) {}
static size_t strlcpy(char* d, const char* s, size_t n) { size_t l = strlen(s); if (n) { size_t c = l < n - 1 ? l : n - 1; memcpy(d, s, c); d[c] = 0; } return l; }
static size_t strlcat(char* d, const char* s, size_t n) { size_t l = strlen(d); return l + strlcpy(d + l, s, n > l ? n - l : 0); }
#define MQTT_TOPIC_MAX_LEN 128
#define MQTT_REPUBLISH_OFFLINE_THRESHOLD_MS 600000UL
static char MQTTSubNamespace[64] = "OTGW/set/otgw";
// packets: queued counts every packet handed to the client, sent counts the ones the test lets leave
static int queued = 0, sent = 0, published[8] = {0}, dirtyClearedAfterCsv = -1;
struct Client {
  bool up = true;
  bool connected() { return up; }
  bool subscribe(const char*, int) { queued++; return true; }
  size_t queueSize() { return (size_t)(queued - sent); }
} MQTTclient;
static void q(int kind, int n) { queued += n; published[kind] += n; }
static void publishBirthOnline() { q(0, 1); }
static void mqttV2MigrationOnConnect() { q(1, 6); }
static void sendMQTTversioninfo() { q(2, 6); }
static void publishAllPICsettings() { q(3, 15); }
static void publishCommandDroppedState() { q(4, 1); }
static void publishBoilerUnsupportedMsgids() { q(5, 1); }
static void clearBoilerUnsupportedDirty() { dirtyClearedAfterCsv = published[5] > 0 ? 1 : 0; }
static void requestMQTTRepublishAll() {}
static void clearMQTTConfigDone() {}
static void clearMQTTConfigPending() {}
static void publishNonOTDiscoveryConfigs() {}
static void PrintMQTTError() {}
static void bootHeapPhase(const char*) {}
static void connTraceBegin() {}
static void connTraceSample() {}
enum { CPROBE_CONN_BIRTH = 14, CPROBE_CONN_SUBS = 15 };  // TASK-1757 AC3 probes (MQTTstuff.h)
static void connProbe(uint8_t) {}
static void heapBurstOpen() {}
enum { BOOTHEAP_RUN = 1 };
static int sBootHeapState = 0;
static int8_t reconnectAttempts = 0;
enum states_of_MQTT { MQTT_STATE_INIT, MQTT_STATE_IS_CONNECTED };
static states_of_MQTT stateMQTT = MQTT_STATE_INIT;
struct { struct { bool bConnected; uint32_t iLastConnectedMs; } mqtt; } state = {{false, 0}};
"""

MAIN = r"""
static void tick() { connStageTick(); }
int main(int argc, char** argv) {
  const char* mode = argc > 1 ? argv[1] : "flow";
  if (!strcmp(mode, "connect")) {            // what onMqttConnect queues by itself
    onMqttConnect(false);
    printf("queued=%d\n", queued);
    return 0;
  }
  if (!strcmp(mode, "flow")) {               // healthy: outbox drains after each portion
    onMqttConnect(false);
    printf("c=%d", queued);
    for (int i = 0; i < 60; i++) { nowMs += 50; sent = queued; int before = queued; tick(); if (queued != before) printf(" t%u=%d", nowMs - 1000, queued - before); }
    printf(" total=%d dirty=%d\n", queued, dirtyClearedAfterCsv);
    return 0;
  }
  if (!strcmp(mode, "stuck")) {              // outbox never drains, heap low: portions wait the max, then go
    maxBlock = 10000;
    onMqttConnect(false);
    int firstAt = -1;
    for (int i = 0; i < 500; i++) { nowMs += 50; int before = queued; tick(); if (queued != before && firstAt < 0) firstAt = (int)(nowMs - 1000); }
    printf("first=%d total=%d\n", firstAt, queued);
    return 0;
  }
  if (!strcmp(mode, "gap")) {                // outbox drained at once: the gap still spaces the portions
    onMqttConnect(false);
    int n = 0; uint32_t last = 0, minGap = 99999;
    for (int i = 0; i < 400; i++) { nowMs += 10; sent = queued; int before = queued; tick();
      if (queued != before) { if (n) { uint32_t g = nowMs - last; if (g < minGap) minGap = g; } last = nowMs; n++; } }
    printf("portions=%d mingap=%u\n", n, minGap);
    return 0;
  }
  if (!strcmp(mode, "reconnect")) {          // drop after the first portion; a new connect restarts
    onMqttConnect(false);
    nowMs += 300; sent = queued; tick();     // portion 1
    MQTTclient.up = false; nowMs += 300; tick();
    int afterDrop = queued;
    for (int i = 0; i < 20; i++) { nowMs += 300; sent = queued; tick(); }
    int whileDown = queued - afterDrop;
    MQTTclient.up = true; queued = sent = 0; for (int &p : published) p = 0;
    onMqttConnect(false);
    for (int i = 0; i < 20; i++) { nowMs += 300; sent = queued; tick(); }
    printf("whiledown=%d migr=%d ver=%d pic=%d total=%d\n", whileDown, published[1], published[2], published[3], queued);
    return 0;
  }
  if (!strcmp(mode, "drip")) {               // outbox 20 deep: skip 5, publish on the 6th; shallow: never skip
    uint8_t w = 0; int seq = 0;
    for (int i = 0; i < 7; i++) seq = seq * 2 + (dripOutboxWait(20, w) ? 1 : 0);
    uint8_t w2 = 0; int shallow = 0;
    for (int i = 0; i < 7; i++) shallow += dripOutboxWait(8, w2) ? 1 : 0;
    printf("seq=%d shallow=%d\n", seq, shallow);
    return 0;
  }
  return 2;
}
"""


def load(path: Path) -> str:
    return path.read_text(encoding="utf-8").replace("\r\n", "\n")


def cut_func(src: str, signature: str) -> str:
    i = src.index(signature)
    j = src.index("{", i)
    depth = 0
    for k in range(j, len(src)):
        if src[k] == "{":
            depth += 1
        elif src[k] == "}":
            depth -= 1
            if depth == 0:
                return src[i:k + 1]
    raise AssertionError("unbalanced " + signature)


def program(src: str) -> str:
    m = re.search(r"// --- connStage \(TASK-1757\).*?// --- end connStage", src, re.S)
    # the code from before TASK-1757 has no stages: an empty tick keeps it compilable for the red run
    stage = m.group(0) if m else "static void connStageTick() {}\nstatic void connStageReset() {}\n"
    stage = stage.replace("static void mqttV2MigrationOnConnect();\n", "")
    drip = ""
    if "static bool dripOutboxWait(" in src:
        d = re.search(r"constexpr uint32_t DRIP_OUTBOX_MAX .*?\n(?=\n)", src, re.S)
        drip = d.group(0) if d else ""
    else:
        drip = "static bool dripOutboxWait(uint32_t, uint8_t&) { return false; }\n"
    return STUBS + stage + "\n" + drip + "\n" + cut_func(src, "static void onMqttConnect(bool sessionPresent) {") + MAIN


class MqttConnectStaging(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "staging.cpp"
        cpp.write_text(program(load(INO)), encoding="utf-8")
        cls.exe = base.compile_cpp(cpp, Path(cls.tmp.name) / ("staging" + (".exe" if os.name == "nt" else "")))
        if cls.exe is None:
            raise unittest.SkipTest("no host C++ compiler (g++, clang++ or MSVC) found")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_mode(self, mode):
        out = subprocess.run([str(self.exe), mode], capture_output=True, text=True, check=True).stdout.strip()
        return dict(kv.split("=", 1) for kv in out.split())

    def test_connect_queues_only_birth_and_command_subscriptions(self):
        self.assertEqual(int(self.run_mode("connect")["queued"]), 3)

    def test_rest_follows_in_four_portions_once_the_outbox_drains(self):
        r = self.run_mode("flow")
        self.assertEqual(int(r["c"]), 3)
        portions = [int(v) for k, v in r.items() if k[0] == "t" and k[1:].isdigit()]
        self.assertEqual(portions, [6, 6, 15, 2], r)        # migration, version, PIC settings, dropped + CSV
        self.assertEqual(int(r["total"]), 32)
        self.assertEqual(r["dirty"], "1", "unsupported_msgids published before its dirty flag clears")

    def test_portions_keep_the_gap(self):
        r = self.run_mode("gap")
        self.assertEqual(int(r["portions"]), 4)
        self.assertGreaterEqual(int(r["mingap"]), 200)

    def test_stuck_outbox_and_low_heap_delay_but_never_drop(self):
        r = self.run_mode("stuck")
        self.assertGreaterEqual(int(r["first"]), 5000)
        self.assertEqual(int(r["total"]), 32)

    def test_disconnect_stops_and_reconnect_restarts(self):
        r = self.run_mode("reconnect")
        self.assertEqual(int(r["whiledown"]), 0)
        self.assertEqual((int(r["migr"]), int(r["ver"]), int(r["pic"]), int(r["total"])), (6, 6, 15, 32), r)

    def test_drip_waits_at_most_five_ticks_on_a_deep_outbox(self):
        r = self.run_mode("drip")
        self.assertEqual(int(r["seq"]), 0b1111101, r)   # skip x5, publish, skip again (a new wait starts)
        self.assertEqual(int(r["shallow"]), 0)


if __name__ == "__main__":
    unittest.main()
