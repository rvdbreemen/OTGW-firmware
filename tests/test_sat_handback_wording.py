"""TASK-2036: the SAT hand-back hints say that SAT is paused, in Robert's chosen wording (option A, DM 1572743956645552149).

gruijter (2026-11-18) read "boiler handed back" as cryptic. SAT is not disabled in that state: it stays enabled and resumes
by itself (Sergeant D entries 56, 57), so the text says "paused". EN and NL are Robert's text verbatim; DE and EL follow
i18n/glossary.md (DE neutral register, EL polite plural). Checked: the EN strings in data/v2.js, the fallback text in
data/v2.html, the three language packs, and that no hint still says "handed back".
"""
import json
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
DATA = ROOT / "src" / "OTGW-firmware" / "data"

WANT = {
    "en": {
        "sat_outside_missing": "SAT paused: no outside temperature for 1 hour, your thermostat controls the boiler",
        "sat_ot_lost": "SAT paused: no OpenTherm connection, the boiler is back under normal control",
    },
    "nl": {
        "sat_outside_missing": "SAT gepauzeerd: al 1 uur geen buitentemperatuur, je thermostaat regelt de ketel",
        "sat_ot_lost": "SAT gepauzeerd: geen OpenTherm-verbinding, de ketel regelt weer normaal",
    },
    "de": {
        "sat_outside_missing": "SAT pausiert: seit 1 Stunde keine Außentemperatur, das Thermostat regelt den Kessel",
        "sat_ot_lost": "SAT pausiert: keine OpenTherm-Verbindung, der Kessel regelt wieder normal",
    },
    "el": {
        "sat_outside_missing": "SAT σε παύση: καμία μέτρηση εξωτερικής θερμοκρασίας εδώ και 1 ώρα, ο θερμοστάτης σας ελέγχει τον λέβητα",
        "sat_ot_lost": "SAT σε παύση: δεν υπάρχει σύνδεση OpenTherm, ο λέβητας λειτουργεί ξανά κανονικά",
    },
}


def _js_value(src, key):
    hits = re.findall('"' + re.escape(key) + '":\\s*"([^"]*)"', src)
    assert len(hits) == 1, (key, len(hits))
    return hits[0]


def _pack_value(pack, key):
    found = []

    def walk(node):
        if isinstance(node, dict):
            for k, v in node.items():
                if k == key and isinstance(v, str):
                    found.append(v)
                else:
                    walk(v)

    walk(pack)
    assert len(found) == 1, (key, len(found))
    return found[0]


class HandBackWording(unittest.TestCase):
    def test_en_strings_in_v2_js(self):
        js = (DATA / "v2.js").read_text(encoding="utf-8")
        for key, text in WANT["en"].items():
            self.assertEqual(_js_value(js, key), text)

    def test_v2_html_fallback_text(self):
        html = (DATA / "v2.html").read_text(encoding="utf-8")
        for key, text in WANT["en"].items():
            self.assertIn('data-i18n="h_mon.%s">%s<' % (key, text), html)

    def test_language_packs(self):
        for lang in ("nl", "de", "el"):
            pack = json.loads((ROOT / "i18n" / ("%s.json" % lang)).read_text(encoding="utf-8"))
            for key, text in WANT[lang].items():
                self.assertEqual(_pack_value(pack, key), text, "%s %s" % (lang, key))

    def test_no_handed_back_hint_left(self):
        for path in (DATA / "v2.js", DATA / "v2.html"):
            self.assertNotIn("boiler handed back", path.read_text(encoding="utf-8"), path.name)


if __name__ == "__main__":
    unittest.main()
