#!/usr/bin/env python3
"""
TASK-1500 (ADR-202): host checks on the Linux/macOS capture launcher text.

scripts/capture-mqtt-debug.sh bootstraps a portable CPython from
python-build-standalone when no Python 3.9+ is on PATH. These tests read the
launcher and check what it pins and the order it tries things in:
  1. python3 / python on PATH, 2. the cached runtime, 3. the download.
They do not run the launcher; the real runs are recorded in TASK-1500.

Run: python tests/test_capture_launcher.py
"""

import re
import unittest
from pathlib import Path

LAUNCHER = Path(__file__).resolve().parent.parent / "scripts" / "capture-mqtt-debug.sh"

TAG = "20261026"
VERSION = "3.12.16"
TRIPLES = [
    "x86_64-unknown-linux-gnu",
    "aarch64-unknown-linux-gnu",
    "armv7-unknown-linux-gnueabihf",
    "x86_64-apple-darwin",
    "aarch64-apple-darwin",
]


class CaptureLauncherTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        raw = LAUNCHER.read_bytes()
        cls.raw = raw
        cls.text = raw.decode("utf-8")

    def test_line_endings_are_lf(self):
        self.assertNotIn(b"\r", self.raw)

    def test_pins_release_tag_and_version(self):
        self.assertIn(f'PBS_TAG="{TAG}"', self.text)
        self.assertIn(f'PY_VER="{VERSION}"', self.text)
        self.assertIn("astral-sh/python-build-standalone", self.text)
        self.assertIn("install_only.tar.gz", self.text)

    def test_five_triples_each_with_a_64_hex_checksum(self):
        for triple in TRIPLES:
            m = re.search(
                r"^\s*" + re.escape(triple) + r"\)\s*PY_SHA=\"([0-9a-f]{64})\"", self.text, re.M
            )
            self.assertIsNotNone(m, f"no 64-hex checksum line for {triple}")

    def test_every_checksum_is_unique(self):
        sums = re.findall(r'PY_SHA="([0-9a-f]{64})"', self.text)
        self.assertEqual(len(sums), len(TRIPLES))
        self.assertEqual(len(set(sums)), len(sums), "two triples share a checksum")

    def test_order_path_then_cache_then_download(self):
        # The main flow is the tail of the file, after the function definitions.
        start = self.text.rfind("for candidate in python3 python")
        self.assertGreaterEqual(start, 0, "PATH step missing")
        tail = self.text[start:]
        cache = tail.find('use_cached_python "$@"')
        download = tail.find('download_python "$@"')
        self.assertGreater(cache, 0, "cached-runtime step missing after the PATH step")
        self.assertGreater(download, cache, "download must come after the cached runtime")

    def test_final_directories(self):
        self.assertIn("/otgw-capture", self.text)
        self.assertIn("python-${PY_VER}", self.text)
        self.assertIn("Library/Application Support", self.text)
        self.assertIn("XDG_DATA_HOME", self.text)

    def test_checksum_is_verified_before_unpacking(self):
        t = self.text
        self.assertLess(t.find("sha256sum"), t.find("tar -xzf"))
        self.assertIn("shasum -a 256", t)


if __name__ == "__main__":
    unittest.main()
