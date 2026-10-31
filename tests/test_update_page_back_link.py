#!/usr/bin/env python3
"""TASK-1534: the /update flash utility page has a way back to the web UI.

A tester on a phone found no way to cancel and return to the main menu from /update. The page
(UpdateServerIndex in src/OTGW-firmware/updateServerHtml.h) now carries a "Back to the web UI"
link at the top of the form page. It is hidden while an update runs (leaving the tab then would
break the update) and shown again by the Back button after a failed run.

Old-vs-fix: OTGW_UPDATE_H points at another copy, e.g. `git show HEAD~:src/OTGW-firmware/updateServerHtml.h`.

Run: python tests/test_update_page_back_link.py
"""
import os
import re
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
HDR = Path(os.environ.get("OTGW_UPDATE_H", REPO / "src" / "OTGW-firmware" / "updateServerHtml.h"))


def page():
    src = HDR.read_text(encoding="utf-8")
    m = re.search(r'UpdateServerIndex\[\] PROGMEM =\s*R"UPD\((.*?)\)UPD"', src, re.S)
    assert m, "UpdateServerIndex raw string not found"
    return m.group(1)


class UpdatePageBackLink(unittest.TestCase):
    def test_form_page_links_back_to_the_web_ui(self):
        html = page()
        form = html[html.index("<div id='pageForm'>"):html.index("<div id='oneBox'>")]
        self.assertRegex(form, r"<p id='backUi'><a href='\./'>[^<]*Back to the web UI</a></p>")

    def test_link_hidden_while_the_update_runs_and_back_after(self):
        html = page()
        run = [l for l in html.splitlines() if "show('oneRun', true)" in l]
        self.assertEqual(len(run), 1, run)
        self.assertIn("show('backUi', false)", run[0])
        back = [l for l in html.splitlines() if "$('oneBack').onclick" in l]
        self.assertEqual(len(back), 1, back)
        self.assertIn("show('backUi', true)", back[0])


if __name__ == "__main__":
    unittest.main(verbosity=2)
