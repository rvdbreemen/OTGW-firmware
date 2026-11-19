#!/usr/bin/env python3
"""TASK-2023 guard: the device settings of the web UI's GitHub version check, and no outbound request from the gateway.

The check runs in the browser (tests/webui/update-check-github.test.mjs runs the real v2.js code). The firmware only stores
three booleans:
  updatecheck       the browser checks the GitHub releases API
  updatecheckasked  the web UI's one-time question was answered
  updatecheckbeta   a stable firmware is also told about betas

This test reads the real sources and checks each key in all four places it needs:
  - the UISection struct (UItypes.h);
  - the settings file write and read (settingStuff.ino writeSettings / updateSetting);
  - GET /api/v2/settings (restAPI.ino addBool);
  - the POST whitelist (restAPI.ino knownSettings).
Also checked:
  - the defaults follow the single constant UI_UPDATE_CHECK_DEFAULT_MODE (0 off and silent, 1 off and ask once, 2 on);
  - the two visible keys have a help text and updatecheckasked is a hidden row in v2.js;
  - no firmware source names api.github.com, so the gateway itself makes no request to GitHub (ADR-003 HTTP only).

Run: python tests/test_update_check_settings.py
"""
import re
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
FW = REPO / "src" / "OTGW-firmware"
KEYS = {"updatecheck": "bUpdateCheck", "updatecheckasked": "bUpdateCheckAsked", "updatecheckbeta": "bUpdateCheckBeta"}


def read(name):
    return (FW / name).read_text(encoding="utf-8", errors="replace")


class UpdateCheckSettings(unittest.TestCase):
    def test_struct_fields_and_default_mode(self):
        ui = read("UItypes.h")
        m = re.search(r"#define UI_UPDATE_CHECK_DEFAULT_MODE (\d)", ui)
        self.assertIsNotNone(m, "UI_UPDATE_CHECK_DEFAULT_MODE missing")
        self.assertIn(m.group(1), "012")
        self.assertIn("bool bUpdateCheck      = (UI_UPDATE_CHECK_DEFAULT_MODE == 2);", ui)
        self.assertIn("bool bUpdateCheckAsked = (UI_UPDATE_CHECK_DEFAULT_MODE != 1);", ui)
        self.assertIn("bool bUpdateCheckBeta  = false;", ui)

    def test_default_semantics(self):
        # The three modes of the open question in TASK-2023 (A = 2, B = 1, C = 0).
        for mode, want in ((0, (False, True)), (1, (False, False)), (2, (True, True))):
            self.assertEqual(((mode == 2), (mode != 1)), want, "mode %d" % mode)

    def test_settings_file_and_dispatch(self):
        st = read("settingStuff.ino")
        for key, field in KEYS.items():
            self.assertIn('writeJsonBoolKV(file, F("%s"), settings.ui.%s);' % (key, field), st, key)
            self.assertRegex(st, r'strcasecmp_P\(field, PSTR\("%s"\)\)==0\)\s+settings\.ui\.%s = EVALBOOLEAN\(newValue\);'
                             % (key, field), key)

    def test_rest_get_and_whitelist(self):
        rest = read("restAPI.ino")
        wl = rest[rest.index("knownSettings"):rest.index("static bool isKnownSetting")]
        for key, field in KEYS.items():
            self.assertIn('addBool(F("%s"), settings.ui.%s, "b");' % (key, field), rest, key)
            self.assertIn('"%s"' % key, wl, key + " not POST-able")

    def test_help_and_hidden_row(self):
        helps = read("settingsHelp.h")
        for key in ("updatecheck", "updatecheckbeta"):
            m = re.search(r'^"%s":"([^"]*)",' % key, helps, re.M)
            self.assertIsNotNone(m, key + " has no help text")
            self.assertLessEqual(len(m.group(1)), 300, key + " help too long")
        self.assertIn("api.github.com", re.search(r'^"updatecheck":"([^"]*)"', helps, re.M).group(1),
                      "the help names who is contacted (privacy)")
        v2 = read("data/v2.js")
        self.assertRegex(v2, r"var SET_HIDE_ROW = \{[^}]*updatecheckasked: 1")
        # The one-time question names the same host as the help text (09's bench note on 383): api.github.com, in every
        # language, never the bare github.com.
        texts = {"en": re.search(r'"upd_ask_body": "([^"]*)"', v2).group(1)}
        for lang in ("nl", "de", "el"):
            pack = (REPO / "i18n" / ("%s.json" % lang)).read_text(encoding="utf-8")
            texts[lang] = re.search(r'"upd_ask_body": "([^"]*)"', pack).group(1)
        for lang, text in texts.items():
            self.assertIn("api.github.com", text, lang)
            self.assertNotRegex(text, r"(?<![\w.])github\.com", lang + ": bare github.com")

    def test_gateway_never_calls_github(self):
        hits = []
        for p in sorted(FW.rglob("*")):
            if p.name == "settingsHelp.h":   # help text that names the host for the user, not code (compiled out, TASK-1955)
                continue
            if p.suffix in (".ino", ".h", ".cpp", ".c") and "api.github.com" in p.read_text(encoding="utf-8", errors="replace"):
                hits.append(str(p.relative_to(REPO)))
        self.assertEqual(hits, [], "firmware code must not contact GitHub (the browser does, ADR-003)")


if __name__ == "__main__":
    unittest.main()
