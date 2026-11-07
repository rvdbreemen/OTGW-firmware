"""TASK-1694: the OT-Direct boot auto-detect must not persist the master mode it picks.

Source-level check on the real OTDirect.ino: the auto-detect block switches with
applyOTDirectMode() (RAM only), applyOTDirectMode() never writes OTDmode, and the
user path setOTDirectMode() still persists it.
Run: python tests/test_otd_autodetect_no_persist.py [--old-rev REV]
"""
import re
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REL = "src/OTGW-firmware/OTDirect.ino"
OLD_REV = None


def source():
    if OLD_REV:
        return subprocess.run(["git", "show", f"{OLD_REV}:{REL}"], cwd=ROOT, check=True,
                              capture_output=True, text=True, encoding="utf-8").stdout
    return (ROOT / REL).read_text(encoding="utf-8")


def body(src, signature):
    start = src.index(signature)
    depth, i = 0, src.index("{", start)
    for j in range(i, len(src)):
        depth += {"{": 1, "}": -1}.get(src[j], 0)
        if depth == 0:
            return src[i:j + 1]
    raise AssertionError("unbalanced body for " + signature)


class AutoDetectDoesNotPersist(unittest.TestCase):
    def setUp(self):
        self.src = source()
        a = self.src.index("Auto-detect thermostat presence")
        self.block = self.src[a:self.src.index("\n}\n", a)]

    def test_autodetect_switches_without_persisting(self):
        self.assertNotRegex(self.block, r"\bsetOTDirectMode\s*\(",
                            "auto-detect must not call the persisting setOTDirectMode()")
        self.assertRegex(self.block, r"\bapplyOTDirectMode\s*\(\s*OTD_MODE_MASTER\s*\)")

    def test_apply_does_not_write_the_mode(self):
        self.assertNotIn('updateSetting("OTDmode"', body(self.src, "static void applyOTDirectMode("))

    def test_user_path_still_persists(self):
        b = body(self.src, "static void setOTDirectMode(")
        self.assertIn("applyOTDirectMode(newMode)", b)
        self.assertIn('updateSetting("OTDmode"', b)


if __name__ == "__main__":
    if "--old-rev" in sys.argv:
        k = sys.argv.index("--old-rev"); OLD_REV = sys.argv[k + 1]; del sys.argv[k:k + 2]
    unittest.main()
