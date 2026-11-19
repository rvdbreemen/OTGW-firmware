#!/usr/bin/env python3
"""TASK-1991: a retained <label>/override topic left from before a reboot is cleared after the next MQTT connect.

The ADR-118 override store is RAM only: after a reboot it is empty, so publishActiveOverrides() (TASK-1985) never
clears a topic that was retained before the reboot, and HA keeps showing that old value.

REAL code from src/OTGW-firmware/OTGW-Core.ino: recordOTOverride(), publishActiveOverrides(), startStaleOverrideClear()
and staleOverrideClearStep(); OTOverrideEntry_t and the OVERRIDE_* constants from OTGW-Core.h. OTmap is a stub with the
real shape (index = msg id): TSet (1) and Toutside (27) are f88, SConfig (3) a flag8, the rest undefined. A stub broker
keeps retained topics: a publish with a payload stores it, an empty one removes it. The connect path is driven as
MQTTstuff.ino does: startStaleOverrideClear() at connect, then one staleOverrideClearStep() per paced pass, and
publishActiveOverrides() on its 60 s timer.
Old code (no sweep): OTGW_CORE_INO=<older OTGW-Core.ino> OTGW_CORE_H=<older OTGW-Core.h> python tests/test_mqtt_override_reboot_clear.py
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402
from test_mqtt_override_retained_clear import func  # noqa: E402

REPO = Path(__file__).resolve().parent.parent
FW = REPO / "src" / "OTGW-firmware"
CORE = Path(os.environ.get("OTGW_CORE_INO", FW / "OTGW-Core.ino")).read_text(encoding="utf-8").replace("\r\n", "\n")
HDR = Path(os.environ.get("OTGW_CORE_H", FW / "OTGW-Core.h")).read_text(encoding="utf-8").replace("\r\n", "\n")

PRE = r"""
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#define PSTR(s) (s)
#define snprintf_P snprintf
#define OT_MSGID_MAX 133
static uint32_t g_now = 1000;
static uint32_t millis() { return g_now; }
static struct { struct { bool bEnable = true; } mqtt; } settings;
enum OTtype_t { ot_f88, ot_s16, ot_s8s8, ot_u16, ot_u8u8, ot_flag8, ot_flag8flag8, ot_special, ot_flag8u8, ot_u8, ot_undef };
struct OTlookup_t { int id; int msgcmd; OTtype_t type; const char* label; const char* friendlyname; const char* unit; bool bSlaveEchoesValue; };
static OTlookup_t OTmap[OT_MSGID_MAX + 1];
#define PROGMEM_readAnything(src, dst) memcpy(&(dst), (src), sizeof(dst))
enum OTLibMessageID { OT_DUMMY = 0 };
static const char* messageIDToString(OTLibMessageID id) { return OTmap[(int)id].label; }
static void dtostrf(float v, int, int, char* out) { snprintf(out, 15, "%.2f", v); }
static void feedWatchDog() {}
static std::map<std::string, std::string> g_broker;   // retained topics
static int g_pubs = 0;
static bool sendMQTTData(const char* topic, const char* payload, bool retain) {
  g_pubs++;
  if (retain) { if (payload[0]) g_broker[topic] = payload; else g_broker.erase(topic); }
  return true;
}
"""

MAIN = r"""
OTOverrideEntry_t otOverrideStore[OVERRIDE_STORE_MAX] = {};
@@FUNCS@@
static void initMap() {
  for (int i = 0; i <= OT_MSGID_MAX; i++) OTmap[i] = {i, 0, ot_undef, "", "", "", true};
  OTmap[1]  = {1, 0, ot_f88, "TSet", "", "", true};
  OTmap[3]  = {3, 0, ot_flag8, "SConfig", "", "", true};
  OTmap[27] = {27, 0, ot_f88, "Toutside", "", "", true};
}
// MQTT connect as MQTTstuff.ino runs it: the sweep starts at connect, one step per pass (100 ms), the override
// refresh on its 60 s timer.
static void connectAndRun(uint32_t ms) {
#ifdef HAS_SWEEP
  startStaleOverrideClear();
#endif
  for (uint32_t t = 0; t < ms; t += 100) {
    g_now += 100;
#ifdef HAS_SWEEP
    staleOverrideClearStep();
#endif
    if (t % 60000 == 59900) publishActiveOverrides();
  }
}
static void dump() {
  for (auto& kv : g_broker) printf("RET %s [%s]\n", kv.first.c_str(), kv.second.c_str());
  printf("PUBS %d\n", g_pubs);
}
int main(int argc, char** argv) {
  const char* c = argv[1];
  initMap();
  // Before the reboot: CS and Toutside overridden, both retained on the broker.
  g_broker["TSet/override"] = "40.00";
  g_broker["Toutside/override"] = "7.50";
  if (!strcmp(c, "reboot_none")) {         // after the reboot no override is active
    connectAndRun(60000);
  } else if (!strcmp(c, "reboot_active")) { // after the reboot OT= is still active (frames keep coming)
    recordOTOverride(27, 0, 8.25f);
    connectAndRun(60000);
  }
  dump();
  printf("END\n");
  return 0;
}
"""

_tmp = None
_exe = None


def setUpModule():
    global _tmp, _exe
    defs = "\n".join(re.findall(r"^#define OVERRIDE_(?:STORE_MAX|ACTIVE_TIMEOUT)\b.*$", HDR, re.M))
    st = re.search(r"struct OTOverrideEntry_t \{.*?\};", HDR, re.S).group(0)
    funcs = func(CORE, "void recordOTOverride(uint8_t id, uint8_t kind, float value) {") + "\n" + \
        func(CORE, "void publishActiveOverrides() {")
    has = "// --- staleOverrideClear begin" in CORE
    if has:
        a = CORE.index("// --- staleOverrideClear begin")
        funcs += "\n" + CORE[a:CORE.index("// --- staleOverrideClear end")]
    _tmp = Path(tempfile.mkdtemp(prefix="ovr_reboot_"))
    cpp = _tmp / "t.cpp"
    cpp.write_text(("#define HAS_SWEEP 1\n" if has else "") + PRE + defs + "\n" + st + "\n" + MAIN.replace("@@FUNCS@@", funcs),
                   encoding="utf-8")
    _exe = compile_cpp(cpp, _tmp / ("t.exe" if os.name == "nt" else "t"))
    if _exe is None:
        raise unittest.SkipTest("no C++ compiler")


def tearDownModule():
    if _tmp:
        shutil.rmtree(_tmp, ignore_errors=True)


def broker(case):
    out = subprocess.run([str(_exe), case], capture_output=True, text=True, check=True).stdout.splitlines()
    assert out[-1] == "END", out
    ret = {l.split(" ", 2)[1]: l.split("[", 1)[1].rsplit("]", 1)[0] for l in out if l.startswith("RET ")}
    pubs = int([l for l in out if l.startswith("PUBS ")][0].split()[1])
    return ret, pubs


class RebootClear(unittest.TestCase):
    def test_a_retained_override_from_before_the_reboot_is_gone_within_a_minute(self):
        ret, _ = broker("reboot_none")
        self.assertNotIn("TSet/override", ret, "old code: the topic stays retained forever after a reboot")
        self.assertNotIn("Toutside/override", ret)

    def test_an_override_active_after_the_reboot_keeps_its_topic_with_the_new_value(self):
        ret, _ = broker("reboot_active")
        self.assertEqual(ret.get("Toutside/override"), "8.25", "AC2: published again within the 60 s timer, never cleared")
        self.assertNotIn("TSet/override", ret)

    def test_only_f88_labels_are_touched(self):
        _, pubs = broker("reboot_none")
        self.assertLessEqual(pubs, 2, "one clear per f88 label in the stub map (TSet, Toutside); SConfig and undefined ids skipped")


class Wiring(unittest.TestCase):
    """The host cannot run handleMQTT(); check that MQTTstuff.ino starts the sweep at every connect and paces it."""
    def setUp(self):
        self.mq = (FW / "MQTTstuff.ino").read_text(encoding="utf-8").replace("\r\n", "\n")

    def test_the_sweep_starts_at_every_connect(self):
        start = func(self.mq, "static void connStageStart() {")
        self.assertIn("startStaleOverrideClear();", start)

    def test_the_sweep_runs_paced_on_the_connected_state(self):
        tick = func(self.mq, "static void staleOverrideClearTick() {")
        self.assertIn("staleOverrideClearStep()", tick)
        self.assertIn("OVR_CLEAR_GAP_MS", tick)
        self.assertIn("CONN_STAGE_MAX_OUTBOX", tick)
        conn = self.mq[self.mq.index("case MQTT_STATE_IS_CONNECTED:"):]
        self.assertLess(conn.index("connStageTick();"), conn.index("staleOverrideClearTick();"))


if __name__ == "__main__":
    unittest.main(verbosity=2)
