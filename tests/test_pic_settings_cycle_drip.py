#!/usr/bin/env python3
"""TASK-1971: after a boot the PIC settings cycle (15 x PR=, one per 3 s) waited 4-5 min while the MQTT discovery drip
ran (09 on .63, beta.306+041400e: 0/15 after 80 s).

Cause: doTaskEvery3s() (3 s timer) asked queryNextPICsetting(), which defers while a drip tick is due within 500 ms.
The drip ticks every 1 s from its own timer. Both start together at boot and, with SKIP_MISSED_TICKS, drift only by
the loop lateness, so the 3 s tick stays inside the drip window and is deferred tick after tick until the phase has
drifted out or the drip has nothing pending. The simulation holds the phase fixed (no drift): it proves the mechanism,
not the 4-5 min timing.

Nothing is re-implemented. The host program holds the real dripDueWithinMs() (MQTTstuff.ino), the real PIC settings
readout block (triggerPICsettingsReadout, queryNextPICsetting and, when present, pollPICsettingsQuery, OTGW-Core.ino)
and the real doTaskEvery3s() (OTGW-firmware.ino). The loop is simulated in the firmware order: the 3 s task, the
discovery drip (which publishes its next due time as loopMQTTDiscovery() does), then pollPICsettingsQuery() if the
source has it. Stubs: millis, the PIC/flash state, addCommandToQueue (records the time of each PR=).

Old-vs-fix: OTGW_FW_DIR points at a tree of the old code; the worst-phase case sends no PR= in 60 s there.

Run: python tests/test_pic_settings_cycle_drip.py
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

REPO = Path(__file__).resolve().parent.parent
FW = Path(os.environ.get("OTGW_FW_DIR", REPO / "src" / "OTGW-firmware"))


def read(name):
    return (FW / name).read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")


def braced(src, start_pat):
    m = re.search(start_pat, src)
    if not m:
        raise AssertionError("not found: " + start_pat)
    i, depth = src.index("{", m.start()), 0
    for j in range(i, len(src)):
        if src[j] == "{":
            depth += 1
        elif src[j] == "}":
            depth -= 1
            if depth == 0:
                return src[m.start():j + 1]
    raise AssertionError("unbalanced: " + start_pat)


PRE = r"""
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#define PSTR(s) (s)
#define F(s) (s)
#define snprintf_P snprintf
#define OTDebugTln(...) do {} while (0)
#define OTDebugTf(...) do {} while (0)
static uint32_t g_now = 0;
static uint32_t millis() { return g_now; }
static bool isPICEnabled() { return true; }
static bool isGatewayFirmware() { return true; }
static bool isStatusBurstActive() { return false; }
struct { struct { bool bESPactive; bool bPICactive; } flash; } state = { { false, false } };
static uint8_t MQTTautoCfgPendingMap[32];
static uint32_t sDripDueAtMs = 0;
static int g_sent = 0; static uint32_t g_sentAt[64];
static void addCommandToQueue(const char* cmd, int, bool) { if (!strncmp(cmd, "PR=", 3) && g_sent < 64) g_sentAt[g_sent++] = g_now; }
static void evaluateOTBusLiveness(int) {}
enum class OTBusLivenessTrigger { Tick };
static void evaluateOTBusLiveness(OTBusLivenessTrigger) {}
static void pollPicTempSensor() {}
"""

MAIN = r"""
int main(int argc, char** argv) {
  // argv: dripPhaseMs (drip tick offset after each whole second) dripPending(0/1) endMs
  const uint32_t phase = (uint32_t)atoi(argv[1]);
  const bool pending = argv[2][0] == '1';
  const uint32_t endMs = (uint32_t)atoi(argv[3]);
  if (pending) MQTTautoCfgPendingMap[0] = 0xFF;
  triggerPICsettingsReadout();
  uint32_t next3 = 3000, dripDue = 1000 + phase;
  for (g_now = 0; g_now <= endMs; g_now += 10) {
    if (g_now >= next3) { next3 += 3000; doTaskEvery3s(); }      // timer3s block
    sDripDueAtMs = dripDue;                                        // loopMQTTDiscovery(): exposes the due time
    if (g_now >= dripDue) dripDue += 1000;                         //   before DUE() moves it (MQTTstuff.ino)
#ifdef HAVE_POLL
    pollPICsettingsQuery();
#endif
  }
  printf("%d", g_sent);
  for (int i = 0; i < g_sent; i++) printf(" %u", (unsigned)g_sentAt[i]);
  printf("\n");
  return 0;
}
"""


def setUpModule():
    global _tmp, _exe
    mqtt = read("MQTTstuff.ino")
    core = read("OTGW-Core.ino")
    main_ino = read("OTGW-firmware.ino")
    drip = braced(mqtt, r"bool dripDueWithinMs\(uint32_t windowMs\)\s*\{")
    a = core.index("bool            picSettingsCycleActive")
    b = core.index("//===================[ handlePRresponse")
    block = core[a:b]
    task3 = braced(main_ino, r"void doTaskEvery3s\(\)\s*\{")
    has_poll = "pollPICsettingsQuery(" in block
    src = ("#define HAVE_POLL 1\n" if has_poll else "") + PRE + drip + "\n" + block + "\n" + task3 + "\n" + MAIN
    _tmp = Path(tempfile.mkdtemp(prefix="pic_cycle_"))
    cpp = _tmp / "t.cpp"
    cpp.write_text(src, encoding="utf-8")
    _exe = compile_cpp(cpp, _tmp / ("t.exe" if os.name == "nt" else "t"))


def tearDownModule():
    shutil.rmtree(_tmp, ignore_errors=True)


def run(phase, pending, end_ms):
    if _exe is None:
        raise unittest.SkipTest("no host C++ compiler")
    out = subprocess.run([str(_exe), str(phase), "1" if pending else "0", str(end_ms)],
                         capture_output=True, text=True, check=True).stdout.split()
    return [int(x) for x in out[1:]]


class CycleDuringDrip(unittest.TestCase):
    def test_worst_phase_completes_within_60_s(self):
        # the drip ticks 300 ms after every 3 s task tick: inside the 500 ms window each time (09's case)
        sent = run(300, True, 60000)
        self.assertEqual(len(sent), 15, "PR= sent at %s" % sent)

    def test_boot_phase_zero_completes_within_60_s(self):
        # at boot both timers come due in the same loop pass; the 3 s task runs first and sees the drip tick as due
        sent = run(0, True, 60000)
        self.assertEqual(len(sent), 15, "PR= sent at %s" % sent)

    def test_queries_stay_at_least_3_s_apart(self):
        sent = run(300, True, 90000)
        gaps = [b - a for a, b in zip(sent, sent[1:])]
        self.assertTrue(gaps and min(gaps) >= 3000, gaps)

    def test_each_query_avoids_the_500_ms_before_a_drip_tick(self):
        # ADR-088 / TASK-1699: the drip gate stays; no PR= goes out within 500 ms before a drip tick
        for t in run(300, True, 60000):
            left = (1000 + 300 - t % 1000) % 1000 or 1000
            self.assertGreater(left, 500, "PR= at %d ms is %d ms before a drip tick" % (t, left))

    def test_harmless_phase_and_no_drip_still_complete(self):
        self.assertEqual(len(run(700, True, 60000)), 15)
        self.assertEqual(len(run(300, False, 60000)), 15)


if __name__ == "__main__":
    unittest.main(verbosity=2)
