#!/usr/bin/env python3
"""The local thermo-nova copy is byte-identical to the pinned commit 7beaaf5.

    python tests/test_thermo_nova_pin.py                                # other-projects/SAT-releases-thermo-nova
    OTGW_THERMO_NOVA_DIR=<dir> python tests/test_thermo_nova_pin.py      # another copy (shows the red case)

The SAT ground-truth rule (.claude/rules/sat-ground-truth.md, ADR-196) cites thermo-nova at commit
7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb and says the local copy is identical to it and is never updated silently.
tests/data/thermo_nova_7beaaf5.sha256 holds the sha256 of every file of the GitHub tarball at that commit. This test
hashes the local folder and fails on a changed, missing or extra file. Allowed beside the pin files: UPSTREAM_COMMIT
(the marker file in OTGW-other-projects that names the pin) and Python caches (__pycache__/, *.pyc).

The folder lives in the private other-projects submodule, which CI cannot fetch; without it the test is skipped.
Moving the pin on purpose means a new manifest from the new tarball in the same commit that updates the rule.
"""
import hashlib
import os
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
MANIFEST = REPO / "tests" / "data" / "thermo_nova_7beaaf5.sha256"
LOCAL = Path(os.environ.get("OTGW_THERMO_NOVA_DIR", REPO / "other-projects" / "SAT-releases-thermo-nova"))
ALLOWED_EXTRA = {"UPSTREAM_COMMIT"}


def load_manifest():
    want = {}
    for line in MANIFEST.read_text(encoding="utf-8").splitlines():
        if not line or line.startswith("#"):
            continue
        digest, rel = line.split("  ", 1)
        want[rel] = digest
    return want


def hash_tree(root: Path):
    have = {}
    for p in sorted(root.rglob("*")):
        if not p.is_file():
            continue
        rel = p.relative_to(root).as_posix()
        if "__pycache__/" in rel or rel.endswith(".pyc") or rel in ALLOWED_EXTRA:
            continue
        have[rel] = hashlib.sha256(p.read_bytes()).hexdigest()
    return have


class ThermoNovaPin(unittest.TestCase):
    def test_manifest_is_complete(self):
        want = load_manifest()
        self.assertEqual(len(want), 127, "the tarball at 7beaaf5 has 127 files")
        self.assertIn("custom_components/sat/heating_control.py", want)

    def test_local_copy_matches_the_pin(self):
        if not (LOCAL / "custom_components" / "sat").is_dir():
            self.skipTest(f"no thermo-nova copy at {LOCAL} (other-projects submodule not checked out)")
        want, have = load_manifest(), hash_tree(LOCAL)
        changed = sorted(r for r in want if r in have and have[r] != want[r])
        missing = sorted(r for r in want if r not in have)
        extra = sorted(r for r in have if r not in want)
        self.assertEqual((changed, missing, extra), ([], [], []),
                         f"{LOCAL} differs from thermo-nova 7beaaf5: changed {changed}, missing {missing}, extra {extra}")


if __name__ == "__main__":
    unittest.main(verbosity=2)
