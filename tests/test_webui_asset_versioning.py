"""Web UI asset caching contract (ADR-139, amended by TASK-958).

index.html loads its stylesheets and scripts through a sequential loader
(TASK-960): loadCss('x.css', ...) calls and a SCRIPTS array. Every asset that
loader requests must be served by serveVersionedAsset() in FSexplorer.ino (ETag
= filesystem hash, Cache-Control: no-cache, so a reflash shows on the next
load) under a stable URL: no ?v= query versioning.
"""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]
DATA_DIR = ROOT / "src" / "OTGW-firmware" / "data"
FSEXPLORER = ROOT / "src" / "OTGW-firmware" / "FSexplorer.ino"

MIME = {".css": "text/css", ".js": "application/javascript"}


def loader_assets(html):
    """Assets the index.html loader requests, in load order, without a leading './'."""
    css = re.findall(r"loadCss\('([^']+)'", html)
    m = re.search(r"var SCRIPTS = \[(.*?)\];", html, re.S)
    scripts = re.findall(r"'([^']+)'", m.group(1)) if m else []
    return [a[2:] if a.startswith("./") else a for a in css + scripts]


def route_mime(fsexplorer, asset):
    """MIME type the /<asset> route passes to serveVersionedAsset(), or None."""
    m = re.search(r'server\.on\("/' + re.escape(asset) + r'",\s*HTTP_GET,.*?serveVersionedAsset\("/'
                  + re.escape(asset) + r'",\s*F\("([^"]+)"\)\)', fsexplorer)
    return m.group(1) if m else None


class TestWebUiAssetVersioning(unittest.TestCase):
    def setUp(self):
        self.assets = loader_assets((DATA_DIR / "index.html").read_text(encoding="utf-8"))
        self.fsexplorer = FSEXPLORER.read_text(encoding="utf-8")

    def test_loader_requests_the_stylesheets_and_scripts(self):
        for asset in ("ds-tokens.css", "components.css", "index.js"):
            self.assertIn(asset, self.assets)

    def test_every_loader_asset_is_served_as_a_versioned_asset(self):
        for asset in self.assets:
            with self.subTest(asset=asset):
                self.assertNotIn("?", asset, "stable URL: no ?v= query versioning (TASK-958)")
                self.assertEqual(route_mime(self.fsexplorer, asset), MIME[Path(asset).suffix],
                                 f"/{asset} needs a serveVersionedAsset() route with its MIME type")


if __name__ == "__main__":
    unittest.main(verbosity=2)
