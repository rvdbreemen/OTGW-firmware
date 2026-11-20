#!/usr/bin/env python3
"""TASK-2030: docs/MANUAL.md is generated from the English chapters and must match them.

The single-file manual was kept by hand and drifted (it lacked Board mode, bootdetect.log, the BLE PSRAM
default and still named /settings.json). scripts/build_manual.py now builds it; this test fails when the
committed file differs from the generated text, so a chapter edit without a rebuild is caught.

Also checked: every relative link in MANUAL.md resolves from docs/ (the generator rebases chapter links).

Old-vs-fix: on the hand-kept MANUAL.md from before TASK-2030 test_matches_generated fails.
Run: python tests/test_manual_generated.py
"""
import re
import subprocess
import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "scripts"))
import build_manual  # noqa: E402


class ManualGenerated(unittest.TestCase):
    def test_matches_generated(self):
        have = build_manual.OUT.read_text(encoding="utf-8").replace("\r\n", "\n")
        self.assertEqual(have, build_manual.build(),
                         "docs/MANUAL.md differs from the chapters: run python scripts/build_manual.py")

    def test_check_flag_exit_code(self):
        r = subprocess.run([sys.executable, str(REPO / "scripts" / "build_manual.py"), "--check"],
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)

    def test_relative_links_resolve(self):
        text = build_manual.OUT.read_text(encoding="utf-8")
        missing = []
        for target in re.findall(r"\]\(([^)\s]+)\)", text):
            if re.match(r"^[a-z][a-z0-9+.-]*:", target, re.I) or target.startswith(("#", "/")):
                continue
            path = target.split("#", 1)[0]
            if path and not (REPO / "docs" / path).resolve().exists():
                missing.append(target)
        self.assertEqual(missing, [], "links in MANUAL.md that do not resolve from docs/")

    def test_rebase(self):
        self.assertEqual(build_manual.rebase_link("../../hardware/PINOUT.md"), "hardware/PINOUT.md")   # docs/hardware/
        self.assertEqual(build_manual.rebase_link("../../../CHANGELOG.md"), "../CHANGELOG.md")
        self.assertEqual(build_manual.rebase_link("ch04-home-assistant.md#x"), "manuals/en/ch04-home-assistant.md#x")
        self.assertEqual(build_manual.rebase_link("https://x.y/z"), "https://x.y/z")
        self.assertEqual(build_manual.rebase_link("#anchor"), "#anchor")


if __name__ == "__main__":
    unittest.main(verbosity=2)
