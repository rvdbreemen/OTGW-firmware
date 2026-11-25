#!/usr/bin/env python3
"""TASK-3011 / TASK-3015 (entry 451, Robert's Q1-Q3 answers): BLE feeds SAT only from a sensor the user chose.

Nothing is re-implemented for the BLE choice: the test cuts the real `struct BLERuntime` and `satBLEUpdateState()` out of
src/OTGW-firmware/SATble.ino and compiles them with a host C++ compiler against small stubs for settings/state and the
BLE mutex. The migration block of readSettings() ([sat-blepinonly-migrate:begin/end]) is compiled the same way.

Cases (old code = before TASK-3011):
  1. no pin, one fresh sensor in the roster: old drives SAT from it (bBleTempValid), fix does not;
  2. pin set and fresh: both drive SAT from the pinned sensor;
  3. pin stale, failover on, the only other fresh sensor is NOT assigned to an area (e.g. a neighbour's): old switches to
     it, fix does not;
  4. pin stale, failover on, another fresh sensor IS assigned to an area: fix switches to it;
  5. pin stale, failover off: nothing drives SAT;
  6. migration (no SATblepinonly marker): bBleFailover becomes false, a stored pin is kept and flagged (bBlePinNotice);
     no pin -> no notice; with the marker present nothing changes.
Source checks: the TASK-508 auto-select pin is gone; a SATblemac write clears the notice only outside settings loading;
every return of satGetRoomTemp() records its source (TASK-3015) and the topic sat/room_temp_source is published.

Red on the old code (cases 1, 3, 6 and the source checks), green on the fix. Run: python tests/test_sat_ble_pin_only.py
"""
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from test_sat_pwm_override import compile_cpp  # noqa: E402

FW = HERE.parent / "src" / "OTGW-firmware"
BLE_INO = FW / "SATble.ino"
SS_INO = FW / "settingStuff.ino"
SAT_INO = FW / "SATcontrol.ino"


def read(p):
    return Path(p).read_text(encoding="utf-8").replace("\r\n", "\n")


def func(src, sig):
    a = src.index(sig)
    b = src.index("\n}\n", a) + 3
    return src[a:b]


PRE = r"""
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <cstdarg>
#ifdef _MSC_VER
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif
#define SAT_BLE_MAX_ROSTER 8
#define PSTR(x) x
#define F(x) x
typedef int portMUX_TYPE;
#define portENTER_CRITICAL(m) (void)(m)
#define portEXIT_CRITICAL(m) (void)(m)
static portMUX_TYPE _bleSensorsMux = 0;
static uint32_t g_ms = 1000000;
static uint32_t millis() { return g_ms; }
static const uint32_t BLE_STALE_MS = 300000;
static bool _bleFailoverActive = false;
struct { struct { char sBleMAC[18]; char sBleMac[SAT_BLE_MAX_ROSTER][18]; char sSensorArea[4][18]; bool bBleFailover; bool bBlePinNotice; } sat; } settings;
struct { struct { uint8_t iBleSensorCount; float fBleTemp, fBleHumidity; uint8_t iBleBattery; int8_t iBleRssi; bool bBleTempValid; uint32_t iBleTempLastMs; } sat; } state;
struct { bool active = false; void recBle(uint8_t, int16_t, int16_t, uint8_t, int8_t) {} } diagRec;
static int16_t diagX100(float v) { return (int16_t)(v * 100); }
static void satSetAreaTemp(uint8_t, float) {}
static void satNoteBleHumidity(float, uint32_t) {}
static void SATBLEDebugTf(const char*, ...) {}
static int g_narr = 0;
static void satNarratef_P(const char*, ...) { g_narr++; }
static void satNarrate_P(const char*) { g_narr++; }
@STRUCT@
static BLERuntime _bleRuntime[SAT_BLE_MAX_ROSTER] = {};
@FUNC@
static void seen(int slot, const char* mac, float t, uint32_t age) {
  strcpy(settings.sat.sBleMac[slot], mac);
  _bleRuntime[slot].fTemperature = t; _bleRuntime[slot].fHumidity = NAN; _bleRuntime[slot].iLastSeenMs = g_ms - age;
}
static void reset() { memset(&settings, 0, sizeof settings); memset(&state, 0, sizeof state); memset(_bleRuntime, 0, sizeof _bleRuntime); _bleFailoverActive = false; }
int main(int argc, char** argv) {
  const char* sc = argv[1];
  reset();
  if (!strcmp(sc, "nopin"))   { seen(0, "AA:AA:AA:AA:AA:01", 19.5f, 1000); }
  if (!strcmp(sc, "pinned"))  { seen(0, "AA:AA:AA:AA:AA:01", 19.5f, 1000); seen(1, "AA:AA:AA:AA:AA:02", 22.0f, 1000); strcpy(settings.sat.sBleMAC, "AA:AA:AA:AA:AA:02"); }
  if (!strcmp(sc, "neighbour")) { seen(0, "AA:AA:AA:AA:AA:01", 19.5f, 400000); seen(1, "BB:BB:BB:BB:BB:09", 24.0f, 1000);
                                  strcpy(settings.sat.sBleMAC, "AA:AA:AA:AA:AA:01"); settings.sat.bBleFailover = true; }
  if (!strcmp(sc, "area"))    { seen(0, "AA:AA:AA:AA:AA:01", 19.5f, 400000); seen(1, "BB:BB:BB:BB:BB:09", 24.0f, 1000); seen(2, "CC:CC:CC:CC:CC:03", 20.5f, 1000);
                                strcpy(settings.sat.sSensorArea[1], "CC:CC:CC:CC:CC:03");
                                strcpy(settings.sat.sBleMAC, "AA:AA:AA:AA:AA:01"); settings.sat.bBleFailover = true; }
  if (!strcmp(sc, "nofailover")) { seen(0, "AA:AA:AA:AA:AA:01", 19.5f, 400000); seen(2, "CC:CC:CC:CC:CC:03", 20.5f, 1000);
                                   strcpy(settings.sat.sSensorArea[1], "CC:CC:CC:CC:CC:03"); strcpy(settings.sat.sBleMAC, "AA:AA:AA:AA:AA:01"); }
  satBLEUpdateState();
  printf("valid=%d temp=%.1f\n", (int)state.sat.bBleTempValid, state.sat.bBleTempValid ? state.sat.fBleTemp : 0.0f);
  return 0;
}
"""

MIG = r"""
#include <cstdio>
#include <cstring>
#define PSTR(x) x
#define F(x) x
static void DebugTf(const char*, ...) {}
static void DebugTln(const char*) {}
static bool settingsDirty = false;
struct { struct { char sBleMAC[18]; bool bBleFailover; bool bBlePinNotice; } sat; } settings;
int main(int argc, char** argv) {
  const bool marker = argv[1][0] == '1';
  strcpy(settings.sat.sBleMAC, argv[2][0] == '-' ? "" : argv[2]);
  settings.sat.bBleFailover = true; settings.sat.bBlePinNotice = false;
  bool g_sawSatBlePinOnlyMarkerKey = marker;
@MIGRATE@
  printf("failover=%d notice=%d mac=%s dirty=%d\n", (int)settings.sat.bBleFailover, (int)settings.sat.bBlePinNotice, settings.sat.sBleMAC, (int)settingsDirty);
  return 0;
}
"""


def build_ble(src):
    st = re.search(r"^struct BLERuntime \{.*?^\};\n", src, re.S | re.M).group(0)
    fn = func(src, "void satBLEUpdateState()\n{")
    return PRE.replace("@STRUCT@", st).replace("@FUNC@", fn)


def build_mig(src):
    m = re.search(r"// \[sat-blepinonly-migrate:begin\][^\n]*\n(.*?)// \[sat-blepinonly-migrate:end\]", src, re.S)
    return MIG.replace("@MIGRATE@", m.group(1) if m else "  (void)g_sawSatBlePinOnlyMarkerKey;   // no migration block in this source")


class _Base(unittest.TestCase):
    exe = None

    def run_exe(self, *args):
        if self.exe is None:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe), *args], capture_output=True, text=True, check=True).stdout.strip()
        return dict(kv.split("=", 1) for kv in out.split())


class BleChoice(_Base):
    @classmethod
    def setUpClass(cls):
        cls.d = Path(tempfile.mkdtemp(prefix="sat_ble_pin_"))
        cpp = cls.d / "ble.cpp"
        cpp.write_text(build_ble(read(BLE_INO)), encoding="utf-8")
        cls.exe = compile_cpp(cpp, cls.d / "ble.exe")

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.d, ignore_errors=True)

    def test_1_no_pin_does_not_drive_sat(self):
        self.assertEqual(self.run_exe("nopin")["valid"], "0", "an unpinned roster sensor drove SAT")

    def test_2_fresh_pin_drives_sat(self):
        r = self.run_exe("pinned")
        self.assertEqual((r["valid"], r["temp"]), ("1", "22.0"))

    def test_3_stale_pin_never_fails_over_to_an_unchosen_sensor(self):
        self.assertEqual(self.run_exe("neighbour")["valid"], "0", "failover went to a sensor the user did not choose")

    def test_4_stale_pin_fails_over_to_an_area_sensor(self):
        r = self.run_exe("area")
        self.assertEqual((r["valid"], r["temp"]), ("1", "20.5"))

    def test_5_stale_pin_without_failover_drives_nothing(self):
        self.assertEqual(self.run_exe("nofailover")["valid"], "0")


class Migration(_Base):
    @classmethod
    def setUpClass(cls):
        cls.d = Path(tempfile.mkdtemp(prefix="sat_ble_mig_"))
        cpp = cls.d / "mig.cpp"
        cpp.write_text(build_mig(read(SS_INO)), encoding="utf-8")
        cls.exe = compile_cpp(cpp, cls.d / "mig.exe")

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.d, ignore_errors=True)

    def test_6a_upgrade_with_a_pin_keeps_it_flags_it_and_turns_failover_off(self):
        self.assertEqual(self.run_exe("0", "AA:AA:AA:AA:AA:01"),
                         {"failover": "0", "notice": "1", "mac": "AA:AA:AA:AA:AA:01", "dirty": "1"})

    def test_6b_upgrade_without_a_pin_has_no_notice(self):
        r = self.run_exe("0", "-")
        self.assertEqual((r["failover"], r["notice"]), ("0", "0"))

    def test_6c_marker_present_changes_nothing(self):
        self.assertEqual(self.run_exe("1", "AA:AA:AA:AA:AA:01"),
                         {"failover": "1", "notice": "0", "mac": "AA:AA:AA:AA:AA:01", "dirty": "0"})


ROOM_PRE = r"""
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#ifdef _MSC_VER
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif
#define F(x) x
#define PSTR(x) x
static const uint8_t SAT_ROOMSRC_AUTO = 0, SAT_ROOMSRC_THERMOSTAT = 1, SAT_ROOMSRC_EXTERNAL = 2, SAT_ROOMSRC_BLE = 3, SAT_ROOMSRC_AREAS = 4, SAT_ROOMSRC_MAX = 4;
static const uint32_t SAT_STALE_TEMP_BLE_MS = 300000UL, SAT_STALE_TEMP_MS = 300000UL;
static uint32_t g_ms = 10000000UL;
static uint32_t millis() { return g_ms; }
struct { struct { bool bSimulation = false, bThermalComfort = false, bBleEnable = true, bMultiArea = false, bUseExternalTemp = false;
                  int iMultiAreaCount = 0; uint32_t iSensorMaxAgeS = 21600; uint8_t iRoomSource = 0; } sat; } settings;
struct { struct { bool bBleTempValid = false, bExternalTempValid = false, bFallbackActive = false; float fBleTemp = 0, fExternalTemp = 0,
                  fSimRoomTemp = 0, fHumidity = 0; uint32_t iBleTempLastMs = 0, iExternalTempLastMs = 0, iLastKnownRoomMs = 0, iSimDropoutExpiryMs = 0; } sat;
         struct { uint32_t iTrThermostatMs = 0; } otBus; } state;
struct { float Tr = NAN; } OTcurrentSystemState;
static float g_areas = NAN;
static float satGetWeightedRoomTemp() { return g_areas; }
static bool satRoomTempValid(float v) { return !std::isnan(v) && v >= -10.0f && v <= 50.0f; }
static bool satHumidityFresh(uint32_t) { return false; }
static float satCalcSimmerIndex(float t, float) { return t; }
static float satEstimateRoomTemp(float) { return NAN; }
static float satGetOutsideTemp() { return 5.0f; }
static void SATDebugTln(const char*) {}
static void SATTraceTf(const char*, ...) {}
static void SATTraceTln(const char*) {}
@ROOM@
int main(int argc, char** argv) {
  // argv: primary ble ext areas tr   (value or '-' for none; 'S' = stale)
  settings.sat.iRoomSource = (uint8_t)atoi(argv[1]);
  if (argv[2][0] == 'S') { state.sat.bBleTempValid = true; state.sat.fBleTemp = 21.0f; state.sat.iBleTempLastMs = g_ms - 400000UL; }
  else if (argv[2][0] != '-') { state.sat.bBleTempValid = true; state.sat.fBleTemp = (float)atof(argv[2]); state.sat.iBleTempLastMs = g_ms - 1000; }
  if (argv[3][0] == 'S') { settings.sat.bUseExternalTemp = true; state.sat.bExternalTempValid = true; state.sat.fExternalTemp = 22.0f; state.sat.iExternalTempLastMs = g_ms - 30000000UL; }
  else if (argv[3][0] != '-') { settings.sat.bUseExternalTemp = true; state.sat.bExternalTempValid = true; state.sat.fExternalTemp = (float)atof(argv[3]); state.sat.iExternalTempLastMs = g_ms - 1000; }
  if (argv[4][0] != '-') { settings.sat.bMultiArea = true; settings.sat.iMultiAreaCount = 2; g_areas = (float)atof(argv[4]); }
  if (argv[5][0] == 'S') { OTcurrentSystemState.Tr = 20.0f; state.otBus.iTrThermostatMs = g_ms - 400000UL; }
  else if (argv[5][0] != '-') { OTcurrentSystemState.Tr = (float)atof(argv[5]); state.otBus.iTrThermostatMs = g_ms - 1000; }
  const float r = satGetRoomTemp();
  printf("room=%.1f src=%s\n", std::isnan(r) ? -99.0f : r, satRoomSourceName());
  return 0;
}
"""


def build_room(src):
    start = "// TASK-3011: the thermostat's room temperature (MsgID 24)"
    a = src.index("// TASK-3015: which source the last satGetRoomTemp() used")
    b = src.index("static float satGetRoomTemp()\n{", a)
    names = src[a:b]
    if start in names:          # the resolver helpers sit between the source names and satGetRoomTemp()
        names = names[:names.index(start)] + src[src.index(start):b]
    room = func(src, "static float satGetRoomTemp()\n{")
    return ROOM_PRE.replace("@ROOM@", names + room)


class PrimarySource(_Base):
    """TASK-3011 (Robert, follow-up A): SATroomsource puts one source first; the rest follow the auto order
    (pinned BLE > areas > external > thermostat), then none (heating curve only)."""

    @classmethod
    def setUpClass(cls):
        cls.d = Path(tempfile.mkdtemp(prefix="sat_room_src_"))
        cpp = cls.d / "room.cpp"
        cpp.write_text(build_room(read(SAT_INO)), encoding="utf-8")
        cls.exe = compile_cpp(cpp, cls.d / "room.exe")

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.d, ignore_errors=True)

    def room(self, primary, ble="-", ext="-", areas="-", tr="-"):
        r = self.run_exe(str(primary), ble, ext, areas, tr)
        return r["room"], r["src"]

    def test_p1_auto_keeps_todays_order(self):
        self.assertEqual(self.room(0, ble="21.0", ext="22.0", areas="20.5", tr="19.5"), ("21.0", "ble"))
        self.assertEqual(self.room(0, ext="22.0", areas="20.5", tr="19.5"), ("20.5", "areas"))
        self.assertEqual(self.room(0, ext="22.0", tr="19.5"), ("22.0", "external"))
        self.assertEqual(self.room(0, tr="19.5"), ("19.5", "thermostat"))

    def test_p2_thermostat_first_with_a_fresh_reading(self):
        self.assertEqual(self.room(1, ble="21.0", ext="22.0", tr="19.5"), ("19.5", "thermostat"))

    def test_p3_stale_thermostat_falls_back_along_the_auto_order(self):
        self.assertEqual(self.room(1, ext="22.0", tr="S"), ("22.0", "external"))

    def test_p4_external_first(self):
        self.assertEqual(self.room(2, ble="21.0", ext="22.0", tr="19.5"), ("22.0", "external"))

    def test_p5_stale_external_falls_back_to_ble(self):
        self.assertEqual(self.room(2, ble="21.0", ext="S", tr="19.5"), ("21.0", "ble"))

    def test_p6_areas_first(self):
        self.assertEqual(self.room(4, ble="21.0", areas="20.5", tr="19.5"), ("20.5", "areas"))

    def test_p7_nothing_valid_is_the_heating_curve(self):
        self.assertEqual(self.room(3, ble="S", ext="S", tr="S"), ("-99.0", "none"))


class Source(unittest.TestCase):
    def test_auto_select_pin_is_gone(self):
        self.assertNotIn('updateSetting("SATblemac", settings.sat.sBleMac[seenIdx])', read(BLE_INO))

    def test_failover_defaults_off(self):
        self.assertRegex(read(FW / "SATtypes.h"), r"bool\s+bBleFailover\s*=\s*false;")

    def test_notice_cleared_only_by_a_user_write(self):
        ss = read(SS_INO)
        m = re.search(r'PSTR\("SATblemac"\)\) == 0\) \{(.*?)\n  \}', ss, re.S)
        self.assertTrue(m and "if (!g_loadingSettingsFile) settings.sat.bBlePinNotice = false;" in m.group(1),
                        "SATblemac setter must clear the notice outside settings loading only")
        self.assertIn('F("SATblepinonly")', ss)
        self.assertIn('F("SATblepinnotice")', ss)

    def test_notice_reaches_rest_and_log(self):
        ble = read(BLE_INO)
        self.assertIn('je.field(F("ble_pin_notice"),', ble)
        self.assertIn("SAT uses BLE sensor %.17s: confirm or change it on the SAT page", ble)

    def test_every_room_temp_return_records_its_source(self):
        sat = read(SAT_INO)
        body = func(sat, "static float satGetRoomTemp()\n{")
        rets = [ln for ln in body.splitlines() if re.search(r"\breturn\b", ln) and not ln.strip().startswith("//")]
        self.assertTrue(rets)
        for i, ln in enumerate(body.splitlines()):
            if re.search(r"\breturn\b", ln) and not ln.strip().startswith("//"):
                prev = body.splitlines()[i - 1]
                self.assertTrue("_sat_roomSrc = " in ln or "_sat_roomSrc = " in prev or "satTryRoomSource(" in ln,
                                "return without a source: %r" % ln)
        self.assertIn('publishIfChangedS(F("sat/room_temp_source"), satRoomSourceName()', sat)

    # TASK-3011 (entries 452/454: command + retained state): sat/room_source is in the always-on core set, so the retained
    # setting is published on the first pass after a boot or reconnect (first-seen shadow), also while SAT is off or not
    # controlling, and its shadow is reset by satRepublishCoreState().
    def test_room_source_state_is_in_the_core_set(self):
        sat = read(SAT_INO)
        core = func(sat, "static void satPublishCoreState()\n{")
        self.assertIn('publishIfChangedS(F("sat/room_source"), satPrimaryRoomSourceName(settings.sat.iRoomSource), s_core_room_source, true)', core)
        rep = func(sat, "void satRepublishCoreState()\n{")
        self.assertIn("memset(&s_core_room_source, 0, sizeof(s_core_room_source));", rep)
        self.assertNotIn('F("sat/room_source")', func(sat, "void satPublishMQTT()\n{"), "published twice")
        self.assertIn('je.field(F("room_temp_source"),', sat)
        ha = read(FW / 'MQTTHaDiscovery.cpp')
        self.assertIn('"sat/room_temp_source"', ha)
        self.assertRegex(ha, r'ha_lbl_sat_room_temp_source,.*HaEntityCat::diagnostic')


if __name__ == "__main__":
    unittest.main(verbosity=2)
