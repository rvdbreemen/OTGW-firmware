#!/usr/bin/env python3
"""TASK-3021 (entries 460, 461): the frost guard while SAT is off and no thermostat is seen.

Room <= 6.0 C -> the gateway sends CS=30 with CH=1 itself; room >= 7.0 C -> CS=0; no room temperature -> nothing. A thermostat
on the bus, SAT controlling, or a user CS= in force keep the guard out; SAT or a user CS= taking over ends it without a CS=0.

Nothing is re-implemented: the test cuts the real block from src/OTGW-firmware/SATcontrol.ino (the types between the satFrostTypes markers in SATtypes.h, and from the SAT_FROST_ON_C
constant to the '// --- satFrostGuard end' marker: satFrostGuardStep(), satFrostUserCsInForce(), satFrostGuardDecide()) and
compiles it with a stub SAT_CS_REPEAT_MS (30 s, SATcontrol.ino). The glue satFrostGuardService() (the room source, the command
queue, the debug lines) is not compiled here; the bench covers it.

Red on the code before TASK-3021 (the block does not exist: the gateway never asks for heat with SAT off and no thermostat),
green on the fix. Run: python tests/test_sat_frost_guard.py
"""
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from test_sat_pwm_override import compile_cpp  # noqa: E402

SAT_INO = HERE.parent / "src" / "OTGW-firmware" / "SATcontrol.ino"
TYPES_H = HERE.parent / "src" / "OTGW-firmware" / "SATtypes.h"   # SatFrostAction, SatFrostIn

PRE = r"""
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <cmath>
static const uint32_t SAT_CS_REPEAT_MS = 30000UL;
@CODE@
static const char* name(SatFrostAction a) {
  switch (a) { case SAT_FROST_ENTER: return "enter"; case SAT_FROST_REPEAT: return "repeat";
               case SAT_FROST_RELEASE: return "release"; case SAT_FROST_DROP: return "drop"; default: return "none"; }
}
int main(int argc, char** argv) {
  const char* sc = argv[1];
  if (!strcmp(sc, "step")) {            // the hysteresis alone: active room
    const bool active = atoi(argv[2]) != 0;
    const float room = !strcmp(argv[3], "nan") ? NAN : (float)atof(argv[3]);
    printf("%d\n", satFrostGuardStep(active, room) ? 1 : 0);
    return 0;
  }
  if (!strcmp(sc, "usercs")) {          // seen value ageMs
    printf("%d\n", satFrostUserCsInForce(atoi(argv[2]) != 0, (float)atof(argv[3]), (uint32_t)atol(argv[4])) ? 1 : 0);
    return 0;
  }
  // decide: active room mayAct thermostat otherOwner sinceSendMs
  SatFrostIn in;
  in.active      = atoi(argv[2]) != 0;
  in.room        = !strcmp(argv[3], "nan") ? NAN : (float)atof(argv[3]);
  in.mayAct      = atoi(argv[4]) != 0;
  in.thermostat  = atoi(argv[5]) != 0;
  in.otherOwner  = atoi(argv[6]) != 0;
  in.sinceSendMs = (uint32_t)atol(argv[7]);
  printf("%s\n", name(satFrostGuardDecide(in)));
  return 0;
}
"""


def cut_block(src):
    src = src.replace("\r\n", "\n")
    a = src.index("static const float    SAT_FROST_ON_C")
    b = src.index("// --- satFrostGuard end", a)
    types = TYPES_H.read_text(encoding="utf-8").replace("\r\n", "\n")
    ta = types.index("// --- satFrostTypes begin")
    tb = types.index("// --- satFrostTypes end", ta)
    return types[ta:tb] + "\n" + src[a:b]


class FrostGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.d = Path(tempfile.mkdtemp(prefix="sat_frost_"))
        cpp = cls.d / "frost.cpp"
        cpp.write_text(PRE.replace("@CODE@", cut_block(SAT_INO.read_text(encoding="utf-8"))), encoding="utf-8")
        cls.exe = compile_cpp(cpp, cls.d / "frost.exe")

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.d, ignore_errors=True)

    def run_exe(self, *args):
        if self.exe is None:
            self.skipTest("no host C++ compiler")
        return subprocess.run([str(self.exe), *[str(a) for a in args]], capture_output=True, text=True,
                              check=True).stdout.strip()

    def decide(self, active, room, may=1, thermostat=0, other=0, since=0):
        return self.run_exe("decide", active, room, may, thermostat, other, since)

    # AC1: SAT off, no thermostat, room <= 6.0 -> CS=30 + CH=1; >= 7.0 -> back to CH off (CS=0)
    def test_cold_room_without_thermostat_enters(self):
        self.assertEqual(self.decide(0, 5.0), "enter")
        self.assertEqual(self.decide(0, 6.0), "enter")

    def test_between_the_thresholds_keeps_the_state(self):
        self.assertEqual(self.decide(0, 6.5), "none")            # not yet on
        self.assertEqual(self.decide(1, 6.5, since=1000), "none")  # on, repeat not due
        self.assertEqual(self.decide(1, 6.9, since=30000), "repeat")

    def test_warm_room_releases(self):
        self.assertEqual(self.decide(1, 7.0, since=1000), "release")
        self.assertEqual(self.decide(0, 7.0), "none")

    # AC2: a thermostat on the bus -> no frost demand; one that appears while the guard is on ends it with CS=0
    def test_thermostat_connected_no_action(self):
        self.assertEqual(self.decide(0, 4.0, thermostat=1), "none")
        self.assertEqual(self.decide(1, 4.0, thermostat=1), "release")

    # AC3: no room temperature known -> no frost demand
    def test_no_room_temperature_no_demand(self):
        self.assertEqual(self.decide(0, "nan"), "none")
        self.assertEqual(self.decide(1, "nan"), "release")

    def test_sat_or_user_cs_owns_the_setpoint(self):
        self.assertEqual(self.decide(0, 4.0, other=1), "none")
        self.assertEqual(self.decide(1, 4.0, other=1), "drop")    # no CS=0: their setpoint stays

    def test_boot_grace_monitor_release_pending_wait(self):
        self.assertEqual(self.decide(0, 4.0, may=0), "none")

    def test_step_hysteresis(self):
        self.assertEqual(self.run_exe("step", 0, 6.0), "1")
        self.assertEqual(self.run_exe("step", 1, 6.99), "1")
        self.assertEqual(self.run_exe("step", 1, 7.0), "0")
        self.assertEqual(self.run_exe("step", 1, "nan"), "0")

    def test_user_cs_in_force(self):
        self.assertEqual(self.run_exe("usercs", 0, 40, 0), "0")
        self.assertEqual(self.run_exe("usercs", 1, 40, 129999), "1")
        self.assertEqual(self.run_exe("usercs", 1, 40, 130000), "0")   # expired, not repeated
        self.assertEqual(self.run_exe("usercs", 1, 5, 99999999), "1")  # below 8 C never expires
        self.assertEqual(self.run_exe("usercs", 1, 0, 0), "0")         # CS=0 released it


class FrostGuardWiring(unittest.TestCase):
    """Static checks on the glue that is not compiled above."""

    def setUp(self):
        self.sat = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")

    def test_service_runs_every_pass_also_with_sat_off(self):
        i = self.sat.index("void satControlLoop()\n{")
        body = self.sat[i:self.sat.index("\n}\n", i)]
        self.assertIn("satControlLoopRun();\n  satFrostGuardService();", body)

    def test_room_source_is_measured_only_and_keeps_room_src(self):
        i = self.sat.index("static float satFrostRoomTemp()")
        body = self.sat[i:self.sat.index("\n}\n", i)]
        self.assertNotIn("satGetRoomTemp", body)
        self.assertNotIn("bSimulation", body)
        self.assertIn("_sat_roomSrc = keepSrc;", body)

    def test_guard_commands_are_own_and_outside_the_sat_cs_cache(self):
        i = self.sat.index("static void satFrostGuardService()")
        body = self.sat[i:self.sat.index("\n}\n", i)]
        self.assertNotIn("satSendCS", body)
        self.assertNotIn("fLastSentCS", body)
        self.assertIn('satFrostSend("CS=30.0") && satFrostSend("CH=1")', body)
        self.assertIn('satFrostSend("CS=0")', body)

    def test_external_cs_is_noted_in_the_queue(self):
        core = (HERE.parent / "src" / "OTGW-firmware" / "OTGW-Core.ino").read_text(encoding="utf-8")
        self.assertIn("satNoteExternalCS(v);", core)



class FrostGuardDecidedPoints(unittest.TestCase):
    """One test per point of the register (entries 460, 461, 485) and per implementation choice (marked as such)."""

    def setUp(self):
        self.sat = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")

    # entry 485: fixed 6 C on, 7 C off, no setting
    def test_thresholds_are_fixed_6_and_7(self):
        self.assertRegex(self.sat, r"static const float\s+SAT_FROST_ON_C\s+= 6\.0f;")
        self.assertRegex(self.sat, r"static const float\s+SAT_FROST_OFF_C\s+= 7\.0f;")

    def test_no_setting_for_the_frost_guard(self):
        root = HERE.parent / "src" / "OTGW-firmware"
        for name in ("settingStuff.ino", "OTGW-firmware.h", "SATtypes.h", "SATsettings.h"):
            p = root / name
            if not p.exists():
                continue
            text = p.read_text(encoding="utf-8", errors="replace")
            # the guard's state flag in the SAT state struct is allowed; a threshold or enable setting is not
            self.assertNotRegex(text, r"(?i)\b(f|b|i)Frost(Threshold|Guard(Enable|On|Min|Max)|Temp|On|Off)\b", name)
            self.assertNotRegex(text, r"(?i)frost_?(threshold|enable|min|max)", name)

    # entries 460/461: both boards through the command queue, CS=30 + CH=1, release CS=0
    def test_both_boards_use_the_command_queue_with_cs30_ch1_and_release_cs0(self):
        i = self.sat.index("static bool satFrostSend(")
        self.assertIn("addCommandToQueue(", self.sat[i:i + 200])
        j = self.sat.index("static void satFrostGuardService()")
        body = self.sat[j:self.sat.index("\n}\n", j)]
        self.assertIn('satFrostSend("CS=30.0") && satFrostSend("CH=1")', body)
        self.assertIn('satFrostSend("CS=0")', body)
        self.assertNotIn("OTD", body)          # no board-specific path: the queue serves both
        self.assertNotIn("PICreturn", body)

    # entry 460: SAT off. Entry 460: with SAT controlling the guard stays out
    def test_sat_owning_the_setpoint_keeps_the_guard_out(self):
        j = self.sat.index("static void satFrostGuardService()")
        body = self.sat[j:self.sat.index("\n}\n", j)]
        self.assertIn("satOwnsControlSetpoint()", body)

    # entry 460: no thermostat seen = the bus thermostat state
    def test_no_thermostat_uses_the_bus_thermostat_state(self):
        j = self.sat.index("static void satFrostGuardService()")
        body = self.sat[j:self.sat.index("\n}\n", j)]
        self.assertIn("in.thermostat  = state.otBus.bThermostatState;", body)

    # entry 460: no room temperature -> nothing (decide, hysteresis and the room source returning NAN)
    def test_no_room_temperature_means_nothing_at_all(self):
        i = self.sat.index("static float satFrostRoomTemp()")
        body = self.sat[i:self.sat.index("\n}\n", i)]
        self.assertIn(": NAN;", body)
        self.assertNotIn("satThermostatRoomTemp", body)

    # implementation choice: the room chain is the chosen BLE sensor and the external sensor only (entries 457/458)
    def test_room_chain_is_ble_and_external_only(self):
        i = self.sat.index("static float satFrostRoomTemp()")
        body = self.sat[i:self.sat.index("\n}\n", i)]
        self.assertIn("SAT_ROOMSRC_BLE", body)
        self.assertIn("SAT_ROOMSRC_EXTERNAL", body)
        self.assertNotIn("SAT_ROOMSRC_AREAS", body)
        self.assertNotIn("SAT_ROOMSRC_THERMOSTAT", body)

    # implementation choices, each marked in the code as ours
    def test_implementation_choices_are_marked_as_such(self):
        self.assertIn("Implementation choices (NOT in the register; ours, TASK-3021)", self.sat)
        for word in ("monitor mode", "60 s grace", "130 s", "hand-back", "5 s", "about every 30 s"):
            self.assertIn(word, self.sat[self.sat.index("Implementation choices (NOT in the register"):][:900])

    def test_boot_grace_user_cs_window_and_eval_period_values(self):
        self.assertRegex(self.sat, r"SAT_FROST_BOOT_GRACE_MS\s+= 60000UL")
        self.assertRegex(self.sat, r"SAT_FROST_USER_CS_MS\s+= 130000UL")
        self.assertRegex(self.sat, r"SAT_FROST_EVAL_MS\s+= 5000UL")

    def test_monitor_mode_flashing_and_pending_handback_block_it(self):
        j = self.sat.index("static void satFrostGuardService()")
        body = self.sat[j:self.sat.index("\n}\n", j)]
        for part in ("!_sat_releasePending", "now >= SAT_FROST_BOOT_GRACE_MS", "!monitorMode", "!isFlashing()"):
            self.assertIn(part, body)

    def test_cs30_repeats_at_the_sat_cs_period_not_before(self):
        fg = FrostGuard
        fg.setUpClass()
        try:
            t = fg("test_cold_room_without_thermostat_enters")
            self.assertEqual(t.decide(1, 5.0, since=29999), "none")
            self.assertEqual(t.decide(1, 5.0, since=30000), "repeat")
        finally:
            fg.tearDownClass()


if __name__ == "__main__":
    unittest.main()
