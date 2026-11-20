#!/usr/bin/env python3
"""TASK-1986: the capture upload fits one Discord attachment and always holds the summary and the crash log.

gruijter's beta.250 upload (2026-11-15) held 7 files: the zip was too big for Discord, so he packed by hand and the
MQTT log, summary.txt and crashlog.log were left out. build_upload_zip() now keeps summary.txt and crashlog.log in
every upload, packs a too-big zip as .tar.xz, and when that is still too big puts the transcript, summary, crash log,
core dump and device state in the first file and the rest in <run>-upload-part2.tar.xz, saying so.

    python tests/test_capture_upload_size.py

The limit is a parameter, so the three paths run with small files: fits as zip, fits only as .tar.xz, split.
"""
import os
import random
import sys
import tarfile
import tempfile
import unittest
import zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "scripts"))
import otgw_capture as oc  # noqa: E402


class UploadSize(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.run = Path(self.tmp.name) / "20261115-120000"
        self.run.mkdir()
        self.cap = oc.Capture(self.run, echo=False)
        rnd = random.Random(1986)
        # a telnet-like transcript: repetitive text that xz packs far better than deflate
        lines = [f"12:{m:02d}:{s:02d}.{rnd.randint(0, 999999):06d} (  45{rnd.randint(0, 999):03d}| 21492) processOT   (6217): "
                 f"Boiler B40192DBD  25 Read-Ack > Tboiler = {rnd.randint(300, 600) / 10:.2f} C\n"
                 for m in range(60) for s in range(60) for _ in range(3)]
        self.transcript = self.run.parent / "transcript-20261115-120000.txt"
        self.transcript.write_text("".join(lines), encoding="utf-8")
        self.cap.summary("Capture started: test")
        self.cap.store.write("crashlog.log", "12:00:00  crashlog: available:false\n")
        (self.run / oc.FRAMES_LOG).write_bytes(os.urandom(300_000))   # does not compress: lands in part 2
        (self.run / "device-state-start.txt").write_text("device/info {}\n", encoding="utf-8")
        self.cap.store.close()

    def names_zip(self, p):
        with zipfile.ZipFile(p) as z:
            return set(z.namelist())

    def names_tar(self, p):
        with tarfile.open(p, "r:xz") as t:
            return set(t.getnames())

    def summary(self):
        return (self.run / "summary.txt").read_text(encoding="utf-8")

    def test_fits_as_zip_and_holds_summary_and_crashlog(self):
        target = oc.build_upload_zip(self.cap, self.transcript, False, limit=10**9)
        self.assertEqual(target.suffix, ".zip")
        names = self.names_zip(target)
        for n in (self.transcript.name, "summary.txt", "crashlog.log", oc.FRAMES_LOG, "device-state-start.txt"):
            self.assertIn(n, names)

    def test_too_big_zip_becomes_tar_xz(self):
        zsize = oc.build_upload_zip(self.cap, self.transcript, False, limit=10**9).stat().st_size
        target = oc.build_upload_zip(self.cap, self.transcript, False, limit=zsize - 1)
        self.assertTrue(target.name.endswith(".tar.xz"), target)
        self.assertLess(target.stat().st_size, zsize)
        self.assertFalse(oc.upload_zip_path(self.run).exists(), "the too-big zip is removed")
        self.assertFalse(oc.upload_part2_path(self.run).exists())
        self.assertIn("summary.txt", self.names_tar(target))
        self.assertIn("packed as", self.summary())

    def test_still_too_big_splits_with_the_essentials_first(self):
        target = oc.build_upload_zip(self.cap, self.transcript, False, limit=250_000)
        part2 = oc.upload_part2_path(self.run)
        self.assertTrue(part2.exists())
        first, second = self.names_tar(target), self.names_tar(part2)
        self.assertEqual(first, {self.transcript.name, "summary.txt", "crashlog.log", "device-state-start.txt"})
        self.assertIn(oc.FRAMES_LOG, second)
        self.assertTrue(first.isdisjoint(second))
        self.assertIn("Upload both", self.summary())
        self.assertLessEqual(target.stat().st_size, 250_000)
        self.assertIn("still", self.summary())   # part 2 (random bytes) cannot fit: the tool says so


if __name__ == "__main__":
    unittest.main(verbosity=2)
