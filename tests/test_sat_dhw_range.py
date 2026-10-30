#!/usr/bin/env python3
"""TASK-1279: the SAT DHW setpoint range is one range, 30..60, named once.

Parses the real sources (nothing is re-implemented) and checks that the settings
clamp, the SW= send condition, the simulator check, the settings-page range, the v2
slider (markup and JS fallback) all use or equal SAT_DHW_SETPOINT_MIN / _MAX, and that
the SAT status JSON carries dhw_enabled, which the v2 hot water card consumes.

Old-vs-fix: every source can be pointed at another copy through an env var, e.g.
`git show HEAD:src/OTGW-firmware/SATtypes.h > $TMP/SATtypes.h` and
OTGW_SATTYPES_H=$TMP/SATtypes.h. Variables: OTGW_SATTYPES_H, OTGW_SAT_INO,
OTGW_SETTING_INO, OTGW_REST_INO, OTGW_V2_HTML, OTGW_V2_JS.

Run: python tests/test_sat_dhw_range.py
"""
import os
import re
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
FW = REPO / "src" / "OTGW-firmware"


def _src(env, default):
    p = Path(os.environ.get(env, default))
    return p.read_text(encoding="utf-8", errors="replace")


TYPES = _src("OTGW_SATTYPES_H", FW / "SATtypes.h")
SAT = _src("OTGW_SAT_INO", FW / "SATcontrol.ino")
SETTING = _src("OTGW_SETTING_INO", FW / "settingStuff.ino")
REST = _src("OTGW_REST_INO", FW / "restAPI.ino")
HTML = _src("OTGW_V2_HTML", FW / "data" / "v2.html")
JS = _src("OTGW_V2_JS", FW / "data" / "v2.js")


def _define(name):
    m = re.search(r"^#define\s+" + name + r"\s+(\d+)\s*$", TYPES, re.M)
    return int(m.group(1)) if m else None


MIN = _define("SAT_DHW_SETPOINT_MIN")
MAX = _define("SAT_DHW_SETPOINT_MAX")


def _function_body(text, signature_re):
    m = re.search(signature_re, text)
    assert m, "function not found: " + signature_re
    i = text.index("{", m.end() - 1) if text[m.end() - 1] != "{" else m.end() - 1
    depth, j = 0, i
    while j < len(text):
        if text[j] == "{":
            depth += 1
        elif text[j] == "}":
            depth -= 1
            if depth == 0:
                return text[i:j + 1]
        j += 1
    raise AssertionError("unbalanced braces after " + signature_re)


class DhwRangeConstants(unittest.TestCase):
    def test_constants_are_30_and_60(self):
        self.assertEqual((MIN, MAX), (30, 60))


class FirmwareUsesConstants(unittest.TestCase):
    def test_settings_clamp_max_is_the_constant(self):
        m = re.search(r'PSTR\("SATdhwsetpoint"\)\)\s*==\s*0\)\s*settings\.sat\.fDhwSetpoint\s*=\s*'
                      r'clampSettingFloat\([^;]*?,\s*([^,;]+?),\s*([^,;]+?)\);', SETTING)
        self.assertIsNotNone(m, "SATdhwsetpoint clamp line not found")
        self.assertEqual(m.group(1).strip(), "0.0f", "clamp min stays 0 (= off)")
        self.assertRegex(m.group(2), r"SAT_DHW_SETPOINT_MAX", "clamp max must be the constant")

    def test_sw_send_condition_uses_constants(self):
        i = SAT.index('PSTR("SW=%d")')
        window = SAT[max(0, i - 900):i]
        self.assertIn("SAT_DHW_SETPOINT_MIN", window)
        self.assertIn("SAT_DHW_SETPOINT_MAX", window)
        cond = re.search(r"if \((!settings\.sat\.bDhwEnabled \|\| sp < .*?)\) \{", SAT[SAT.index("static void satDhwSetpointSync()"):], re.S)
        self.assertIsNotNone(cond, "SW= send condition not found")
        self.assertEqual(" ".join(cond.group(1).split()),
                         "!settings.sat.bDhwEnabled || sp < (float)SAT_DHW_SETPOINT_MIN || sp > (float)SAT_DHW_SETPOINT_MAX")

    def test_sim_check_uses_constants(self):
        m = re.search(r"if \(dhwDraw\) \{(.*?)\}", SAT, re.S)
        self.assertIsNotNone(m, "sim dhwDraw block not found")
        self.assertIn("SAT_DHW_SETPOINT_MIN", m.group(1))
        self.assertIn("SAT_DHW_SETPOINT_MAX", m.group(1))

    def test_no_literal_70_range_left_on_dhw_setpoint(self):
        for line in SAT.splitlines():
            if "fDhwSetpoint" in line:
                self.assertNotRegex(line, r"\b70(\.0f?)?\b", line.strip())
                self.assertNotRegex(line, r">=\s*30(\.0f?)?\b", line.strip())

    def test_settings_page_range_uses_constant(self):
        m = re.search(r'addNum\(F\("satdhwsetpoint"\),[^;]*?,\s*0,\s*([^)]+)\);', REST)
        self.assertIsNotNone(m, "satdhwsetpoint addNum not found")
        self.assertEqual(m.group(1).strip(), "SAT_DHW_SETPOINT_MAX")

    def test_status_json_emits_dhw_enabled(self):
        self.assertRegex(SAT, r'je\.field\(F\("dhw_enabled"\),\s*settings\.sat\.bDhwEnabled\)')


class WebUiMatchesConstants(unittest.TestCase):
    def test_slider_markup_range(self):
        m = re.search(r'<input type="range"([^>]*?)id="satDhwSlider"([^>]*)>', HTML)
        self.assertIsNotNone(m, "satDhwSlider not found")
        attrs = m.group(1) + m.group(2)
        self.assertEqual(int(re.search(r'min="(\d+)"', attrs).group(1)), MIN)
        self.assertEqual(int(re.search(r'max="(\d+)"', attrs).group(1)), MAX)
        self.assertRegex(attrs, r"\bdisabled\b", "slider starts disabled until the status says dhw_enabled")

    def test_enable_toggle_and_hint_exist(self):
        self.assertIn('id="satDhwEnabled"', HTML)
        self.assertIn('id="satDhwHint"', HTML)

    def test_js_fallback_range(self):
        mn = re.search(r"SAT_DHW_SETPOINT_MIN\s*=\s*(\d+)", JS)
        mx = re.search(r"SAT_DHW_SETPOINT_MAX\s*=\s*(\d+)", JS)
        self.assertIsNotNone(mn and mx, "v2.js range constants not found")
        self.assertEqual((int(mn.group(1)), int(mx.group(1))), (MIN, MAX))

    def test_fetch_dhw_bounds_intersects_and_has_no_old_literals(self):
        body = _function_body(JS, r"function fetchDhwBounds\(\)\s*\{")
        self.assertIn("SAT_DHW_SETPOINT_MIN", body)
        self.assertIn("SAT_DHW_SETPOINT_MAX", body)
        self.assertNotRegex(body, r"\b40\b")
        self.assertNotRegex(body, r"\b65\b")

    def test_render_reads_dhw_enabled_and_shows_off(self):
        body = _function_body(JS, r"function satRenderDhw\(d\)\s*\{")
        self.assertIn("d.dhw_enabled", body)
        self.assertIn("slider.disabled", body)
        self.assertIn("'off'", body)

    def test_toggle_posts_dhw_enabled_with_in_flight_guard(self):
        m = re.search(r"var dsOn = document\.getElementById\('satDhwEnabled'\);(.*?)var de = ", JS, re.S)
        self.assertIsNotNone(m, "satDhwEnabled wiring not found")
        wiring = m.group(1)
        self.assertIn("satPost('settings/dhw_enabled'", wiring)
        self.assertIn("satDhwEnabledPending = true", wiring)
        self.assertEqual(wiring.count("satPost("), 1, "one request at a time")

    def test_settings_page_hint(self):
        self.assertRegex(JS, r"satdhwsetpoint:\s*\{[^}]*hint: '°C \(below 30 = off, 30-60\)'")


if __name__ == "__main__":
    unittest.main(verbosity=2)
