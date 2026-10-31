#!/usr/bin/env python3
"""TASK-1445: FSexplorer reboot redirects and the Helper page use page-relative URLs.

Behind a reverse proxy that serves the gateway under a path (Home Assistant ingress
/api/ingress/<name>/, nginx /otgw/), "/" resolves to the proxy root. The reboot and
reset-wireless countdown redirect therefore must go to "./", and the Helper page (served when
FSexplorer.html is missing) must post to "upload" and open "update" without a leading slash,
as on the 1.x line (18068a353).

Old-vs-fix: OTGW_FSEXPLORER_INO points at another copy.

Run: python tests/test_fsexplorer_relative_urls.py
"""
import os
import re
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
INO = Path(os.environ.get("OTGW_FSEXPLORER_INO", REPO / "src" / "OTGW-firmware" / "FSexplorer.ino"))


class RelativeUrls(unittest.TestCase):
    def setUp(self):
        self.src = INO.read_text(encoding="utf-8")

    def test_reboot_redirects_are_page_relative(self):
        calls = re.findall(r'doRedirect\("Reboot[^"]*", \d+, "([^"]*)", true\)', self.src)
        self.assertEqual(len(calls), 2, calls)
        self.assertEqual(calls, ["./", "./"])

    def test_helper_forms_have_no_leading_slash(self):
        helper = self.src[self.src.index("const char Helper[] PROGMEM"):self.src.index("const char Header[] PROGMEM")]
        self.assertIn('action=\\"upload\\"', helper)
        self.assertIn("action='update'", helper)
        self.assertNotIn('action=\\"/', helper)
        self.assertNotIn("action='/", helper)


if __name__ == "__main__":
    unittest.main(verbosity=2)
