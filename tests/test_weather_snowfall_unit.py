#!/usr/bin/env python3
"""TASK-1464: the Home Assistant unit of sat/weather/snowfall matches what Open-Meteo sends.

Open-Meteo reports snowfall in centimetres (docs: "cm (inch)", "Snowfall amount of the preceding hour in
centimeters"; live API: current_units.snowfall = "cm"). The firmware requests snowfall without a unit
parameter (SATweather.ino) but announced mm. The discovery row now uses HaUnit::cm.

Old-vs-fix: OTGW_SRC_DIR points at a directory with older copies of MQTTHaDiscovery.cpp and MQTTstuff.h.

Run: python tests/test_weather_snowfall_unit.py
"""
import os
import re
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
SRC = Path(os.environ.get("OTGW_SRC_DIR", REPO / "src" / "OTGW-firmware"))


class SnowfallUnit(unittest.TestCase):
    def test_discovery_row_announces_cm(self):
        ha = (SRC / "MQTTHaDiscovery.cpp").read_text(encoding="utf-8")
        row = [l for l in ha.splitlines() if "ha_name_sat_weather_snowfall," in l and "HaUnit::" in l]
        self.assertEqual(len(row), 1, row)
        self.assertIn("HaUnit::cm,", row[0])

    def test_cm_unit_is_rendered(self):
        ha = (SRC / "MQTTHaDiscovery.cpp").read_text(encoding="utf-8")
        self.assertRegex(ha, r'case HaUnit::cm: \{ static const char s\[\] PROGMEM = "cm"; return s; \}')
        self.assertRegex((SRC / "MQTTstuff.h").read_text(encoding="utf-8"), r"\n\s*cm,\s*//")

    def test_request_sets_no_snowfall_unit(self):
        w = (SRC / "SATweather.ino").read_text(encoding="utf-8")
        self.assertNotIn("precipitation_unit=inch", w)


if __name__ == "__main__":
    unittest.main(verbosity=2)
