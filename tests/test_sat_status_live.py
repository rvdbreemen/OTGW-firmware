#!/usr/bin/env python3
"""TASK-1885: /api/v2/sat/status carries the boiler's actual flow, return and flame, and the sensor ages.

Before this task sat/status had sim_flow_temp/sim_flame_on in simulation only, so outside simulation the web UI could
not plot the real flow, and nothing said how old the room or outside temperature was (VTherm UI review U7/U8,
docs/sat/vtherm-comparison-2026-11-13.md).

The functional part cuts the REAL block between 'begin/end sat-status-live record' out of SATcontrol.ino and the
real JsonEmit out of jsonEmit.h, compiles them on the host against fixture state, and parses the JSON that
satEmitStatusLive() writes. Nothing is re-implemented. The wiring (the snapshot fills the struct before the closure,
the closure emits it, satGetRoomTemp() keeps the source order the age mirror follows) is checked on the source text.
The full satSendStatusJSON() is not compiled on the host; the live path is checked on the bench.

Old-vs-fix: OTGW_SAT_INO=<path to another SATcontrol.ino> (e.g. `git show origin/dev:src/OTGW-firmware/SATcontrol.ino`).
Run: python tests/test_sat_status_live.py
"""
import json
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from test_sat_pwm_override import compile_cpp  # noqa: E402

FW = HERE.parent / "src" / "OTGW-firmware"
SAT_INO = Path(os.environ.get("OTGW_SAT_INO", FW / "SATcontrol.ino"))
SRC = SAT_INO.read_text(encoding="utf-8")
BEGIN, END = "// begin sat-status-live record", "// end sat-status-live record"

PRELUDE = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#define PROGMEM
#define PSTR(s) (s)
#define F(s) (reinterpret_cast<const __FlashStringHelper*>(s))
#define FPSTR(s) (reinterpret_cast<const __FlashStringHelper*>(s))
#define snprintf_P snprintf
typedef const char* PGM_P;
class __FlashStringHelper;
struct String { const char* c_str() const { return ""; } };
struct Print {
  void print(char c) { fputc(c, stdout); }
  void print(const char* s) { fputs(s, stdout); }
  void print(const __FlashStringHelper* s) { fputs(reinterpret_cast<const char*>(s), stdout); }
};
"""

FIXTURE = r"""
static const uint32_t SAT_STALE_TEMP_MS = 300000UL;
static const uint8_t  SAT_MAX_AREAS = 4;
struct SatSettings { bool bSimulation=false, bBleEnable=false, bMultiArea=false, bUseExternalTemp=false;
                     uint8_t iMultiAreaCount=0; float fAreaWeight[4] = {1, 1, 1, 1}; };
struct Settings { SatSettings sat; };
struct SatState { bool bBleTempValid=false, bExternalTempValid=false, bFallbackActive=false; bool bAreaValid[4] = {false};
                  uint32_t iBleTempLastMs=0, iExternalTempLastMs=0; uint32_t iAreaLastMs[4] = {0};
                  float fSimFlowTemp=0, fSimReturnTemp=0; };
struct OtBusState { uint32_t iTrThermostatMs = 0; };
struct State { SatState sat; OtBusState otBus; };
struct OTSys { float Tr = NAN, Tboiler = 0, Tret = 0; };
static Settings settings; static State state; static OTSys OTcurrentSystemState;
enum class SatFeedClass : uint8_t { Valid, Stale };
static bool g_flowFresh = false, g_retFresh = false;
static SatFeedClass satFlowFeedClass() { return g_flowFresh ? SatFeedClass::Valid : SatFeedClass::Stale; }
static bool satFeedClassIsFresh(SatFeedClass c) { return c == SatFeedClass::Valid; }
static bool satReturnFresh() { return g_retFresh; }
"""

MAIN = r"""
// argv: scenario room nowMs. Prints the JSON object satEmitStatusLive() writes for the fixture.
int main(int argc, char** argv) {
  const char* sc = argv[1];
  const float room = (float)atof(argv[2]);
  const uint32_t now = (uint32_t)strtoul(argv[3], 0, 10);
  OTcurrentSystemState.Tboiler = 47.5f; OTcurrentSystemState.Tret = 38.0f;
  if (!strcmp(sc, "boiler")) {            // real boiler: fresh flow and return, thermostat Tr 90 s old
    g_flowFresh = g_retFresh = true; OTcurrentSystemState.Tr = 20.5f; state.otBus.iTrThermostatMs = now - 90000UL;
  } else if (!strcmp(sc, "noflow")) {     // flow stale, return absent, Tr fed by TR= (no thermostat stamp)
    OTcurrentSystemState.Tr = 20.5f;
  } else if (!strcmp(sc, "ble")) {
    settings.sat.bBleEnable = true; state.sat.bBleTempValid = true; state.sat.iBleTempLastMs = now - 42000UL;
    OTcurrentSystemState.Tr = 20.5f; state.otBus.iTrThermostatMs = now - 5000UL;
  } else if (!strcmp(sc, "areas")) {      // oldest contributing area wins; area 2 has weight 0 and does not count
    settings.sat.bMultiArea = true; settings.sat.iMultiAreaCount = 3; settings.sat.fAreaWeight[2] = 0.0f;
    state.sat.bAreaValid[0] = state.sat.bAreaValid[1] = state.sat.bAreaValid[2] = true;
    state.sat.iAreaLastMs[0] = now - 30000UL; state.sat.iAreaLastMs[1] = now - 120000UL; state.sat.iAreaLastMs[2] = now - 290000UL;
  } else if (!strcmp(sc, "external")) {
    settings.sat.bUseExternalTemp = true; state.sat.bExternalTempValid = true; state.sat.iExternalTempLastMs = now - 600000UL;
  } else if (!strcmp(sc, "fallback")) {   // the thermal estimate: no age
    state.sat.bFallbackActive = true; OTcurrentSystemState.Tr = 99.0f; state.otBus.iTrThermostatMs = now - 1000UL;
  } else if (!strcmp(sc, "sim")) {
    settings.sat.bSimulation = true; state.sat.fSimFlowTemp = 55.25f; state.sat.fSimReturnTemp = 44.0f;
  } else if (!strcmp(sc, "wrap")) {       // millis wrapped between the stamp and now
    g_flowFresh = true; OTcurrentSystemState.Tr = 20.5f; state.otBus.iTrThermostatMs = 0xFFFFF000u;
  }
  SatStatusLive l;
  uint32_t ms = 0;
  const bool have = satRoomTempSampleMs(room, now, &ms);
  l.roomAgeS = satSampleAgeS(have, ms, now);
  l.outsideAgeS = satSampleAgeS(!strcmp(sc, "boiler"), now - 1800000UL, now);   // a 30 min old weather fetch
  l.flowTemp = satStatusFlowTemp();
  l.returnTemp = satStatusReturnTemp();
  l.flameOn = !strcmp(sc, "boiler") || !strcmp(sc, "sim");
  Print out; JsonEmit je(out);
  je.beginObject(); satEmitStatusLive(je, l.flowTemp, l.returnTemp, l.flameOn, l.roomAgeS, l.outsideAgeS); je.endObject();
  printf("\n");
  return 0;
}
"""


def between(src, a, b):
    i, j = src.find(a), src.find(b)
    assert i >= 0 and j > i, f"markers '{a}' .. '{b}' not found in {SAT_INO.name}"
    return src[i:j]


class StatusLive(unittest.TestCase):
    exe = None
    tmp = None

    @classmethod
    def setUpClass(cls):
        block = between(SRC, BEGIN, END)
        je = (FW / "jsonEmit.h").read_text(encoding="utf-8")
        je = re.sub(r"#include <(Arduino|platform)\.h>.*", "", je)
        cls.tmp = tempfile.TemporaryDirectory()
        d = Path(cls.tmp.name)
        cpp = d / "live.cpp"
        cpp.write_text(PRELUDE + je + FIXTURE + block + MAIN, encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / ("live.exe" if os.name == "nt" else "live"))
        if cls.exe is None:
            raise unittest.SkipTest("no C++ compiler on this host")

    @classmethod
    def tearDownClass(cls):
        if cls.tmp:
            cls.tmp.cleanup()

    def run_sc(self, sc, room="20.5", now=10_000_000):
        out = subprocess.run([str(self.exe), sc, room, str(now)], capture_output=True, text=True, check=True).stdout
        return json.loads(out)

    def test_real_boiler_fields(self):
        d = self.run_sc("boiler")
        self.assertEqual(list(d), ["flow_temp", "return_temp", "flame_on", "room_temp_age_s", "outside_temp_age_s"])
        self.assertAlmostEqual(d["flow_temp"], 47.5)
        self.assertAlmostEqual(d["return_temp"], 38.0)
        self.assertIs(d["flame_on"], True)
        self.assertEqual(d["room_temp_age_s"], 90)
        self.assertEqual(d["outside_temp_age_s"], 1800)

    def test_missing_flow_and_return_are_null_and_tr_feed_has_no_age(self):
        d = self.run_sc("noflow")
        self.assertIsNone(d["flow_temp"])
        self.assertIsNone(d["return_temp"])
        self.assertIs(d["flame_on"], False)
        self.assertEqual(d["room_temp_age_s"], -1)
        self.assertEqual(d["outside_temp_age_s"], -1)

    def test_no_room_reading_has_no_age(self):
        self.assertEqual(self.run_sc("boiler", room="nan")["room_temp_age_s"], -1)

    def test_ble_age_wins_over_thermostat(self):
        self.assertEqual(self.run_sc("ble")["room_temp_age_s"], 42)

    def test_multi_area_age_is_the_oldest_weighted_area(self):
        self.assertEqual(self.run_sc("areas")["room_temp_age_s"], 120)

    def test_external_age(self):
        self.assertEqual(self.run_sc("external")["room_temp_age_s"], 600)

    def test_fallback_estimate_has_no_age(self):
        self.assertEqual(self.run_sc("fallback", room="19.0")["room_temp_age_s"], -1)

    def test_simulation_uses_model_values_without_noise(self):
        d = self.run_sc("sim")
        self.assertAlmostEqual(d["flow_temp"], 55.25)
        self.assertAlmostEqual(d["return_temp"], 44.0)
        self.assertEqual(d["room_temp_age_s"], 0)

    def test_millis_wrap(self):
        self.assertEqual(self.run_sc("wrap", now=0x00001000)["room_temp_age_s"], 8)


class Wiring(unittest.TestCase):
    def setUp(self):
        self.assertTrue(BEGIN in SRC, "sat-status-live block missing")

    def body(self, sig):
        i = SRC.find(sig)
        self.assertGreaterEqual(i, 0, sig)
        return SRC[i:SRC.find("\n}\n", i)]

    def test_snapshot_fills_live_and_closure_emits_it(self):
        b = self.body("void satSendStatusJSON()")
        closure = b.index("restSendChunked(")
        fill = b.index("snap->live.flowTemp")
        self.assertLess(fill, closure, "the live values are frozen in the snapshot, before the closure")
        self.assertIn("satEmitStatusLive(je, snap->live.flowTemp", b[closure:])
        self.assertNotIn("satGetFlowTemp()", b, "satGetFlowTemp() advances the sim noise; REST must not call it")
        self.assertIn("satResolveOutsideTemp(&snap->outsideTemp", b)
        # the existing fields stay
        for k in ("\"room_temp\"", "\"outside_temp\"", "\"current_modulation\"", "\"sim_flow_temp\""):
            self.assertIn(k, b)

    def test_room_age_mirror_follows_satGetRoomTemp_source_order(self):
        order = ["bSimulation", "bBleTempValid", "bMultiArea", "bUseExternalTemp", "bFallbackActive"]
        def positions(text):
            return [text.index(k) for k in order]
        real = self.body("static float satGetRoomTemp()")
        mirror = self.body("static bool satRoomTempSampleMs(")
        self.assertEqual(sorted(positions(real)), positions(real))
        self.assertEqual(sorted(positions(mirror)), positions(mirror))


if __name__ == "__main__":
    unittest.main(verbosity=2)
