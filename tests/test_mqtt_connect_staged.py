#!/usr/bin/env python3
"""TASK-1757 (Robert: options C + B): the MQTT connect burst leaves in small portions.

Background (TASK-1757 AC2, bench .61 beta.171, connect-window trace): onMqttConnect queued the
whole connect burst at once (birth, 2 command SUBSCRIBEs, 6 V2-migration SUBSCRIBEs, 6
version-info publishes, PIC settings, command_retry_exceeded, unsupported_msgids: 17 packets on
OT-Direct). Queueing cost ~4 KB, but transmitting all of it in the next 50-150 ms cost 30-50 KB
of free heap; with two browser tabs loading at the same moment the heap fell to 2,916 B
(below CRITICAL 3,072 B).

Checks:
  1. Source: onMqttConnect() itself only sends the birth and the two command SUBSCRIBEs and then
     calls mqttConnectStageStart(); it no longer calls mqttV2MigrationOnConnect(),
     sendMQTTversioninfo(), publishAllPICsettings(), publishCommandDroppedState() or
     publishBoilerUnsupportedMsgids(). handleMQTT() calls mqttConnectStageTick() while connected.
     discoveryCanPublishNow() consults discoveryOutboxAllows().
  2. Behaviour, host-compiled: the REAL mqttConnectStageStart/Tick and discoveryOutboxAllows cut
     from MQTTstuff.ino, with the client, the clock and the step functions stubbed:
     - the four steps run in order, one per tick, each only once the outbox is at <= 2 packets;
     - a step still runs after 3 s when the outbox stays high (no stall);
     - a disconnect stops the sequence;
     - the drip gate refuses while the outbox holds more than 8 packets, allows at <= 8, and
       allows again after 10 s of continuous waiting.
     Skipped when no host C++ compiler is found.

OTGW_MQTT_INO points at another copy (e.g. `git show HEAD~1:...`) for old-vs-fix: on the old code
check 1 fails (onMqttConnect calls the follow-up directly) and check 2 cannot find the functions.
Not checked: the heap effect on the device (bench: TASK-1757 runner, W boots old vs fix).
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402

MQTT_INO = Path(os.environ.get("OTGW_MQTT_INO", REPO / "src" / "OTGW-firmware" / "MQTTstuff.ino"))


def body_of(src: str, signature: str) -> str:
    """The text of a function from its signature up to the matching closing brace."""
    m = re.search(re.escape(signature) + r"\s*\{", src)   # the definition, not a forward declaration
    if m is None:
        raise AssertionError(f"definition not found: {signature}")
    i, j = m.start(), m.end() - 1
    depth = 0
    for k in range(j, len(src)):
        if src[k] == "{":
            depth += 1
        elif src[k] == "}":
            depth -= 1
            if depth == 0:
                return src[i:k + 1]
    raise AssertionError(f"unbalanced: {signature}")


def strip_comments(s: str) -> str:
    s = re.sub(r"/\*.*?\*/", "", s, flags=re.S)
    return re.sub(r"//[^\n]*", "", s)


class SourceOrder(unittest.TestCase):
    def setUp(self):
        self.src = MQTT_INO.read_text(encoding="utf-8")

    def test_connect_handler_sends_only_birth_and_command_subscribes(self):
        b = strip_comments(body_of(self.src, "static void onMqttConnect(bool sessionPresent)"))
        self.assertIn("publishBirthOnline()", b)
        self.assertIn('MQTTclient.subscribe("homeassistant/status"', b)
        self.assertIn("mqttConnectStageStart()", b)
        for staged in ("mqttV2MigrationOnConnect()", "sendMQTTversioninfo()", "publishAllPICsettings()",
                       "publishCommandDroppedState()", "publishBoilerUnsupportedMsgids()"):
            self.assertNotIn(staged, b, f"{staged} must leave in a later loop pass, not inside the connect callback")

    def test_refused_drip_publish_is_retried_not_dropped(self):
        # The gate only makes discoveryCanPublishNow() return false; the drip call site then
        # returns without publishing and the same slot is tried on the next tick.
        self.assertRegex(strip_comments(self.src), r"if \(gate != nullptr && !discoveryCanPublishNow\(\)\) return false;")
        self.assertIn("discoveryOutboxWaitReset()", strip_comments(body_of(self.src, "static void onMqttDisconnect(espMqttClientTypes::DisconnectReason reason)")))

    def test_handle_mqtt_runs_the_stage_tick_and_drip_gate_checks_outbox(self):
        self.assertIn("mqttConnectStageTick();", strip_comments(body_of(self.src, "void handleMQTT()")))
        self.assertIn("discoveryOutboxAllows()", strip_comments(body_of(self.src, "static bool discoveryCanPublishNow()")))


HARNESS = r"""
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#define PSTR(x) x
static uint32_t g_ms = 1000;
static uint32_t millis() { return g_ms; }
static uint32_t g_depth = 0;
static bool g_conn = true;
uint32_t mqttOutboxDepth() { return g_depth; }
struct Client { bool connected() { return g_conn; } } MQTTclient;
static std::string g_log;
static void mqttV2MigrationOnConnect() { g_log += "M"; }
static void sendMQTTversioninfo() { g_log += "V"; }
static void publishAllPICsettings() { g_log += "P"; }
static void publishCommandDroppedState() { g_log += "C"; }
static void publishBoilerUnsupportedMsgids() { g_log += "U"; }
static void clearBoilerUnsupportedDirty() { }
static uint32_t platformFreeHeap() { return 50000; }
static uint32_t platformMaxFreeBlock() { return 30000; }
#define DebugTf(...) do {} while (0)
static void connTraceSample() { }
%(GATE)s
%(STAGE)s
static int fails = 0;
#define CHECK(c, m) do { if (!(c)) { printf("FAIL %%s\n", m); fails++; } } while (0)
int main() {
  // staged steps: wait while the outbox is above 2, one step per tick, in order
  mqttConnectStageStart();
  g_depth = 17; mqttConnectStageTick(); g_ms += 10; mqttConnectStageTick();
  CHECK(g_log == "", "no step while the outbox holds 17");
  g_depth = 2; g_ms += 10; mqttConnectStageTick();
  CHECK(g_log == "M", "step 1 at depth 2");
  g_depth = 8; g_ms += 10; mqttConnectStageTick();
  CHECK(g_log == "M", "step 2 waits while depth 8");
  g_depth = 1; g_ms += 10; mqttConnectStageTick(); mqttConnectStageTick(); mqttConnectStageTick(); mqttConnectStageTick();
  CHECK(g_log == "MVPCU", "steps 2..4 one per tick, then idle");
  // bounded wait: a step still runs after 3 s with a high outbox
  g_log.clear(); mqttConnectStageStart(); g_depth = 20; g_ms += 2999; mqttConnectStageTick();
  CHECK(g_log == "", "still waiting at 2999 ms");
  g_ms += 1; mqttConnectStageTick();
  CHECK(g_log == "M", "step 1 after 3000 ms even with depth 20");
  // a disconnect stops the sequence
  g_conn = false; g_depth = 0; g_ms += 10; mqttConnectStageTick(); g_conn = true; mqttConnectStageTick();
  CHECK(g_log == "M", "no steps after a disconnect until the next connect");
  // drip gate: > 8 refuses, <= 8 allows; after 10 s of continuous waiting ONE publish is let
  // through and the wait starts again (the drip still sends: slowed to one per 10 s, not stalled)
  g_depth = 9;  CHECK(!discoveryOutboxAllows(), "depth 9 refused");
  g_ms += 9999; CHECK(!discoveryOutboxAllows(), "still refused at 9999 ms");
  g_ms += 1;    CHECK(discoveryOutboxAllows(), "one publish let through after 10 s (sends anyway)");
  g_ms += 1;    CHECK(!discoveryOutboxAllows(), "re-armed: refused again right after the release");
  g_ms += 9999; CHECK(discoveryOutboxAllows(), "next release 10 s after the previous one");
  g_depth = 8;  CHECK(discoveryOutboxAllows(), "depth 8 allowed");
  g_depth = 9; g_ms += 5; CHECK(!discoveryOutboxAllows(), "wait restarts after an allowed check");
  // a wait from before a disconnect does not carry over: reset on connect/disconnect
  g_ms += 20000; discoveryOutboxWaitReset();
  CHECK(!discoveryOutboxAllows(), "after a reset the first high-outbox check waits again");
  mqttConnectStageStart(); CHECK(!discoveryOutboxAllows(), "mqttConnectStageStart resets the wait");
  printf(fails ? "FAILED %%d\n" : "OK\n", fails);
  return fails ? 1 : 0;
}
"""


class Behaviour(unittest.TestCase):
    def test_real_stage_and_gate_logic(self):
        src = MQTT_INO.read_text(encoding="utf-8")
        a = src.find("static const uint32_t MQTT_CONNECT_STAGE_MAX_DEPTH")
        self.assertGreaterEqual(a, 0, "staged connect follow-up not found (old code?)")
        stage = src[a:src.index("}", src.index("sConnStageStep = (sConnStageStep >= 4)")) + 1]
        g = src.find("static const uint32_t DISCOVERY_OUTBOX_MAX ")
        self.assertGreaterEqual(g, 0, "drip outbox gate not found (old code?)")
        gate = src[g:src.index("static bool discoveryCanPublishNow()", g)]
        with tempfile.TemporaryDirectory() as d:
            cpp = Path(d) / "t.cpp"
            cpp.write_text(HARNESS % {"STAGE": stage, "GATE": gate}, encoding="utf-8")
            exe = compile_cpp(cpp, Path(d) / "t.exe")
            if exe is None:
                self.skipTest("no host C++ compiler")
            r = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            self.assertIn("OK", r.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
