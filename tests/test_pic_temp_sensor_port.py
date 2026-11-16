#!/usr/bin/env python3
"""TASK-1940: port of 1.x TASK-1147, the opt-in readout of the temperature sensor wired to the PIC (PR=E).

1.x reference: otgw-1.x.x commit 7ee2344e2 (pollPicTempSensor, picTempSensorHasData, the 'E' case of
handlePRresponse, setting PICtempsensor, otgw-pic/temperature_reading, HA PIC_Temperature_Reading).
Maintainer (Robert, 2026-11-14): on 2.x only for the PIC Classic; on OT-Direct no PIC is read.

The code under test is REAL:
  - picTempSensorPollGate() from src/OTGW-firmware/PICreturnSensor.h, compiled as is;
  - the `if (reg == 'E') { ... }` block of handlePRresponse() in OTGW-Core.ino, cut out and compiled with
    doubles that record the MQTT publish and the discovery request; picParseSensorE() is the real header.
Source checks cover what cannot run on the host: the 3 s call site, the OT-Direct exclusion of the poll gate,
the just-in-time discovery of faux id 239 and the setting's GET / whitelist / dispatch / file triple (ADR-051).

Old-vs-fix: run with OTGW_FW_DIR pointing at a checkout of the old src/OTGW-firmware; every test fails there.
Run: python tests/test_pic_temp_sensor_port.py
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402

FW = Path(os.environ.get("OTGW_FW_DIR", REPO / "src" / "OTGW-firmware"))


def read(name):
    return (FW / name).read_text(encoding="utf-8", errors="replace")


def braced(src, start_pat):
    m = re.search(start_pat, src)
    if not m:
        return None
    i, depth = src.index("{", m.start()), 0
    for j in range(i, len(src)):
        if src[j] == "{":
            depth += 1
        elif src[j] == "}":
            depth -= 1
            if depth == 0:
                return src[m.start():j + 1]
    return None


PRE = r"""
#include <cstdio>
#include <cstdint>
#include <cstring>
#include "PICreturnSensor.h"
#define PSTR(s) (s)
#define F(s) (s)
#define DebugTf(...) do {} while (0)
static size_t strlcpy(char* d, const char* s, size_t n) { size_t l = strlen(s); if (n) { size_t c = l < n - 1 ? l : n - 1; memcpy(d, s, c); d[c] = 0; } return l; }
static uint32_t millis() { return 1000; }
struct { struct { bool bTempSensor; } picBoot; } settings = { { false } };
struct { struct { char sTempSensor[4]; char sTempReading[8]; } picSettings; } state = { { "O", "" } };
static uint8_t OTGWpictempid = 239;
static int g_pending = 0, g_pub = 0, g_tret = 0; static char g_last[16] = "";
static void setMQTTConfigPending(uint8_t id) { if (id == 239) g_pending++; }
static void sendMQTTDataPic(const char* topic, const char* v) { if (!strcmp(topic, "temperature_reading")) { g_pub++; snprintf(g_last, sizeof g_last, "%s", v); } }
static void applyFramelessTret(float) { g_tret++; }
static uint32_t picBoardTretMs = 0; static bool picBoardTretSeen = false; static uint16_t picTretPollResults = 0;
static bool picTempSensorHasData() { return state.picSettings.sTempReading[0] != '\0'; }
"""

MAIN = r"""
static void onE(const char* value) {
  const char reg = 'E';
  @EBLOCK@
}
int main(int argc, char** argv) {
  // argv: setting(0/1) sensorFunction then values
  settings.picBoot.bTempSensor = argv[1][0] == '1';
  state.picSettings.sTempSensor[0] = argv[2][0];
  for (int a = 3; a < argc; a++) {
    onE(argv[a]);
    printf("v=%s pub=%d pending=%d tret=%d last=%s stored=%s\n", argv[a], g_pub, g_pending, g_tret, g_last,
           state.picSettings.sTempReading);
  }
  printf("GATE");
  for (int s = 0; s < 2; s++) for (int p = 0; p < 2; p++) for (int f = 0; f < 2; f++) for (int t = 0; t < 2; t++)
#ifdef HAVE_GATE
    printf(" %d", picTempSensorPollGate(s, p, f, t) ? 1 : 0);
#else
    printf(" x");
#endif
  printf("\n");
  return 0;
}
"""

_tmp = None
_exe = None


def setUpModule():
    global _tmp, _exe
    core = read("OTGW-Core.ino")
    eblock = braced(core, r"if \(reg == 'E'\) \{")
    if eblock is None:
        raise AssertionError("PR=E block not found in handlePRresponse()")
    _tmp = Path(tempfile.mkdtemp(prefix="pic_e_"))
    shutil.copy(FW / "PICreturnSensor.h", _tmp / "PICreturnSensor.h")
    gate = "#define HAVE_GATE 1\n" if "picTempSensorPollGate" in read("PICreturnSensor.h") else ""
    cpp = _tmp / "t.cpp"
    cpp.write_text(gate + PRE + MAIN.replace("@EBLOCK@", eblock.replace("return;", "return;")), encoding="utf-8")
    _exe = compile_cpp(cpp, _tmp / ("t.exe" if os.name == "nt" else "t"))
    if _exe is None:
        raise unittest.SkipTest("no C++ compiler")


def tearDownModule():
    if _tmp:
        shutil.rmtree(_tmp, ignore_errors=True)


def run(*args):
    return subprocess.run([str(_exe)] + list(args), capture_output=True, text=True, check=True).stdout.splitlines()


class ReadingIsPublished(unittest.TestCase):
    def test_valid_reading_publishes_and_announces_once(self):
        out = run("1", "O", "19.19", "20.50")
        self.assertIn("v=19.19 pub=1 pending=1", out[0])
        self.assertIn("last=19.19 stored=19.19", out[0])
        self.assertIn("v=20.50 pub=2 pending=1", out[1], "discovery is requested on the first reading only")

    def test_no_sensor_publishes_nothing_and_clears(self):
        out = run("1", "O", "-", "abc", "19.19", "-")
        self.assertIn("v=- pub=0 pending=0", out[0])
        self.assertIn("v=abc pub=0 pending=0", out[1])
        self.assertIn("v=19.19 pub=1 pending=1", out[2])
        self.assertTrue(out[3].endswith("stored="), out[3])   # "-" after a reading clears the stored value

    def test_setting_off_ignores_the_reply(self):
        out = run("0", "O", "19.19")
        self.assertIn("v=19.19 pub=0 pending=0", out[0])

    def test_return_water_path_still_fed_and_shares_the_reply(self):
        out = run("1", "R", "34.56")
        self.assertIn("pub=1", out[0])
        self.assertIn("tret=1", out[0], "TASK-1314: a valid R reading still feeds Tret")


class PollGate(unittest.TestCase):
    def test_truth_table(self):
        gate = [x for x in run("0", "O")[-1].split()[1:]]
        # order: setting, picPath, flashing, tretPollActive (each 0/1, nested loops)
        expect = []
        for s in (0, 1):
            for p in (0, 1):
                for f in (0, 1):
                    for t in (0, 1):
                        expect.append("1" if (s and p and not f and not t) else "0")
        self.assertEqual(gate, expect)

    def test_poll_excludes_ot_direct_and_runs_every_3_s_tick(self):
        core = read("OTGW-Core.ino")
        fn = braced(core, r"void pollPicTempSensor\(\)\s*\{")
        self.assertIsNotNone(fn, "pollPicTempSensor() missing")
        self.assertIn("!isOTDirectEnabled()", fn)
        self.assertIn("DECLARE_TIMER_SEC(timerPicTempSensor, 180", fn)
        self.assertIn("picSensorTretPollActive()", fn, "no second PR=E while the return water poll runs")
        task3 = braced(read("OTGW-firmware.ino"), r"void doTaskEvery3s\(\)\s*\{")
        # TASK-1971: the PR= settings cycle moved to the loop, so the 3 s task has no early return before the poll
        self.assertIn("pollPicTempSensor();", task3)
        self.assertNotIn("return;", task3.split("pollPicTempSensor();")[0])


class DiscoveryAndSetting(unittest.TestCase):
    def test_faux_id_239_is_just_in_time(self):
        mq = read("MQTTstuff.ino")
        q = braced(mq, r"static void queueNonOTDiscoveryIds\(\)\s*\{")
        self.assertIn("if (picTempSensorHasData()) setMQTTConfigPending(OTGWpictempid);", q)
        self.assertIn("if (i == OTGWpictempid) continue;", mq)
        self.assertRegex(read("OTGW-firmware.h"), r"byte\s+OTGWpictempid\s*=\s*239;")
        disc = read("MQTTHaDiscovery.cpp")
        self.assertIn("{239, 0x08, ha_lbl_pic_temperature_reading,", disc)
        self.assertIn('"temperature_reading"', disc)
        self.assertIn('"PIC_Temperature_Reading"', disc)
        for src in (disc, mq):
            self.assertIn("case 239: return HaDevice::Sensors;", src)

    def test_setting_triple_and_file_key(self):
        self.assertIn('addBool(F("pictempsensor"), settings.picBoot.bTempSensor, "b");', read("restAPI.ino"))
        self.assertRegex(read("restAPI.ino"), r'"pictempsensor",')
        st = read("settingStuff.ino")
        self.assertIn('writeJsonBoolKV(file, F("PICtempsensor"), settings.picBoot.bTempSensor);', st)
        self.assertIn('PSTR("PICtempsensor"))==0)        settings.picBoot.bTempSensor = EVALBOOLEAN(newValue);', st)
        self.assertRegex(read("PICtypes.h"), r"bool bTempSensor\s*=\s*false;")


if __name__ == "__main__":
    unittest.main(verbosity=2)
