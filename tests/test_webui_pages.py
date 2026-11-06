#!/usr/bin/env python3
"""
TASK-1640: tests for scripts/webui_pages.py, which turns each web page into one
gzipped file (stylesheets, scripts and fonts inlined) and the firmware update
pages into gzipped PROGMEM arrays.

Run: python tests/test_webui_pages.py
"""

import base64
import gzip
import re
import sys
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
DATA_DIR = REPO_ROOT / "src" / "OTGW-firmware" / "data"
sys.path.insert(0, str(REPO_ROOT / "scripts"))

import webui_pages  # noqa: E402 - after the sys.path change

PAGE = """<!doctype html>
<html><head>
<link rel="stylesheet" href="a.css">
<link rel="stylesheet" href="https://cdn.example/x.css">
</head><body>
<script src="app.js"></script>
</body></html>
"""


def make_site(root, js="var x = 1;\n", css='@font-face{src:url("fonts/f.woff2") format("woff2")}\n'):
    root = Path(root)
    (root / "fonts").mkdir(exist_ok=True)
    (root / "fonts" / "f.woff2").write_bytes(b"\x00woff2bytes\xff")
    (root / "a.css").write_text(css, encoding="utf-8")
    (root / "app.js").write_text(js, encoding="utf-8")
    (root / "v2.html").write_text(PAGE, encoding="utf-8")
    (root / "favicon.ico").write_bytes(b"ico")
    return root


class TestInlinePage(unittest.TestCase):
    def test_inlines_stylesheet_script_and_font(self):
        with tempfile.TemporaryDirectory() as tmp:
            html, used = webui_pages.inline_page(make_site(tmp), "v2.html")
        b64 = base64.b64encode(b"\x00woff2bytes\xff").decode()
        self.assertIn("<style>@font-face{src:url(data:font/woff2;base64," + b64 + ")", html)
        self.assertIn("<script>var x = 1;\n</script>", html)
        self.assertNotIn('href="a.css"', html)
        self.assertNotIn('src="app.js"', html)
        self.assertEqual(used, {"v2.html", "a.css", "app.js", "fonts/f.woff2"})

    def test_remote_stylesheet_left_alone(self):
        with tempfile.TemporaryDirectory() as tmp:
            html, _ = webui_pages.inline_page(make_site(tmp), "v2.html")
        self.assertIn('<link rel="stylesheet" href="https://cdn.example/x.css">', html)

    def test_missing_referenced_file_raises(self):
        with tempfile.TemporaryDirectory() as tmp:
            site = make_site(tmp)
            (site / "app.js").unlink()
            with self.assertRaises(FileNotFoundError):
                webui_pages.inline_page(site, "v2.html")

    def test_missing_font_raises(self):
        with tempfile.TemporaryDirectory() as tmp:
            site = make_site(tmp)
            (site / "fonts" / "f.woff2").unlink()
            with self.assertRaises(FileNotFoundError):
                webui_pages.inline_page(site, "v2.html")

    def test_script_end_tag_inside_js_is_escaped(self):
        with tempfile.TemporaryDirectory() as tmp:
            site = make_site(tmp, js='var s = "</script><b>";\n')
            html, _ = webui_pages.inline_page(site, "v2.html")
        # Exactly one closing tag: the real one. The JS string keeps its value ("<\/" is "</").
        self.assertEqual(html.lower().count("</script>"), 1)
        self.assertIn('var s = "<\\/script><b>";', html)

    def test_html_comment_opener_in_js_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            site = make_site(tmp, js="var s = '<!--';\n")
            with self.assertRaises(ValueError):
                webui_pages.inline_page(site, "v2.html")


class TestWritePages(unittest.TestCase):
    def test_replaces_inputs_keeps_favicon_removes_empty_fonts_dir(self):
        with tempfile.TemporaryDirectory() as tmp:
            site = make_site(tmp)
            (site / "other.txt").write_text("keep", encoding="utf-8")
            sizes = webui_pages.write_pages(site)
            self.assertEqual(sorted(p.name for p in site.iterdir()),
                             ["favicon.ico", "other.txt", "v2.html.gz"])
            self.assertEqual(sizes, {"v2.html.gz": (site / "v2.html.gz").stat().st_size})
            html = gzip.decompress((site / "v2.html.gz").read_bytes()).decode("utf-8")
            self.assertIn("<style>", html)
            self.assertIn("<script>var x = 1;", html)

    def test_gzip_is_reproducible(self):
        self.assertEqual(webui_pages.gzip_bytes(b"abc" * 100), webui_pages.gzip_bytes(b"abc" * 100))


class TestRealPages(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.pages, cls.used = webui_pages.build_pages(DATA_DIR)
        cls.html = {n: gzip.decompress(b).decode("utf-8") for n, b in cls.pages.items()}

    def test_both_pages_built(self):
        self.assertEqual(set(self.pages), {"v2.html.gz", "FSexplorer.html.gz"})

    def test_v2_has_no_local_stylesheet_or_script_reference(self):
        html = self.html["v2.html.gz"]
        self.assertNotRegex(html, r'<link[^>]*rel="stylesheet"[^>]*href="(?!https?:|//)')
        self.assertNotRegex(html, r'<script[^>]*\ssrc="(?!https?:|//)')
        self.assertEqual(html.count("data:font/woff2;base64,"), 3)

    def test_fsexplorer_carries_its_stylesheet(self):
        css = (DATA_DIR / "fsexplorer.css").read_text(encoding="utf-8")
        first_rule = re.search(r"[^{}/]+\{[^{}]+\}", re.sub(r"/\*.*?\*/", "", css, flags=re.S)).group(0)
        self.assertIn(first_rule.strip(), self.html["FSexplorer.html.gz"])

    def test_inputs_are_reported_as_used(self):
        self.assertTrue({"v2.js", "v2.css", "ds-tokens.css", "fsexplorer.css"} <= self.used)
        self.assertNotIn("favicon.ico", self.used)


class TestProgmemHeader(unittest.TestCase):
    def test_round_trip(self):
        with tempfile.TemporaryDirectory() as tmp:
            page = Path(tmp) / "p.html"
            page.write_bytes("<html>hello \u00e9</html>\r\n".encode("utf-8"))  # a CRLF checkout (Windows autocrlf)
            text = webui_pages.progmem_header({"MyPage": page}, "MYPAGE_H")
        m = re.search(r"MyPage\[\] PROGMEM = \{(.*?)\};", text, re.S)
        data = bytes(int(x, 16) for x in re.findall(r"0x([0-9a-f]{2})", m.group(1)))
        self.assertEqual(gzip.decompress(data), "<html>hello \u00e9</html>\n".encode("utf-8"))
        self.assertIn("static const size_t MyPageLen = sizeof(MyPage);", text)
        self.assertIn("#ifndef MYPAGE_H", text)

    def test_write_update_header_second_call_unchanged(self):
        with tempfile.TemporaryDirectory() as tmp:
            fw = Path(tmp) / "src" / "OTGW-firmware"
            (fw / "webui").mkdir(parents=True)
            for rel in webui_pages.UPDATE_PAGES.values():
                (fw / rel).write_text("<html>" + rel + "</html>", encoding="utf-8")
            self.assertTrue(webui_pages.write_update_header(tmp))
            self.assertFalse(webui_pages.write_update_header(tmp))
            (fw / "webui" / "update.html").write_text("<html>changed</html>", encoding="utf-8")
            self.assertTrue(webui_pages.write_update_header(tmp))


if __name__ == "__main__":
    unittest.main(verbosity=2)
