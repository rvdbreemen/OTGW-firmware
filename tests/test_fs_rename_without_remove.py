#!/usr/bin/env python3
"""TASK-1916: temp-file writers replace the live file by rename, without removing it first.

    python tests/test_fs_rename_without_remove.py
    OTGW_FSREN_REV=<sha> python tests/test_fs_rename_without_remove.py     # another revision (old vs fix)

lfs_rename() replaces an existing destination in one metadata commit (TASK-1913, esp_littlefs
vfs_littlefs_rename -> lfs_rename), so 'remove the live file, then rename the temp file' only opens
a window in which a reset leaves just the temp file. Three writers did that:
  - writeOtThermoFile() and writeOtBoilerFile() (OTGW-Core.ino): /ot-thermo.json, /ot-boiler.json;
    the loader ignores *.tmp, so the learned OT support maps fall back to empty;
  - appendBootDetectLog() (OTGW-firmware.ino): the /bootdetect.log diagnostic history
    (not the ADR-201 board verdict, which is persisted elsewhere).
The fix commits through fsCommitTempFile() (helperStuff.ino), which checks the size on flash and the
rename result. Static test on each function body of the revision.
"""
import os
import re
import subprocess
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
REV = os.environ.get("OTGW_FSREN_REV")


def read(rel):
    if REV is None:
        return (REPO / rel).read_text(encoding="utf-8").replace("\r\n", "\n")
    return subprocess.run(["git", "-C", str(REPO), "show", f"{REV}:{rel}"], capture_output=True, text=True,
                          encoding="utf-8", check=True).stdout.replace("\r\n", "\n")


def body(src, signature):
    a = src.find(signature)
    if a < 0:
        raise AssertionError(f"{signature} not found")
    return src[a:src.find("\n}\n", a)]


CASES = [
    ("src/OTGW-firmware/OTGW-Core.ino", "static bool writeOtThermoFile("),
    ("src/OTGW-firmware/OTGW-Core.ino", "static bool writeOtBoilerFile("),
    ("src/OTGW-firmware/OTGW-firmware.ino", "static void appendBootDetectLog()"),
]
REMOVE = re.compile(r"LittleFS\.remove\(\s*(canonicalPath|BOOTDETECT_FILE)\s*\)")


class FsRenameWithoutRemove(unittest.TestCase):
    def test_no_remove_of_the_live_file(self):
        for rel, sig in CASES:
            with self.subTest(sig):
                self.assertIsNone(REMOVE.search(body(read(rel), sig)), "removes the live file before the rename")

    def test_commit_through_fs_commit_temp_file(self):
        for rel, sig in CASES:
            with self.subTest(sig):
                b = body(read(rel), sig)
                self.assertIn("fsCommitTempFile(", b)
                self.assertNotIn("LittleFS.rename(", b, "a bare rename skips the size check")
                self.assertRegex(b, r"written = \w+\.position\(\);", "expected size from position(), as writeSettings")


if __name__ == "__main__":
    unittest.main(verbosity=2)
