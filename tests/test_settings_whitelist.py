#!/usr/bin/env python3
"""TASK-1254 guard: every setting GET /api/v2/settings exposes must be POST-able.

sendDeviceSettings() in restAPI.ino emits each setting with addStr/addInt/addNum/
addBool(F("<key>"), ...). postSettings() refuses any name that is not in the
knownSettings[] whitelist with 400 "Unknown setting name". A key that GET shows
but the whitelist lacks renders as an editable field in the Web UI and then fails
to save (alpha.436: "Legacy OT topic layout" -> "Save failed").

This test reads the real restAPI.ino source, collects both key sets and fails
when a GET key is missing from knownSettings[], unless the key is in
READ_ONLY_GET_KEYS below. Point OTGW_RESTAPI_INO at another copy of
restAPI.ino (for example `git show HEAD~1:src/OTGW-firmware/restAPI.ino`) to
check an older revision.

Preprocessor guards are ignored on both sides: a key counts as emitted or
whitelisted when it appears in the source, whatever #if surrounds it.

Run: python tests/test_settings_whitelist.py
"""
import os
import re
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
RESTAPI = Path(os.environ.get("OTGW_RESTAPI_INO",
                              REPO / "src" / "OTGW-firmware" / "restAPI.ino"))

# GET keys that are deliberately NOT writable through POST /api/v2/settings.
# Each entry must be emitted with type "r" (read-only), which v2.js renders as a
# disabled input; test_read_only_keys_are_type_r enforces that.
READ_ONLY_GET_KEYS = {
    # Connected Wi-Fi SSID, read from WiFi.SSID(). Wi-Fi credentials are set
    # through the AP-mode setup portal, not through the settings API.
    "ssid",
}

_EMIT_RE = re.compile(r'\badd(?:Str|Int|Num|Bool)\s*\(\s*F\("([^"]+)"\)\s*,([^;]*);')


def _source():
    return RESTAPI.read_text(encoding="utf-8", errors="replace")


def _get_body(src):
    start = src.index("void sendDeviceSettings()")
    end = src.index("} // sendDeviceSettings()", start)
    return src[start:end]


def get_keys(src):
    """{key: emit-call argument text} for every setting the GET handler emits."""
    return {m.group(1).lower(): m.group(2) for m in _EMIT_RE.finditer(_get_body(src))}


def known_settings(src):
    start = src.index("knownSettings[] = {")
    end = src.index("};", start)
    block = re.sub(r"//[^\n]*", "", src[start:end])   # drop comments
    return {k.lower() for k in re.findall(r'"([^"]+)"', block)}


class SettingsWhitelistTest(unittest.TestCase):
    def setUp(self):
        self.src = _source()
        self.get = get_keys(self.src)
        self.known = known_settings(self.src)

    def test_parser_sees_the_whole_handler(self):
        # Guards the regexes: a parser that silently matched nothing would make
        # the real check pass vacuously. Spot-check spaced forms like "addInt (".
        self.assertGreater(len(self.get), 150, "too few GET keys parsed")
        self.assertGreater(len(self.known), 150, "too few whitelist keys parsed")
        for k in ("hostname", "satwindowminsec", "satsensorarea3", "webhookurlon"):
            self.assertIn(k, self.get)

    def test_every_get_key_is_postable(self):
        missing = sorted(k for k in self.get
                         if k not in self.known and k not in READ_ONLY_GET_KEYS)
        self.assertEqual(missing, [],
                         "GET /api/v2/settings exposes keys that postSettings() rejects "
                         "(add them to knownSettings[] in restAPI.ino, or to "
                         "READ_ONLY_GET_KEYS if they are type \"r\"): " + ", ".join(missing))

    def test_read_only_keys_are_type_r(self):
        for k in sorted(READ_ONLY_GET_KEYS):
            self.assertIn(k, self.get, k + " is allowlisted but GET no longer emits it")
            self.assertRegex(self.get[k], r'"r"\s*,',
                             k + ' is allowlisted as read-only but not emitted with type "r"')
            self.assertNotIn(k, self.known, k + " is allowlisted as read-only but is POST-able")


if __name__ == "__main__":
    unittest.main(verbosity=2)
