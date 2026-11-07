#!/usr/bin/env python3
"""
TASK-1711: host checks on the Windows capture launcher text (scripts/capture-mqtt-debug.bat).

A tester's Windows Defender quarantined the launcher as "Trojan, Severe" (Discord #alpha-testing,
2026-11-07). The portable-Python download ran as an inline PowerShell command with
-ExecutionPolicy Bypass and Net.WebClient.DownloadFile: the download-and-run pattern that AMSI
(Windows Antimalware Scan Interface) inspects in every PowerShell command line. The launcher now
downloads with the curl.exe, checks with certutil -hashfile and unpacks with the tar.exe that ship
with Windows 10 1803 and later, so no PowerShell runs at all.

These tests read the launcher; they do not run it (the real run is recorded in TASK-1711):
  a  no PowerShell, no ExecutionPolicy, no Net.WebClient anywhere in the launcher
  b  the download uses curl.exe, the check certutil -hashfile SHA256, the unpack tar.exe
  c  the SHA-256 check comes before the unpack, and a mismatch deletes the zip
  d  three 64-hex checksums, one per architecture, all different
  e  order: Python on PATH, then the cached runtime, then the download
  f  CRLF line endings (cmd.exe misreads labels in an LF batch file)

Run: python tests/test_capture_launcher_bat.py
"""

import re
import unittest
from pathlib import Path

LAUNCHER = Path(__file__).resolve().parent.parent / "scripts" / "capture-mqtt-debug.bat"


class BatLauncherTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.raw = LAUNCHER.read_bytes()
        cls.text = cls.raw.decode("ascii")
        # Commands only: rem lines may explain the history.
        cls.code = "\n".join(l for l in cls.text.splitlines() if not l.lstrip().lower().startswith("rem"))

    def test_a_no_powershell_download(self):
        low = self.code.lower()
        for word in ("powershell", "executionpolicy", "webclient", "downloadfile", "invoke-webrequest"):
            self.assertNotIn(word, low, f"launcher still uses {word}")

    def test_b_curl_certutil_tar(self):
        self.assertRegex(self.code, r'curl\.exe"? .*-o "%PY_ZIP%" "%PY_URL%"')
        self.assertRegex(self.code, r'certutil(\.exe)?"? -hashfile "%PY_ZIP%" SHA256')
        self.assertRegex(self.code, r'tar\.exe"? -xf "%PY_ZIP%" -C "%PY_DIR%"')

    def test_c_checksum_before_unpack(self):
        check = self.code.find("-hashfile")
        unpack = self.code.find("-xf")
        self.assertGreater(check, 0)
        self.assertGreater(unpack, check, "the zip is unpacked before its SHA-256 is checked")
        mismatch = self.code[check:unpack]
        self.assertIn('del /f /q "%PY_ZIP%"', mismatch, "a zip with the wrong checksum is not deleted")

    def test_d_three_unique_checksums(self):
        sums = re.findall(r'PY_SHA=([0-9a-f]{64})"', self.code)
        self.assertEqual(len(sums), 3)
        self.assertEqual(len(set(sums)), 3)

    def test_e_order_path_cache_download(self):
        t = self.code
        path_step = t.find("call :try_python py -3")
        cache = t.find('if exist "%PY_DIR%\\python.exe"')
        download = t.find("-o \"%PY_ZIP%\"")
        self.assertGreater(path_step, 0)
        self.assertGreater(cache, path_step)
        self.assertGreater(download, cache)

    def test_f_crlf(self):
        self.assertNotIn(b"\n", self.raw.replace(b"\r\n", b""))


if __name__ == "__main__":
    unittest.main()
