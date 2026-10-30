#!/usr/bin/env python3
"""TASK-1282: the firmware-only humidity "comfort adjust" is gone (Sergeant D entry 114, Q32 C).

Source-level checks on the firmware tree. OTGW_FW_DIR points at another copy of
src/OTGW-firmware (e.g. a `git show HEAD:` export) for old-vs-fix runs; on HEAD the
comfort adjust is still present, so the removal cases fail there.

What stays: Thermal comfort (Summer Simmer Index as PID room temperature, thermo-nova
climate.py:276-277) and the weather humidity fallback (TASK-1441).
"""
import os
import re
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
FW = Path(os.environ.get("OTGW_FW_DIR", REPO / "src" / "OTGW-firmware"))
GOLDEN = REPO / "scripts" / "json-golden"

NAMES = re.compile(
    r"comfort_?adjust|satUpdateComfort|fComfortOffset|fComfortHumidity|fComfortMaxOffset|"
    r"comfort_offset|comfort_ref_humidity|comfort_max_offset|comfort_humidity|"
    r"comfortmaxoffset|comforthumidity|Comfort_Adjust|comfort-adjust",
    re.I,
)


def read(name):
    return (FW / name).read_text(encoding="utf-8", errors="replace")


class ComfortAdjustRemoved(unittest.TestCase):
    def test_no_comfort_adjust_name_left_in_firmware_sources(self):
        left = []
        for f in FW.rglob("*"):
            if f.suffix not in (".ino", ".cpp", ".h", ".js", ".html", ".css", ".ini") or not f.is_file():
                continue
            for n, line in enumerate(f.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
                # the retired-entity row is the one place that keeps the old topic name
                if NAMES.search(line) and "RETIRED-SAT-ENTITY" not in line:
                    left.append(f"{f.relative_to(FW).as_posix()}:{n}")
        self.assertEqual(left, [])

    def test_control_loop_no_longer_calls_the_comfort_update(self):
        sat = read("SATcontrol.ino")
        self.assertNotIn("satUpdateComfort", sat)

    def test_effective_target_is_setpoint_plus_pv_boost(self):
        sat = read("SATcontrol.ino")
        body = sat[sat.index("static float satEffectiveTarget()"):]
        body = body[:body.index("\n}\n")]
        self.assertIn("float effectiveTarget = settings.sat.fTargetTemp;", body)
        self.assertIn("fPvBoostAppliedC", body)

    def test_weather_humidity_fallback_is_gone(self):
        # TASK-1441 (entry 117): the outdoor weather humidity is not an indoor humidity source
        sat = read("SATcontrol.ino")
        self.assertNotIn("state.sat.fHumidity = state.sat.weather.fHumidity", sat)

    def test_thermal_comfort_ssi_path_uses_the_one_validity_rule(self):
        sat = read("SATcontrol.ino")
        self.assertIn("settings.sat.bThermalComfort", sat)
        self.assertIn("satHumidityFresh(millis())", sat)
        self.assertIn("satCalcSimmerIndex(otRoom, state.sat.fHumidity)", sat)
        self.assertIn("settings.sat.iHumidityTimeoutS", sat)
        self.assertIn('"SATthermalcomfort"', read("settingStuff.ino"))

    def test_command_dispatch_has_no_comfort_adjust(self):
        mq = read("MQTTstuff.ino")
        self.assertNotRegex(mq, r'"comfort_(adjust|humidity|max_offset)"')
        self.assertIn('"thermal_comfort"', mq)

    def test_ha_switch_is_gone_and_old_entity_is_retired(self):
        ha = read("MQTTHaDiscovery.cpp")
        self.assertNotIn("-sat-comfort-adjust-enable\"),   PSTR", ha)
        rows = [l for l in ha.splitlines() if "RETIRED-SAT-ENTITY" in l and "{" in l]
        self.assertTrue(any('"switch", "sat_comfort_adjust_enable", "/sat/comfort_adjust_enable"' in r for r in rows))
        # discovery switch table: contiguous cases 0..count-1 and one switch per case
        count = int(re.search(r"constexpr uint8_t SAT_HA_SWITCH_COUNT = (\d+);", read("MQTTstuff.h")).group(1))
        body = ha[ha.index("bool streamSatSwitchDiscovery("):]
        body = body[:body.index("default:")]
        cases = [int(c) for c in re.findall(r"^\s*case (\d+):", body, re.M)]
        self.assertEqual(cases, list(range(count)))
        self.assertEqual(count, 12)
        # the dhw_enable gate in the discovery loop still points at its case
        self.assertIn("if (swIdx == 10 && !dhwEnableSwitchAllowed) continue;", read("MQTTstuff.ino"))
        case10 = body[body.index("case 10:"):]
        self.assertIn("-sat-dhw-enable", case10[:case10.index("case 11:")])

    def test_old_settings_key_is_ignored_silently(self):
        # updateSetting() has no catch-all: an unknown key (SATcomfortadjust in an old file) falls
        # through the else-if chain and leaves the struct unchanged
        src = read("settingStuff.ino")
        body = src[src.index("void updateSetting("):src.index("} // updateSetting()")]
        self.assertNotIn("SATcomfort", body)
        self.assertNotRegex(body, r"(?i)unknown (setting|key|field)")

    def test_goldens_have_no_comfort_adjust_fields(self):
        for f in ("v2_debug.json", "v2_sat.json", "v2_sat_status.json", "v2_settings.json"):
            txt = (GOLDEN / f).read_text(encoding="utf-8")
            self.assertIsNone(NAMES.search(txt), f)


if __name__ == "__main__":
    unittest.main()
