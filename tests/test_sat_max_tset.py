#!/usr/bin/env python3
"""TASK-1373: host test for the SAT user Max flow setpoint (MaxTSet), Sergeant D entries 48, 100, 148.

The code under test is NOT re-implemented here. The test cuts the real block
(`--- User Max flow setpoint` .. satMaxTSetSendStep(), plus satEffectiveMaxSetpoint()) out of
src/OTGW-firmware/SATcontrol.ino, the real parseMaxTSetUser() out of settingStuff.ino and the real
satMaxTSetInputOk() out of SATtypes.h, wraps them in stubs (settings, OT state, command queue) and
compiles them with a host C++ compiler (g++, clang++ or MSVC cl via vswhere). Without a compiler the
tests are skipped, they do not pass silently.

Covered: not chosen -> no SH= ever, CS capped at 62 radiators / 50 underfloor; chosen 70 -> SH=65 once
(the global SAT ceiling caps CS and SH= alike, interim choice pending Sergeant D's Q-b); chosen 60 ->
SH=60 once, CS cap 60; a boot with a chosen value -> exactly one SH=; MsgID 49 bounds clamp; underfloor
<= 50; SH= never carries the heat pump 40 cap; going back to 0 sends nothing; the setter rejects a
typo 1..19.

Each process run is one boot: _sat_lastSentSH starts at -1 exactly as after a restart.

Old-vs-fix: OTGW_SAT_INO / OTGW_SETTINGS_INO / OTGW_SATTYPES_H point at other copies, e.g. the output
of `git show HEAD:src/OTGW-firmware/SATcontrol.ino`; on HEAD the block does not exist, the harness
cannot be built and every case errors (red).

Run: python tests/test_sat_max_tset.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_sat_pwm_override as base  # noqa: E402  (shared compiler helpers)

REPO = base.REPO
FW = REPO / "src" / "OTGW-firmware"
SAT_INO = Path(os.environ.get("OTGW_SAT_INO", FW / "SATcontrol.ino"))
SETTINGS_INO = Path(os.environ.get("OTGW_SETTINGS_INO", FW / "settingStuff.ino"))
SATTYPES_H = Path(os.environ.get("OTGW_SATTYPES_H", FW / "SATtypes.h"))

STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#define F(x) x
#define PSTR(x) x
#define SATDebugTf(...) ((void)0)
#define DebugTf(...) ((void)0)
#define snprintf_P snprintf
template <typename T> static T constrain(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }
enum { SAT_HSYS_AUTO = 0, SAT_HSYS_RADIATORS = 1, SAT_HSYS_UNDERFLOOR = 2 };
enum { SAT_SRC_AUTO = 0, SAT_SRC_GAS_BOILER = 1, SAT_SRC_HEAT_PUMP = 2, SAT_SRC_HYBRID = 3 };
#define SAT_MAXTSET_MIN 20
#define SAT_MAXTSET_MAX 80
struct SatSettings { float fMaxTSetUser = 0.0f; float fMaxSetpoint = 65.0f; bool bEnabled = true; int iHeatingSystem = SAT_HSYS_RADIATORS; int iHeatingSource = SAT_SRC_GAS_BOILER; };
struct Settings { SatSettings sat; };
struct OTSys { float MaxTSet = 0.0f; uint16_t MaxTSetUBMaxTSetLB = 0; };
static Settings settings; static OTSys OTcurrentSystemState;
static bool satIsEnabled() { return settings.sat.bEnabled; }
static bool g_hasOT = true;
static bool hasOTCommandInterface() { return g_hasOT; }
static uint8_t satGetEffectiveHeatingSystem() { return (uint8_t)settings.sat.iHeatingSystem; }
static uint8_t satGetEffectiveHeatingSource() { return (uint8_t)settings.sat.iHeatingSource; }
static std::vector<std::string> g_queue;
static void addCommandToQueue(const char* cmd, size_t len, bool, int) { g_queue.push_back(std::string(cmd, len)); }
@CONSTS@
@TYPES@
"""

MAIN = r"""
static void dump() {
  bool chosen = false;
  const bool ug = satGetEffectiveHeatingSystem() == SAT_HSYS_UNDERFLOOR;
  const float boilerVal = satMaxTSetBoilerValue(settings.sat.fMaxTSetUser, ug, OTcurrentSystemState.MaxTSetUBMaxTSetLB, satGlobalMaxSetpoint(), &chosen);
  printf("chosen=%d boiler=%.1f sysMax=%.1f effCS=%.1f queue=", (int)chosen, boilerVal, satGetMaxSetpoint(), satEffectiveMaxSetpoint());
  for (size_t i = 0; i < g_queue.size(); i++) printf("%s%s", i ? "," : "", g_queue[i].c_str());
  printf("\n");
}
int main(int argc, char** argv) {
  if (argc < 2) return 2;
  if (!strcmp(argv[1], "setter") && argc >= 4) {   // setter value current
    printf("stored=%.1f ok=%d\n", parseMaxTSetUser(argv[2], (float)atof(argv[3])), (int)satMaxTSetInputOk((float)strtof(argv[2], nullptr)));
    return 0;
  }
  // boot user system source rawBounds boilerMaxTSet [step ...]: step = tick | set=<v> | raw=<n> | src=<n> | glob=<v> | sat=<0|1>
  if (!strcmp(argv[1], "boot") && argc >= 7) {
    settings = Settings(); OTcurrentSystemState = OTSys();
    settings.sat.fMaxTSetUser = (float)atof(argv[2]);
    settings.sat.iHeatingSystem = atoi(argv[3]);
    settings.sat.iHeatingSource = atoi(argv[4]);
    OTcurrentSystemState.MaxTSetUBMaxTSetLB = (uint16_t)atoi(argv[5]);
    OTcurrentSystemState.MaxTSet = (float)atof(argv[6]);
    for (int i = 7; i < argc; i++) {
      if (!strcmp(argv[i], "tick")) satMaxTSetSendStep();
      else if (!strncmp(argv[i], "set=", 4)) settings.sat.fMaxTSetUser = (float)atof(argv[i] + 4);
      else if (!strncmp(argv[i], "raw=", 4)) OTcurrentSystemState.MaxTSetUBMaxTSetLB = (uint16_t)atoi(argv[i] + 4);
      else if (!strncmp(argv[i], "src=", 4)) settings.sat.iHeatingSource = atoi(argv[i] + 4);
      else if (!strncmp(argv[i], "glob=", 5)) settings.sat.fMaxSetpoint = (float)atof(argv[i] + 5);
      else if (!strncmp(argv[i], "sat=", 4)) settings.sat.bEnabled = atoi(argv[i] + 4) != 0;
    }
    dump();
    return 0;
  }
  return 2;
}
"""


def cut(src: str, start: str, end: str) -> str:
    a = src.index(start)
    return src[a:src.index(end, a)]


def build(workdir: Path):
    src = SAT_INO.read_text(encoding="utf-8")
    consts = "\n".join(re.search(r"^static const float\s+" + n + r"\b.*$", src, re.M).group(0)
                       for n in ("SAT_HARD_MAX_FLOOR", "SAT_HARD_MAX_RAD", "SAT_GLOBAL_MAX_SETPOINT", "SAT_MAX_SETPOINT_DEFAULT"))
    block = cut(src, "// --- User Max flow setpoint", "// Returns heating curve base offset")   # block + satEffectiveMaxSetpoint()
    types = SATTYPES_H.read_text(encoding="utf-8")
    ok = re.search(r"^static inline bool satMaxTSetInputOk\(float v\).*$", types, re.M).group(0)
    setter = SETTINGS_INO.read_text(encoding="utf-8")
    parse = ""
    for fn in ("static float clampSettingFloat(", "static float parseMaxTSetUser("):   # the real helper and the real setter
        a = setter.index(fn)
        parse += setter[a:setter.index("\n}", a) + 2] + "\n"
    cpp = workdir / "maxtset.cpp"
    cpp.write_text(STUBS.replace("@CONSTS@", consts).replace("@TYPES@", ok) + block + parse + MAIN, encoding="utf-8")
    return base.compile_cpp(cpp, workdir / "maxtset.exe")


RAD, FLOOR = 1, 2
GAS, HP = 1, 2


def raw(lb, ub):
    return (ub << 8) | lb


class SatMaxTSet(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = build(Path(cls.tmp.name))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def setUp(self):
        if not self.exe:
            self.skipTest("no host C++ compiler (g++, clang++ or MSVC)")

    def boot(self, user, system=RAD, source=GAS, bounds=0, boiler=0, steps=()):
        out = subprocess.run([str(self.exe), "boot", *map(str, (user, system, source, bounds, boiler, *steps))],
                             capture_output=True, text=True, check=True).stdout
        d = dict(kv.split("=", 1) for kv in out.split())
        d["queue"] = [c for c in d.get("queue", "").split(",") if c]
        return d

    def setter(self, value, current):
        out = subprocess.run([str(self.exe), "setter", value, current], capture_output=True, text=True, check=True).stdout
        return dict(kv.split("=") for kv in out.split())

    # --- not chosen: default ceiling, nothing sent ---
    def test_not_chosen_radiators_default_62_and_no_sh(self):
        r = self.boot(0, steps=["tick", "tick", "tick"])
        self.assertEqual(r["queue"], [])
        self.assertEqual(r["chosen"], "0")
        self.assertEqual(float(r["effCS"]), 62.0)
        self.assertEqual(float(r["sysMax"]), 62.0)

    def test_not_chosen_underfloor_default_50_and_no_sh(self):
        r = self.boot(0, system=FLOOR, steps=["tick", "tick"])
        self.assertEqual(r["queue"], [])
        self.assertEqual(float(r["effCS"]), 50.0)

    def test_not_chosen_boiler_maxtset_below_default_still_caps_cs(self):
        r = self.boot(0, boiler=55)
        self.assertEqual(float(r["effCS"]), 55.0)        # the boiler's own MaxTSet is lower than 62
        self.assertEqual(r["queue"], [])

    # --- chosen ---
    def test_chosen_70_global_cap_sh_equals_cs_cap(self):
        # interim choice (Q-b pending): the 65 global ceiling caps CS and SH= alike
        r = self.boot(70, boiler=70, steps=["tick", "tick", "tick"])
        self.assertEqual(r["queue"], ["SH=65"])
        self.assertEqual(float(r["effCS"]), 65.0)

    def test_chosen_60_sends_sh_once_and_caps_cs_at_60(self):
        r = self.boot(60, boiler=60, steps=["tick", "tick", "tick"])
        self.assertEqual(r["queue"], ["SH=60"])
        self.assertEqual(float(r["effCS"]), 60.0)

    def test_change_sends_again_once(self):
        r = self.boot(60, steps=["tick", "tick", "set=55", "tick", "tick"])
        self.assertEqual(r["queue"], ["SH=60", "SH=55"])

    def test_boot_with_chosen_value_exactly_one_sh(self):
        for _ in range(2):   # two separate boots, each sends once, never per tick
            r = self.boot(58, steps=["tick"] * 10)
            self.assertEqual(r["queue"], ["SH=58"])

    def test_back_to_zero_sends_nothing(self):
        r = self.boot(60, steps=["tick", "set=0", "tick", "tick"])
        self.assertEqual(r["queue"], ["SH=60"])
        self.assertEqual(r["chosen"], "0")
        self.assertEqual(float(r["effCS"]), 62.0)          # SAT is back on its default ceiling

    def test_sat_disabled_sends_nothing(self):
        r = self.boot(60, steps=["sat=0", "tick"])
        self.assertEqual(r["queue"], [])

    # --- MsgID 49 bounds ---
    def test_bounds_clamp_upper(self):
        r = self.boot(75, bounds=raw(30, 55), steps=["tick"])
        self.assertEqual(r["queue"], ["SH=55"])
        self.assertEqual(float(r["effCS"]), 55.0)

    def test_bounds_clamp_lower(self):
        r = self.boot(25, bounds=raw(35, 70), steps=["tick"])
        self.assertEqual(r["queue"], ["SH=35"])

    def test_bounds_arriving_later_resend_the_clamped_value(self):
        r = self.boot(75, steps=["tick", "raw=%d" % raw(30, 55), "tick"])
        self.assertEqual(r["queue"], ["SH=65", "SH=55"])

    def test_invalid_bounds_fall_back_to_20_80(self):
        # upper below lower and upper above 100 are not valid: 20..80 applies
        for bad in (raw(70, 30), raw(30, 110)):
            r = self.boot(25, bounds=bad, steps=["tick"])
            self.assertEqual(r["queue"], ["SH=25"], bad)
        r = self.boot(0, bounds=raw(30, 55))               # the default 62 is cut into the boiler range
        self.assertEqual(float(r["effCS"]), 55.0)

    # --- underfloor never above 50 ---
    def test_underfloor_never_above_50(self):
        r = self.boot(70, system=FLOOR, steps=["tick"])
        self.assertEqual(r["queue"], ["SH=50"])
        self.assertEqual(float(r["effCS"]), 50.0)

    def test_underfloor_below_50_kept(self):
        r = self.boot(42, system=FLOOR, steps=["tick"])
        self.assertEqual(r["queue"], ["SH=42"])
        self.assertEqual(float(r["effCS"]), 42.0)

    # --- global SAT ceiling (settings.sat.fMaxSetpoint) shared by CS and SH= ---
    def test_global_ceiling_from_settings_caps_sh_and_cs(self):
        r = self.boot(70, steps=["glob=60", "tick"])
        self.assertEqual(r["queue"], ["SH=60"])
        self.assertEqual(float(r["effCS"]), 60.0)

    def test_global_ceiling_out_of_range_falls_back_to_65(self):
        r = self.boot(70, steps=["glob=10", "tick"])
        self.assertEqual(r["queue"], ["SH=65"])

    # --- heat pump: the 40 cap stays inside the CS cap, never in SH= ---
    def test_heat_pump_cap_in_cs_not_in_sh(self):
        r = self.boot(55, source=HP, steps=["tick"])
        self.assertEqual(r["queue"], ["SH=55"])
        self.assertEqual(float(r["effCS"]), 40.0)
        self.assertEqual(float(r["sysMax"]), 40.0)

    def test_heat_pump_not_chosen_sends_nothing_cs_40(self):
        r = self.boot(0, source=HP, steps=["tick"])
        self.assertEqual(r["queue"], [])
        self.assertEqual(float(r["effCS"]), 40.0)

    # --- setter: 0 = not chosen, 1..19 is a typo ---
    def test_setter_rejects_typo_1_to_19_keeps_old(self):
        for v in ("1", "5", "19", "19.9", "0.5"):
            r = self.setter(v, "60")
            self.assertEqual(float(r["stored"]), 60.0, v)
            self.assertEqual(r["ok"], "0", v)

    def test_setter_accepts_zero_and_range(self):
        self.assertEqual(float(self.setter("0", "60")["stored"]), 0.0)
        self.assertEqual(float(self.setter("20", "0")["stored"]), 20.0)
        self.assertEqual(float(self.setter("55.5", "0")["stored"]), 55.5)

    def test_setter_cuts_above_80_and_ignores_garbage(self):
        self.assertEqual(float(self.setter("120", "0")["stored"]), 80.0)
        self.assertEqual(float(self.setter("nan", "60")["stored"]), 60.0)
        self.assertEqual(float(self.setter("-5", "60")["stored"]), 60.0)


if __name__ == "__main__":
    unittest.main()
