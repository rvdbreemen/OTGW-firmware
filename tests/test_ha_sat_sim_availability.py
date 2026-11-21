#!/usr/bin/env python3
"""TASK-2014 / ADR-255: the Home Assistant "SAT Simulation" switch is unavailable while a boiler answers.

SAT refuses to enable simulation while a real boiler is on the bus (settingStuff.ino, TASK-795), so the HA switch
jumped back to off within a second, with the reason only on telnet and in the web UI. ADR-255 (Accepted, User
Decision Robert "A") gives this one switch a second availability topic:

(a) satPublishCoreState(), the SAT publisher that also runs while SAT is off, publishes the retained topic
    sat/simulation_available: "false" while a boiler answers and simulation is off, "true" otherwise (also while
    simulation is on, so it can always be switched off: the rule of the web UI switch). satRepublishCoreState()
    re-arms it.
(b) the discovery of the simulation switch lists the device availability topic plus sat/simulation_available
    with avty_mode "all"; every other switch keeps the single avty_t of ADR-102.

Nothing here re-implements the code under test. (a) cuts the real functions out of SATcontrol.ino and compiles them
with stubbed publishIfChanged*() / satBoilerHardwarePresent(). (b) uses test/host/test_ha_discovery_json.py, which
compiles MQTTHaDiscovery.cpp verbatim and records every publish of the real dispatcher. Not reachable from here:
Home Assistant itself (bench run with HA).

Old-vs-fix: OTGW_SRC_REV=<git rev> reads every source from that revision.
  OTGW_SRC_REV=origin/dev python tests/test_ha_sat_sim_availability.py   -> red before TASK-2014
  python tests/test_ha_sat_sim_availability.py                           -> green
"""
import json
import os
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(REPO / "test" / "host"))
from test_ha_climate_sat_follow import COMMON, body, build, read, run, segment  # noqa: E402
import test_ha_discovery_json as H  # noqa: E402

REV = os.environ.get("OTGW_SRC_REV") or None
TOPIC = "sat/simulation_available"


class T1CoreStatePublishesAvailability(unittest.TestCase):
    """(a) the real satPublishCoreState() and satRepublishCoreState()."""

    @classmethod
    def setUpClass(cls):
        sat = read("SATcontrol.ino")
        core_seg = segment(sat, "static SATShadowB s_core_enabled,", "static void satPublishCoreState()")
        core_fn = body(sat, "static void satPublishCoreState()")
        repub_fn = body(sat, "void satRepublishCoreState()")
        cls.ok = None not in (core_seg, core_fn, repub_fn)
        code = COMMON + r"""
struct SATShadowB { int8_t last; bool valid; };
struct SATShadowS { char last[24]; bool valid; };
static uint8_t g_satShadowEpoch = 0;
enum SATPreset { SAT_PRESET_NONE = 0 };
static const char* satGetPresetName(SATPreset) { return "none"; }
struct MqttS { bool bEnable = true; };
struct SatS { bool bEnabled = false, bUseExternalTemp = false, bSimulation = false; uint8_t iRoomSource = 0; };
static const char* satPrimaryRoomSourceName(uint8_t) { return "auto"; }   // TASK-3011: sat/room_source in the core set
struct Settings { MqttS mqtt; SatS sat; };
struct MqttT { bool bConnected = true; };
struct SatT { bool bActive = false, bSafetyTripped = false, bFallbackActive = false, bPushRefused = false, bPm25NoSlot = false,
  bRoomTempLost = false, bOtLinkLost = false, bOutsideTempMissing = false, bNoIgnition = false, bHeatingFailure = false;
  int iShVerdict = 0, eControlMode = 0; uint8_t iRequestedControlMode = 0; SATPreset eActivePreset = SAT_PRESET_NONE; };
struct StateT { MqttT mqtt; SatT sat; };
static Settings settings; static StateT state;
static bool g_boiler = false;
static bool satIsEnabled() { return false; }
static bool satBoilerHardwarePresent() { return g_boiler; }
static void satClimateBridgeForce() {}
static bool publishIfChangedB(const char* t, bool v, SATShadowB& s, bool retained) {
  if (s.valid && s.last == (int8_t)v) return false;
  s.last = (int8_t)v; s.valid = true;
  g_out.push_back(std::string(t) + "=" + (v ? "true" : "false") + (retained ? "" : "(not-retained)")); return true;
}
static bool publishIfChangedS(const char* t, const char* v, SATShadowS& s, bool) {
  if (s.valid && strcmp(s.last, v) == 0) return false;
  snprintf(s.last, sizeof s.last, "%s", v); s.valid = true; g_out.push_back(std::string(t) + "=" + v); return true;
}
""" + (core_seg or "") + (core_fn or "") + "\n" + (repub_fn or "") + r"""
int main(int argc, char** argv) {
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i]; size_t eq = a.find('=');
    std::string k = a.substr(0, eq), v = eq == std::string::npos ? "" : a.substr(eq + 1);
    if (k == "boiler") g_boiler = atoi(v.c_str()) != 0;
    else if (k == "sim") settings.sat.bSimulation = atoi(v.c_str()) != 0;
    else if (k == "republish") satRepublishCoreState();
    else if (k == "run") {
      g_out.clear(); satPublishCoreState(); printf("run:");
      for (auto& s : g_out) if (s.rfind("sat/simulation_", 0) == 0) printf(" %s", s.c_str());
      printf("\n");
    }
  }
  return 0;
}
"""
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = build(cls.tmp.name, "core", code) if cls.ok else None

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def runs(self, *steps):
        self.assertTrue(self.ok, "satPublishCoreState() / satRepublishCoreState() not found")
        if not self.exe:
            self.skipTest("no host C++ compiler")
        return [ln.split()[1:] for ln in run(self.exe, *steps).strip().split("\n")]

    def test_boiler_present_and_sim_off_is_unavailable(self):
        self.assertEqual(self.runs("boiler=1", "sim=0", "run"), [[f"{TOPIC}=false"]])

    def test_no_boiler_is_available(self):
        self.assertEqual(self.runs("boiler=0", "sim=0", "run"), [[f"{TOPIC}=true"]])

    def test_sim_on_stays_available_so_it_can_be_switched_off(self):
        self.assertEqual(self.runs("boiler=1", "sim=1", "run"), [[f"{TOPIC}=true"]])

    def test_boiler_appearing_flips_it_and_republish_resends(self):
        out = self.runs("boiler=0", "run", "boiler=1", "run", "run", "republish", "run")
        self.assertEqual(out, [[f"{TOPIC}=true"], [f"{TOPIC}=false"], [], [f"{TOPIC}=false"]])


class T2Discovery(unittest.TestCase):
    """(b) the real discovery dispatcher (test/host/test_ha_discovery_json.py compiles MQTTHaDiscovery.cpp)."""

    @classmethod
    def setUpClass(cls):
        root = H.GEN / "ha_discovery_json" / ("sim_avail_2014_old" if REV else "sim_avail_2014")
        H.export(REV, root)
        rec = H.build_and_run(f"TASK-2014 ({REV or 'working tree'})", root, REV)
        cls.ctx, cls.pubs = rec[0], rec[2]

    def by_comp(self, comp):
        out = {}
        for p in self.pubs:
            if not p["raw"]:
                continue
            parts = p["topic"].split("/")
            if parts[1] == comp:
                out[parts[3]] = json.loads(p["raw"].decode("utf-8"))
        return out

    def test_simulation_switch_needs_both_availability_topics(self):
        got = self.by_comp("switch").get("sat_simulation_enable")
        self.assertIsNotNone(got, "switch sat_simulation_enable is not announced")
        pub = self.ctx["pub"]
        self.assertNotIn("avty_t", got)
        self.assertEqual(got.get("avty_mode"), "all")
        self.assertEqual(got.get("avty"), [
            {"t": pub},
            {"t": f"{pub}/{TOPIC}", "pl_avail": "true", "pl_not_avail": "false"},
        ])
        self.assertEqual(got["stat_t"], f"{pub}/sat/simulation_enable")

    def test_other_switches_keep_the_single_device_availability(self):
        switches = self.by_comp("switch")
        others = {k: v for k, v in switches.items() if k != "sat_simulation_enable"}
        self.assertGreaterEqual(len(others), 5, sorted(switches))
        for name, cfg in others.items():
            self.assertEqual(cfg.get("avty_t"), self.ctx["pub"], name)   # ADR-102
            self.assertNotIn("avty", cfg, name)
            self.assertNotIn("avty_mode", cfg, name)


if __name__ == "__main__":
    unittest.main()
