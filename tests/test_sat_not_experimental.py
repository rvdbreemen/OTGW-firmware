#!/usr/bin/env python3
"""TASK-2909 guard: from the 2.0.0 RC the web UI no longer calls SAT itself experimental.

Robert, 2026-11-18 (DMs 1572669805515903007 .. 1572670569961496768, relayed by the coordinator): "Als we 2.0.0 releasen
dan gaan we het woord experimenteel eraf halen. SAT krijgt stabiel niveau." SAT stays opt-in and off by default, and the
wizard still asks the user to confirm. The on/off heat-pump type with the 3 h cycle stays experimental (Robert,
"B voor warmtepomp", DM 1572672969744851067), so its labels must still say so.

Checked in data/v2.js (built-in English) and the nl/de/el packs:
  - the wizard confirmation (onb_beta_ack) and the SAT beta notice (sat_beta_head and its fallback SAT_BETA_HEAD) contain
    no form of experimental in any language, and the confirmation still names the boiler;
  - the heat-pump type label (enum_onoff_hp) and its settings help (satsource) still say experimental.

Run: python tests/test_sat_not_experimental.py
"""
import re
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
V2JS = REPO / "src" / "OTGW-firmware" / "data" / "v2.js"
PACKS = {lang: REPO / "i18n" / ("%s.json" % lang) for lang in ("nl", "de", "el")}

# Every form of the word in the four UI languages (Greek stem covers πειραματικό/-ή/-ός/...).
EXPERIMENTAL = re.compile(r"(?i)experiment|πειραματ")
BOILER = {"en": "boiler", "nl": "ketel", "de": "kessel", "el": "λέβητ"}
SAT_KEYS = ("onb_beta_ack", "sat_beta_head")
HP_KEYS = ("enum_onoff_hp", "satsource")


def value(src, key):
    hits = re.findall('"' + re.escape(key) + '":\\s*"((?:[^"\\\\]|\\\\.)*)"', src)
    if not hits:
        raise AssertionError("%s not found" % key)
    return hits


def texts():
    out = {"en": V2JS.read_text(encoding="utf-8")}
    for lang, p in PACKS.items():
        out[lang] = p.read_text(encoding="utf-8")
    return out


class SatNotExperimental(unittest.TestCase):
    def test_sat_strings_do_not_say_experimental(self):
        for lang, src in texts().items():
            for key in SAT_KEYS:
                for v in value(src, key):
                    self.assertIsNone(EXPERIMENTAL.search(v), "%s %s still says experimental: %s" % (lang, key, v))

    def test_fallback_constant(self):
        m = re.search(r"var SAT_BETA_HEAD = '([^']*)';", V2JS.read_text(encoding="utf-8"))
        self.assertIsNotNone(m, "SAT_BETA_HEAD not found")
        self.assertIsNone(EXPERIMENTAL.search(m.group(1)), "SAT_BETA_HEAD still says experimental")
        self.assertEqual(m.group(1), value(texts()["en"], "sat_beta_head")[0], "fallback differs from the English string")

    def test_beta_notice_body_does_not_say_unvalidated(self):
        # Agreed with otgw-docs (TASK-2034): "We need field data to validate it" said SAT is not proven, against the
        # manual's "stable from 2.0.0". The request for field data stays.
        unproven = re.compile(r"(?i)validat|valider|validierung|επικυρ")
        bodies = dict((lang, value(src, "sat_beta_body")[0]) for lang, src in texts().items())
        m = re.search(r"var SAT_BETA_BODY = '([^']*)';", V2JS.read_text(encoding="utf-8"))
        bodies["en fallback"] = m.group(1)
        for lang, v in bodies.items():
            self.assertIsNone(unproven.search(v), "%s sat_beta_body still says it must be validated: %s" % (lang, v))
            self.assertIn("capture", v.lower(), "%s sat_beta_body lost the capture request" % lang)
            # Coordinator, 2026-11-19: the notice shows on every prerelease build, public betas included, so it sends
            # testers to #beta-testing, as the stable docs do (Robert's decision A of 2026-11-18).
            self.assertIn("#beta-testing", v, "%s sat_beta_body does not name #beta-testing" % lang)
            self.assertNotIn("#alpha-testing", v, "%s sat_beta_body still names #alpha-testing" % lang)

    def test_confirmation_still_names_the_boiler(self):
        for lang, src in texts().items():
            v = value(src, "onb_beta_ack")[0]
            self.assertIn(BOILER[lang], v.lower(), "%s onb_beta_ack no longer names the boiler: %s" % (lang, v))

    def test_heat_pump_type_stays_experimental(self):
        for lang, src in texts().items():
            for key in HP_KEYS:
                vals = value(src, key)
                self.assertTrue(any(EXPERIMENTAL.search(v) for v in vals),
                                "%s %s lost 'experimental' (Robert: B voor warmtepomp)" % (lang, key))


if __name__ == "__main__":
    unittest.main()
