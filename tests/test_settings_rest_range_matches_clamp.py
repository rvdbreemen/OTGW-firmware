#!/usr/bin/env python3
"""TASK-1941: the range GET /api/v2/settings advertises for a numeric setting matches the setter's clamp.

    python tests/test_settings_rest_range_matches_clamp.py
    OTGW_RANGE_REV=<sha> python tests/test_settings_rest_range_matches_clamp.py   # another revision

The settings setter (settingStuff.ino) stores a numeric value through clampSettingFloat()/clampSettingInt()
with a min and max; constrain() moves an out-of-range value to the nearest bound without telling the
user. restAPI.ino advertises min/max for the settings UI through addNum(F("name"), buf, type, min, max).
When the advertised min is below the clamp (satflowoffset: UI 0, clamp 0.5), the UI offers a value that
is silently stored as something else. This test pairs every clamped setter with its addNum() entry by
name (case-insensitive) and requires the same bounds.
"""
import os
import re
import subprocess
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
REV = os.environ.get("OTGW_RANGE_REV")


def read(rel):
    if REV is None:
        return (REPO / rel).read_text(encoding="utf-8")
    return subprocess.run(["git", "-C", str(REPO), "show", f"{REV}:{rel}"], capture_output=True, text=True,
                          encoding="utf-8", check=True).stdout


NUM = r"(-?[\d.]+)f?"
CLAMP = re.compile(r'PSTR\("(\w+)"\)\)\s*==\s*0\)\s*[\w.]+\s*=\s*clampSetting\w*\(newValue,\s*[^,]+,\s*'
                   + NUM + r",\s*" + NUM + r"\)")
REST = re.compile(r'addNum\(F\("(\w+)"\),\s*\w+,\s*"\w+",\s*' + NUM + r",\s*" + NUM + r"\)")


class SettingsRestRangeMatchesClamp(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        st, ra = read("src/OTGW-firmware/settingStuff.ino"), read("src/OTGW-firmware/restAPI.ino")
        cls.clamps = {m[0].lower(): (float(m[1]), float(m[2])) for m in CLAMP.findall(st)}
        cls.rest = {m[0].lower(): (float(m[1]), float(m[2])) for m in REST.findall(ra)}

    def test_the_parsers_find_the_settings(self):
        self.assertGreater(len(self.clamps), 30)
        self.assertGreater(len(set(self.clamps) & set(self.rest)), 30)

    def test_satflowoffset_min_is_the_clamp_min(self):
        self.assertEqual(self.rest["satflowoffset"], (0.5, 10.0))

    def test_every_advertised_range_matches_its_clamp(self):
        bad = {k: (self.rest[k], v) for k, v in self.clamps.items() if k in self.rest and self.rest[k] != v}
        self.assertEqual(bad, {}, "name: (advertised, clamp)")


if __name__ == "__main__":
    unittest.main(verbosity=2)
