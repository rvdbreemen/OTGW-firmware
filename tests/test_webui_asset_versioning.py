"""Regression checks for Web UI asset cache-busting."""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]
DATA_DIR = ROOT / "src" / "OTGW-firmware" / "data"
FSEXPLORER = ROOT / "src" / "OTGW-firmware" / "FSexplorer.ino"


class TestWebUiAssetVersioning(unittest.TestCase):
    def test_stylesheets_are_versioned_by_send_index(self):
        html = (DATA_DIR / "index.html").read_text(encoding="utf-8")
        fsexplorer = FSEXPLORER.read_text(encoding="utf-8")

        stylesheet_assets = re.findall(r'href="([^"?]+\.css)"', html)
        self.assertGreater(stylesheet_assets, [], "index.html should link local CSS assets")

        for asset in stylesheet_assets:
            with self.subTest(asset=asset):
                self.assertIn(f'PSTR("href=\\"{asset}\\"")', fsexplorer)
                self.assertIn(f'F("href=\\"{asset}?v=")', fsexplorer)
                self.assertIn(f'httpServer.on("/{asset}"', fsexplorer)

        self.assertIn("sendVersionedAssetCacheHeader();", fsexplorer)


if __name__ == "__main__":
    unittest.main(verbosity=2)
