#!/usr/bin/env python3
"""ADR-264: src/libraries/espMqttClient is a byte copy of a pinned tag of our fork; OTGW-PROVENANCE.md names the tag and the sha256 of src/.

This test recomputes the hash of the copy and compares it with the one in OTGW-PROVENANCE.md. An edit of the copy in place (forbidden by
ADR-264: a change goes to the fork first, gets a new tag, and is then synced) turns it red; a sync of a new tag must update the provenance
file in the same commit.

The hash: sha256 over, for every file below src/ in sorted order of its forward-slash relative path, the path, a NUL, the file content with
CRLF read as LF, and a NUL. (Line endings do not change the hash, so a Windows checkout and a Linux checkout agree.)

Run: python tests/test_espmqttclient_provenance.py          # test
     python tests/test_espmqttclient_provenance.py --print  # print the hash of the working tree copy
"""
import hashlib
import re
import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
LIB = REPO / "src" / "libraries" / "espMqttClient"
SRC = LIB / "src"
PROV = LIB / "OTGW-PROVENANCE.md"


def src_hash(root: Path = SRC) -> str:
    h = hashlib.sha256()
    for p in sorted(f for f in root.rglob("*") if f.is_file()):
        rel = p.relative_to(root).as_posix()
        h.update(rel.encode("utf-8") + b"\0" + p.read_bytes().replace(b"\r\n", b"\n") + b"\0")
    return h.hexdigest()


class Provenance(unittest.TestCase):
    def setUp(self):
        self.text = PROV.read_text(encoding="utf-8")

    def field(self, name):
        m = re.search(rf"(?m)^- {re.escape(name)}: `?([^`\n]+?)`?\s*$", self.text)
        self.assertIsNotNone(m, f"OTGW-PROVENANCE.md has no '- {name}:' line")
        return m.group(1).strip()

    def test_the_copy_equals_the_recorded_hash(self):
        self.assertEqual(src_hash(), self.field("sha256 of src/"),
                         "src/libraries/espMqttClient/src differs from the tag in OTGW-PROVENANCE.md: change the fork, tag it, sync, update the hash")

    def test_the_provenance_names_fork_tag_commit_and_upstream_base(self):
        self.assertRegex(self.field("fork"), r"^https://github\.com/rvdbreemen/espMqttClient")
        self.assertRegex(self.field("tag"), r"^v\d+\.\d+\.\d+-otgw\.\d+$")
        self.assertRegex(self.field("tag commit"), r"^[0-9a-f]{40}$")
        self.assertRegex(self.field("upstream base"), r"^[0-9a-f]{7,40}$")

    def test_the_hash_is_independent_of_line_endings(self):
        import tempfile, shutil
        d = Path(tempfile.mkdtemp(prefix="emc_hash_"))
        try:
            shutil.copytree(SRC, d / "src")
            for p in (d / "src").rglob("*"):
                if p.is_file() and p.suffix in (".h", ".cpp", ".hpp"):
                    p.write_bytes(p.read_bytes().replace(b"\r\n", b"\n").replace(b"\n", b"\r\n"))
            self.assertEqual(src_hash(d / "src"), src_hash())
        finally:
            shutil.rmtree(d, ignore_errors=True)


if __name__ == "__main__":
    if "--print" in sys.argv:
        print(src_hash())
    else:
        unittest.main(verbosity=2)
