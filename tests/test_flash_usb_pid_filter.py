#!/usr/bin/env python3
"""TASK-1394: USB auto-detect also finds a fresh ESP32-S3 that enumerates as 303A:4001.

A board after our first flash shows the built-in USB-Serial JTAG (303A:1001); a fresh board with
its factory firmware shows the USB CDC (303A:4001). Without --port, flash_otgw.bat/.sh hand esptool
a --port-filter; esptool keeps a port when its pid is in the list of pid filters
(esptool/cli_util.py: `port.pid not in pids`), so repeated pid filters are ORed.

Old-vs-fix: OTGW_FLASH_DIR points at a directory with older copies of the three files.

Run: python tests/test_flash_usb_pid_filter.py
"""
import os
import re
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
DIR = Path(os.environ.get("OTGW_FLASH_DIR", REPO))


def read(name):
    return (DIR / name).read_text(encoding="utf-8")


class UsbPidFilter(unittest.TestCase):
    def assert_filter(self, text, where):
        lines = [l for l in text.splitlines() if "--port-filter vid=0x303A" in l]
        self.assertTrue(lines, where + ": no --port-filter line")
        for l in lines:
            self.assertRegex(l, r"--port-filter pid=0x1001", where)
            self.assertRegex(l, r"--port-filter pid=0x4001", where)

    def test_bat_auto_detect_filters_both_pids(self):
        self.assert_filter(read("flash_otgw.bat"), "flash_otgw.bat")

    def test_sh_auto_detect_filters_both_pids(self):
        self.assert_filter(read("flash_otgw.sh"), "flash_otgw.sh")

    def test_flash_esp_knows_both_pids(self):
        self.assertRegex(read("flash_esp.py"), r"\(0x303A,\s*0x4001\)")

    def test_usage_text_names_both_ids(self):
        for name in ("flash_otgw.bat", "flash_otgw.sh"):
            with self.subTest(name=name):
                self.assertIn("303A:4001", read(name))


if __name__ == "__main__":
    unittest.main(verbosity=2)
