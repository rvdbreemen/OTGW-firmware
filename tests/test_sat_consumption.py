#!/usr/bin/env python3
"""TASK-1484: sat/consumption (gas m3/h) follows the min/max consumption settings.

thermo-nova "Current Consumption (Device)" (custom_components/sat/sensor.py:219-262, pinned
7beaaf5): the sensor exists only when minimum and maximum consumption are both > 0
(sensor.py:62-66); its value is 0 unless the coordinator is active (for the OpenTherm Gateway via MQTT: the boiler's
CH-mode bit, coordinator/mqtt/opentherm.py:60-61) and the flame is on (sensor.py:252), otherwise
min + modulation/100 * (max - min), rounded to 3 decimals (sensor.py:252-259).

Behaviour part: the test cuts the REAL "Gas consumption m3/h" block out of satPublishMQTT() in
src/OTGW-firmware/SATcontrol.ino (brace-matched from its comment anchor), wraps it in stubs
(settings, state, satIsFlameOn, isCentralHeatingActive, OTcurrentSystemState, a publishIfChangedF that records topic and
value) and compiles it with a host C++ compiler. Without a compiler the cases are skipped.

Persistence part: the new keys must round-trip through the REAL settings writer/reader
(writeJsonFloatKV -> parseJsonKVLine -> clampSettingFloat, the harness of
tests/test_settings_float_roundtrip.py), and the REST GET field, POST whitelist, v2 UI field and
help text must name them (source checks).

Old-vs-fix: OTGW_SAT_INO=<old SATcontrol.ino> and OTGW_SETTINGSTUFF_INO=<old settingStuff.ino>
(read by the round-trip harness) point at another revision.

Run: python tests/test_sat_consumption.py
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
SRC = REPO / "src" / "OTGW-firmware"
SAT_INO = Path(os.environ.get("OTGW_SAT_INO", SRC / "SATcontrol.ino"))
ANCHOR = "// Gas consumption m3/h (Task #52)."

STUBS = r"""
#include <cstdio>
#include <cstdlib>
#include <cstring>
struct __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper*>(s))
struct SatSettings { float fMinConsumption = 0, fMaxConsumption = 0; bool bSimulation = false; };
struct Settings { SatSettings sat; };
struct SatState { bool bActive = false; };
struct State { SatState sat; };
struct OTSys { float RelModLevel = 0; unsigned char SlaveStatus = 0; };
struct SATShadowF { float last; unsigned nextRepublishMs; bool valid; };
static Settings settings; static State state; static OTSys OTcurrentSystemState;
static SATShadowF s_consumption;
static const float SAT_EPS_FRACTION = 0.005f;
static bool g_flame = false;
static bool satIsFlameOn() { return g_flame; }
static bool isCentralHeatingActive() { return OTcurrentSystemState.SlaveStatus & 0x02; }
static int g_pub = 0; static char g_topic[64] = ""; static float g_val = -1;
static bool publishIfChangedF(const __FlashStringHelper* t, float v, SATShadowF&, float, int dec, bool) {
  g_pub++; snprintf(g_topic, sizeof g_topic, "%s", reinterpret_cast<const char*>(t)); g_val = v; (void)dec; return true;
}
"""

MAIN = r"""
// argv: min max satActive flame mod chMode sim
int main(int argc, char** argv) {
  if (argc != 8) return 2;
  settings.sat.fMinConsumption = (float)atof(argv[1]);
  settings.sat.fMaxConsumption = (float)atof(argv[2]);
  state.sat.bActive = atoi(argv[3]) != 0;
  g_flame = atoi(argv[4]) != 0;
  OTcurrentSystemState.RelModLevel = (float)atof(argv[5]);
  OTcurrentSystemState.SlaveStatus = atoi(argv[6]) ? 0x02 : 0x04;  // CH mode, else DHW mode
  settings.sat.bSimulation = atoi(argv[7]) != 0;
  run_block();
  printf("pub=%d topic=%s val=%.4f\n", g_pub, g_pub ? g_topic : "-", g_val);
  return 0;
}
"""


def extract_block(src: str) -> str:
    i = src.find(ANCHOR)
    if i < 0:
        raise AssertionError("consumption block anchor not found in " + str(SAT_INO))
    j = src.index("{", i)
    depth = 0
    for k in range(j, len(src)):
        depth += src[k] == "{"
        depth -= src[k] == "}"
        if depth == 0:
            return src[j:k + 1]
    raise AssertionError("unterminated consumption block")


class ConsumptionPublish(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        src = SAT_INO.read_text(encoding="utf-8")
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text(STUBS + "static void run_block()\n" + extract_block(src) + "\n" + MAIN, encoding="utf-8")
        cls.exe = compile_cpp(cpp, Path(cls.tmp.name) / "t.exe")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_block(self, mn, mx, active, flame, mod, ch=True, sim=False):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe), str(mn), str(mx), str(int(active)), str(int(flame)), str(mod),
                              str(int(ch)), str(int(sim))],
                             capture_output=True, text=True, check=True).stdout
        m = re.match(r"pub=(\d+) topic=(\S+) val=(-?[\d.]+)", out)
        self.assertIsNotNone(m, out)
        return int(m.group(1)), m.group(2), float(m.group(3))

    def test_formula_when_active_and_flame_on(self):
        pub, topic, val = self.run_block(0.3, 2.5, True, True, 40)
        self.assertEqual((pub, topic), (1, "sat/consumption"), "sat/consumption must be published when min/max > 0")
        self.assertAlmostEqual(val, 0.3 + 0.40 * (2.5 - 0.3), places=4)

    def test_full_and_zero_modulation(self):
        self.assertAlmostEqual(self.run_block(0.3, 2.5, True, True, 100)[2], 2.5, places=4)
        self.assertAlmostEqual(self.run_block(0.3, 2.5, True, True, 0)[2], 0.3, places=4)

    def test_zero_when_flame_off(self):
        self.assertEqual(self.run_block(0.3, 2.5, True, False, 40)[:2], (1, "sat/consumption"))
        self.assertEqual(self.run_block(0.3, 2.5, True, False, 40)[2], 0.0)

    def test_zero_when_boiler_not_in_ch_mode(self):
        # thermo-nova sensor.py:252 returns 0 unless coordinator.active, which the OTGW-via-MQTT
        # coordinator reads from the boiler's CH-mode bit (coordinator/mqtt/opentherm.py:60-61).
        # A hot-water draw (DHW bit, flame on, SAT loop running) is not CH: 0.
        self.assertEqual(self.run_block(0.3, 2.5, True, True, 40, ch=False)[:2], (1, "sat/consumption"))
        self.assertEqual(self.run_block(0.3, 2.5, True, True, 40, ch=False)[2], 0.0, "DHW draw must read 0")

    def test_simulation_flame_stands_in_for_ch_mode(self):
        # Simulation has no boiler status; the thermo-nova simulator's flame implies active
        # (coordinator/simulator/__init__.py:58-63).
        self.assertAlmostEqual(self.run_block(0.3, 2.5, True, True, 40, ch=False, sim=True)[2], 0.3 + 0.4 * 2.2, places=4)
        self.assertEqual(self.run_block(0.3, 2.5, True, False, 40, ch=False, sim=True)[2], 0.0)

    def test_not_published_unless_both_settings_above_zero(self):
        for mn, mx in ((0, 0), (0.3, 0), (0, 2.5)):
            self.assertEqual(self.run_block(mn, mx, True, True, 40)[0], 0, f"min={mn} max={mx} must not publish")


class ConsumptionSettings(unittest.TestCase):
    KEYS = ("SATminconsumption", "SATmaxconsumption")

    @classmethod
    def setUpClass(cls):
        from test_settings_float_roundtrip import FloatRoundTrip
        FloatRoundTrip.setUpClass()   # compiles the real writer/reader; SkipTest without a compiler
        cls.rt = FloatRoundTrip

    @classmethod
    def tearDownClass(cls):
        cls.rt.tearDownClass()

    def test_keys_round_trip_through_settings_ini(self):
        cases = []
        sstuff = Path(os.environ.get("OTGW_SETTINGSTUFF_INO", SRC / "settingStuff.ino")).read_text(encoding="utf-8")
        for key, field in zip(self.KEYS, ("fMinConsumption", "fMaxConsumption")):
            # the updateSetting() line of this key assigns to its own field, clamped from that field
            pat = (r'strcasecmp_P\(field, PSTR\("' + key + r'"\)\) == 0\)\s*settings\.sat\.' + field
                   + r'\s*=\s*clampSettingFloat\(newValue, settings\.sat\.' + field + r',')
            self.assertIsNotNone(re.search(pat, sstuff), key + ": updateSetting() must assign settings.sat." + field)
        for key in self.KEYS:
            self.assertIn(key, self.rt.decimals, key + ": not written by writeSettings()")
            self.assertEqual(self.rt.rng.get(key), (0.0, 8.0), key + ": clamp range in updateSetting() must be 0..8")
            for v in (0.0, 0.35, 2.5, 8.0):
                cases.append((key, v, self.rt.decimals[key], 0.0, 8.0))
        cases.append(("SATmaxconsumption", 9.5, self.rt.decimals["SATmaxconsumption"], 0.0, 8.0))  # clamps
        out = self.rt.run_cases(self.rt, cases)
        print("\n  key, text in file, value read back:", out)
        for (key, v, *_), (_, _, got) in zip(cases, out):
            self.assertAlmostEqual(float(got), min(v, 8.0), places=4, msg=key)

    def test_rest_ui_help_name_the_keys(self):
        rest = (SRC / "restAPI.ino").read_text(encoding="utf-8")
        v2 = (SRC / "data" / "v2.js").read_text(encoding="utf-8")
        hlp = (SRC / "settingsHelp.h").read_text(encoding="utf-8")
        types = (SRC / "SATtypes.h").read_text(encoding="utf-8")
        for f in ("fMinConsumption", "fMaxConsumption"):
            self.assertRegex(types, r"float\s+" + f + r"\s*=\s*0\.0f;", f + " missing in SatSettings (default 0 = off)")
        start = rest.index("static const char* const PROGMEM knownSettings[] = {")
        whitelist = rest[start:rest.index("};", start)]
        for k in ("satminconsumption", "satmaxconsumption"):
            self.assertRegex(rest, r'addNum\(F\("' + k + r'"\),\s*tmpBuf, "f", 0, 8\)', k + ": no REST GET field")
            self.assertIn('"' + k + '"', whitelist, k + ": not in the POST whitelist (knownSettings)")
            self.assertRegex(v2, k + r":\s*\{ cat: 'sat'", k + ": no v2 label entry")
            l3 = v2[v2.index("var SAT_L3"):v2.index("var SAT_NOT_IN_FORMS")]
            self.assertIn("'" + k + "'", l3, k + ": not in the SAT Technical (L3) groups")
            self.assertIn('"' + k + '":', hlp, k + ": no settingsHelp entry")


if __name__ == "__main__":
    unittest.main(verbosity=2)
