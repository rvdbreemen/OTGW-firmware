#!/usr/bin/env python3
"""
TASK-1505: the flash zip must carry Unix modes that Info-ZIP unzip honours.

build.py runs on Windows, where zipfile stamps create_system=0 (MS-DOS). Info-ZIP
unzip on Linux then ignores the Unix mode in external_attr, so the launchers
unpack as -rw-r--r-- and './capture-mqtt-debug.sh' fails with Permission denied.
Every entry must therefore say create_system=3 and carry an explicit mode, and
carry a real date instead of 1980-01-01.

The test calls the real create_distribution_zip() on a small temp project.

Run: python tests/test_flash_zip_modes.py
"""

import io
import os
import sys
import tempfile
import time
import unittest
import zipfile
from pathlib import Path

if hasattr(sys.stdout, "buffer"):
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")

REPO_ROOT = Path(__file__).resolve().parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

import build  # noqa: E402 - after the sys.path change

SEMVER = "9.9.9-test.1+abc1234"
TARGET = "esp32-combo"


def make_project(root):
    """A minimal project tree with every file create_distribution_zip() packs."""
    stem = f"OTGW-firmware-{build.asset_slug(TARGET)}-{SEMVER}"
    bd = root / "build"
    bd.mkdir()
    for name in (f"{stem}-merged-full.bin", f"{stem}.ino.bin", f"{stem}.littlefs.bin"):
        (bd / name).write_bytes(b"\x00\x01binary")
    (root / "flash_otgw.bat").write_text("@echo off\n", encoding="utf-8")
    (root / "flash_otgw.sh").write_text("#!/bin/sh\necho flash\n", encoding="utf-8")
    sd = root / "scripts"
    (sd / "vendor").mkdir(parents=True)
    (sd / "capture-mqtt-debug.bat").write_text("@echo off\n", encoding="utf-8")
    (sd / "capture-mqtt-debug.sh").write_text("#!/bin/sh\necho cap\n", encoding="utf-8")
    (sd / "capture-mqtt-debug-macos.sh").write_text("#!/bin/sh\necho cap\n", encoding="utf-8")
    (sd / "otgw_capture.py").write_text("print('capture')\n", encoding="utf-8")
    (sd / "vendor" / "paho_mqtt-9.9.9-py3-none-any.whl").write_bytes(b"PK-not-really")


class FlashZipModes(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        root = Path(self.tmp.name)
        make_project(root)
        zip_path = build.create_distribution_zip(root, SEMVER, TARGET)
        self.assertIsNotNone(zip_path, "create_distribution_zip returned None")
        with zipfile.ZipFile(zip_path) as zf:
            self.infos = {i.filename: i for i in zf.infolist()}

    def test_all_entries_are_unix(self):
        self.assertTrue(self.infos)
        for name, info in self.infos.items():
            self.assertEqual(info.create_system, 3, name)

    def test_shell_scripts_are_executable(self):
        for name in ("flash_otgw.sh", "capture-mqtt-debug.sh", "capture-mqtt-debug-macos.sh"):
            self.assertIn(name, self.infos)
            self.assertEqual((self.infos[name].external_attr >> 16) & 0o777, 0o755, name)

    def test_regular_files_are_not_executable(self):
        for name, info in self.infos.items():
            if name.endswith(".sh"):
                continue
            self.assertEqual((info.external_attr >> 16) & 0o777, 0o644, name)

    def test_entries_are_regular_files(self):
        for name, info in self.infos.items():
            self.assertEqual((info.external_attr >> 16) & 0o170000, 0o100000, name)

    def test_entries_carry_a_real_date(self):
        year_now = time.localtime().tm_year
        for name, info in self.infos.items():
            self.assertGreaterEqual(info.date_time[0], year_now - 1, f"{name}: {info.date_time}")


if __name__ == "__main__":
    unittest.main()
