#!/usr/bin/env python3
"""TASK-3089: the satzoneheadroom setting is removed end to end; the zone aggregation keeps the fixed 5.0.

Decision: Robert, interactive via the coordinator, 2026-11-25. With at most 4 zones (satzonecount 1 to 4) the 75th
percentile is the highest zone output, so min(highest, P75 + headroom) never binds for any headroom >= 0: the setting
was inert (its help text said "Unused with 4 zones or fewer", TASK-3018). SAT behaviour does not change: the helper
satAggregateZoneSetpoint() keeps thermo-nova's constant 5.0 (area.py:267, :298-299; Sergeant D entry 116 "port
area.py as it is"); tests/test_sat_zone_aggregation.py runs the real helper.

Settings migration: an old settings.ini with SATzoneheadroom keeps loading. updateSetting() has no branch for the key,
so the cascade leaves the struct unchanged and the no-op path returns (platformSettingsNoopUnchanged); readSettings()
clears the dirty flag after the parse; the next save no longer writes the key. POST /api/v2/settings answers 400
"Unknown setting name" for it (knownSettings[] lacks it). Checked here on the source, the same way as
tests/test_otd_setback_fields_removed.py: updateSetting() needs the whole firmware to compile, so this is a source
check, not a run of the parser.

    python tests/test_sat_zone_headroom_removed.py                     # the working tree
    OTGW_SRC_REV=<sha> python tests/test_sat_zone_headroom_removed.py  # another revision (old vs fix)
"""
import json
import os
import re
import subprocess
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
FW = "src/OTGW-firmware/"
REV = os.environ.get("OTGW_SRC_REV")


def read(rel):
    if REV is None:
        return (REPO / rel).read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")
    return subprocess.run(["git", "-C", str(REPO), "show", f"{REV}:{rel}"], capture_output=True, text=True,
                          encoding="utf-8", errors="replace", check=True).stdout.replace("\r\n", "\n")


KEY = re.compile(r"(?i)satzoneheadroom|fZoneAggregationHeadroom")
FILES = [FW + n for n in ("restAPI.ino", "settingStuff.ino", "SATtypes.h", "SATcontrol.ino", "settingsHelp.h",
                          "data/v2.js")] + ["i18n/nl.json", "i18n/de.json", "i18n/el.json",
                                            "scripts/json-golden/v2_settings.json"]


def first_hit(s):
    m = KEY.search(s)
    return None if not m else f"line {s.count(chr(10), 0, m.start()) + 1}: {s.splitlines()[s.count(chr(10), 0, m.start())].strip()[:120]}"


class Removed(unittest.TestCase):
    def test_no_reference_in_firmware_web_ui_i18n_or_golden(self):
        for f in FILES:
            with self.subTest(file=f):
                self.assertIsNone(first_hit(read(f)))

    def test_golden_settings_json_still_parses_without_the_key(self):
        self.assertNotIn("satzoneheadroom", json.loads(read("scripts/json-golden/v2_settings.json"))["settings"])

    def test_i18n_packs_and_web_ui_rows_are_gone(self):
        js = read(FW + "data/v2.js")
        self.assertRegex(js, r"\['Zones', \['satzonecount', 'satzonetimeout'\]\]")
        for lang in ("nl", "de", "el"):
            with self.subTest(lang=lang):
                json.loads(read(f"i18n/{lang}.json"))   # still valid JSON after the line was removed
                self.assertNotIn("satzoneheadroom", read(f"i18n/{lang}.json"))


class OldSettingsLoad(unittest.TestCase):
    def test_an_old_key_is_ignored_without_an_error(self):
        src = read(FW + "settingStuff.ino")
        body = src[src.index("void updateSetting("):src.index("} // updateSetting()")]
        self.assertNotRegex(body, r"(?i)SATzoneheadroom", "no branch: the key falls through the cascade")
        self.assertNotRegex(body, r"(?i)unknown (setting|key|field)", "an unknown key is not reported as an error")
        self.assertIn("platformSettingsNoopUnchanged(&settings, sizeof(settings))", body,
                      "an unchanged struct returns through the no-op path")

    def test_the_boot_parse_clears_the_dirty_state(self):
        src = read(FW + "settingStuff.ino")
        i = src.index("updateSetting(keyBuf, valueBuf);")
        tail = src[i:i + 700]
        self.assertIn("settingsDirty = false;", tail)
        self.assertIn("pendingSideEffects = 0;", tail)

    def test_the_next_save_no_longer_writes_the_key(self):
        self.assertNotRegex(read(FW + "settingStuff.ino"), r'F\("SATzoneheadroom"\)')

    def test_rest_post_refuses_the_name(self):
        src = read(FW + "restAPI.ino")
        self.assertIn("if (!isKnownSetting(field))", src)
        self.assertIn("Unknown setting name", src)
        self.assertNotRegex(src, r'"satzoneheadroom"')


class BehaviourUnchanged(unittest.TestCase):
    def test_the_helper_keeps_the_fixed_five_and_has_no_headroom_parameter(self):
        src = read(FW + "SATcontrol.ino")
        sig = src[src.index("static float satAggregateZoneSetpoint("):]
        sig = sig[:sig.index(")")]
        self.assertNotIn("headroom", sig.lower().replace("coolingheadroom", ""))
        self.assertRegex(src, r"const float zoneHeadroom\s*=\s*5\.0f;")
        self.assertRegex(src, r"float allowed = zoneOut\[idx\] \+ zoneHeadroom;")


if __name__ == "__main__":
    unittest.main(verbosity=2)
