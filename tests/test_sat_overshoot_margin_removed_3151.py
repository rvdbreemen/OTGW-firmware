#!/usr/bin/env python3
"""TASK-3151.3 (Robert 2026-11-28 'ONE overshoot switch to PWM margin and setting', ADR-306): the second margin setting
satovershootmargin (settings.sat.fOvershootMargin) is gone. One margin, SATpwmswitchmargin (default 3.0), is the switch to
PWM, the end-of-cycle OVERSHOOT verdict and the overshoot seconds of the cycle statistics.

(1) the entity SAT_Overshoot_Margin is retired: its retained discovery config AND its retained state topic sat/overshoot_margin
    are emptied on every SAT discovery pass (the real clearRetiredSatEntities() and table are compiled and run);
(2) the removed names survive only on RETIRED-SAT-ENTITY lines: no setting, REST, MQTT, settings.ini writer or parser, debug or
    help line carries them; an old SATovershootmargin line in settings.ini therefore matches no handler and is ignored on load;
(3) the cycle code reads fPwmSwitchMargin where it read fOvershootMargin (verdict, overshoot seconds).

OTGW_SRC_REV=<rev> reads the sources from another revision (old-vs-fix): red on the code before this change, green after.
Run: python tests/test_sat_overshoot_margin_removed_3151.py
"""
import re
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_mqtt_contract_1485 import COMMON, body, build, read, run, segment  # noqa: E402

REMOVED = re.compile(r"overshoot_margin|SATovershootmargin|satovershootmargin|fOvershootMargin")
FILES = ("SATtypes.h", "SATcontrol.ino", "SATcycles.ino", "MQTTHaDiscovery.cpp", "MQTTstuff.ino", "restAPI.ino",
         "settingStuff.ino", "handleDebug.ino", "settingsHelp.h")


class RetiredEntity(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        disc = read("MQTTHaDiscovery.cpp")
        table = segment(disc, "struct RetiredSatEntity {", "void clearRetiredSatEntities(")
        fn = body(disc, "void clearRetiredSatEntities(const HaDiscoveryContext &ctx)")
        code = COMMON + r"""
#define STREAM_TOPIC_MAX 128
struct HaDiscoveryContext { const char* mqttPubTopic; const char* haPrefix; const char* nodeId; };
static bool publishEmptyRetained(const char* t) { g_out.push_back(t); return true; }
""" + table + fn + r"""
int main() {
  HaDiscoveryContext ctx{ "OTGW/value/otgw-1", "homeassistant", "otgw-1" };
  clearRetiredSatEntities(ctx);
  for (auto& s : g_out) printf("%s\n", s.c_str());
  return 0;
}
"""
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = build(cls.tmp.name, "ret3151", code)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_discovery_config_and_retained_state_topic_are_emptied(self):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        out = run(self.exe)
        self.assertIn("homeassistant/sensor/otgw-1/sat_sat_overshoot_margin/config", out)
        self.assertIn("OTGW/value/otgw-1/sat/overshoot_margin", out)


class NamesAreGone(unittest.TestCase):
    def test_removed_names_only_on_retired_lines(self):
        for rel in FILES:
            for n, line in enumerate(read(rel).splitlines(), 1):
                if REMOVED.search(line):
                    self.assertIn("RETIRED-SAT-ENTITY", line, f"{rel}:{n}: {line.strip()[:120]}")

    def test_the_one_margin_is_still_a_setting(self):
        self.assertIn("fPwmSwitchMargin", read("SATtypes.h"))
        self.assertIn('"SATpwmswitchmargin"', read("settingStuff.ino"))
        self.assertIn('"satpwmswitchmargin"', read("restAPI.ino"))

    def test_old_key_has_no_handler_so_it_is_ignored_on_load(self):
        src = read("settingStuff.ino")
        self.assertNotRegex(src, r'strcasecmp_P\(field, PSTR\("SATovershootmargin"\)\)')


class CycleCodeUsesTheOneMargin(unittest.TestCase):
    def test_verdict_and_overshoot_seconds_read_the_switch_margin(self):
        src = read("SATcycles.ino")
        self.assertIn("ctrlErrP90 >= settings.sat.fPwmSwitchMargin", src)
        self.assertIn("flowTemp > _cycle_setpointAtStart + settings.sat.fPwmSwitchMargin", src)
        self.assertIn("ctrlErr >= settings.sat.fPwmSwitchMargin", src)


if __name__ == "__main__":
    unittest.main(verbosity=2)
