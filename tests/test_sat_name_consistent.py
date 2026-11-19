#!/usr/bin/env python3
"""TASK-2928 guard: the web UI expands SAT the same way everywhere.

The SAT page header (data/v2.html) and the manuals call SAT the "Smart Autotune Thermostat". The Settings rail's SAT
category described it as "Standalone Adaptive Thermostat." (built-in English in data/v2.js, both in I18N_EN and in the
SET_CATS table, and in the nl/de packs; el translated that wrong name). The name is a proper name, so every language
shows it as is, like the SAT page header does.

Checked:
  - the SAT page header in v2.html says "Smart Autotune Thermostat";
  - settings.cat.sat.desc in I18N_EN, the SET_CATS fallback and the nl/de/el packs all name "Smart Autotune Thermostat";
  - no "Standalone Adaptive" (or the Greek translation of it) is left in data/ or i18n/.

Run: python tests/test_sat_name_consistent.py
"""
import json
import re
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
DATA = REPO / "src" / "OTGW-firmware" / "data"
V2JS = DATA / "v2.js"
V2HTML = DATA / "v2.html"
PACKS = {lang: REPO / "i18n" / ("%s.json" % lang) for lang in ("nl", "de", "el")}
NAME = "Smart Autotune Thermostat"
WRONG = re.compile(r"(?i)standalone adaptive|αυτόνομος προσαρμοστικός")


def cat_sat_desc_builtin(src):
    m = re.search(r'"sat":\s*\{\s*"desc":\s*"((?:[^"\\]|\\.)*)"', src)
    if not m:
        raise AssertionError("settings.cat.sat.desc not found in I18N_EN")
    return m.group(1)


def cat_sat_desc_setcats(src):
    m = re.search(r"\{ id: 'sat',[^}]*desc: '([^']*)'", src)
    if not m:
        raise AssertionError("SET_CATS sat entry not found")
    return m.group(1)


class SatNameConsistent(unittest.TestCase):
    def test_page_header_uses_the_name(self):
        self.assertIn("SAT · " + NAME, V2HTML.read_text(encoding="utf-8"))

    def test_settings_category_uses_the_same_name(self):
        src = V2JS.read_text(encoding="utf-8")
        self.assertIn(NAME, cat_sat_desc_builtin(src), "I18N_EN settings.cat.sat.desc")
        self.assertIn(NAME, cat_sat_desc_setcats(src), "SET_CATS sat desc")
        for lang, p in PACKS.items():
            desc = json.loads(p.read_text(encoding="utf-8"))["settings"]["cat"]["sat"]["desc"]
            self.assertIn(NAME, desc, "%s settings.cat.sat.desc: %s" % (lang, desc))

    def test_no_other_expansion_left(self):
        files = [V2JS, V2HTML] + list(PACKS.values())
        for f in files:
            hit = WRONG.search(f.read_text(encoding="utf-8"))
            self.assertIsNone(hit, "%s still says %r" % (f.name, hit.group(0) if hit else ""))


if __name__ == "__main__":
    unittest.main()
