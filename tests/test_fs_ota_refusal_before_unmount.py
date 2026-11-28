"""TASK-2980 / ADR-298: a refused filesystem upload (POST /update?cmd=100) leaves LittleFS mounted and untouched.

Bench .76 (2.0.0-beta.482, old table), findings of 2026-11-28: POST /update?cmd=100&size=720896 (an image for the other table) and the
same call without size= were refused correctly, but only AFTER the start had paused the writer, copied the runtime files to the heap,
filled the RTC snapshot and called LittleFS.end(). The error completion handler remounts only when !LittleFSmounted, and the start keeps
LittleFSmounted true across LittleFS.end(), so the filesystem stayed unmounted: version.hash, GET / and /settings.ini failed until a
/ReBoot. The fix decides every refusal that needs only the query string and the partition size BEFORE any of those steps.

What this test checks (text, on the real header, the flash itself needs a unit): in the filesystem branch of _handleUploadStart() the
three refusals (size= missing, image too large, image smaller than the partition) come before fsWriterPauseAndFlush, _keepFiles,
fsOtaRtcFill and LittleFS.end(); none of those four steps sits in the refusal path; the refusal strings are unchanged. Red on the code
before the change (the refusals follow LittleFS.end()). The bench-tester repeats the two POSTs on .76: LittleFS must stay mounted.
"""
import re
import unittest
from pathlib import Path

HDR = (Path(__file__).resolve().parent.parent / "src" / "OTGW-firmware" / "OTGW-ModUpdateServer-esp32.h").read_text(encoding="utf-8").replace("\r\n", "\n")


def fs_branch():
    i = HDR.index("if (_isFilesystemTarget(request, filename)) {")
    j = HDR.index("} else {\n      _uploadTarget = UploadTarget::Firmware;", i)
    return HDR[i:j]


class RefuseBeforeTouching(unittest.TestCase):
    def test_refusals_come_before_every_step_that_touches_littlefs(self):
        b = fs_branch()
        refusals = [b.index('"filesystem upload needs size=<bytes>'), b.index('"filesystem image too large"'),
                    b.index("does not match this unit's partition layout")]
        for step in ("fsWriterPauseAndFlush(2000)", "_keepFiles();", "fsOtaRtcFill();", "LittleFS.end();"):
            pos = b.index(step)
            for r in refusals:
                self.assertLess(r, pos, "a refusal must be decided before " + step)

    def test_the_refusal_path_unmounts_nothing(self):
        b = fs_branch()
        m = re.search(r"if \(fsRefusal\) \{(.*?)\n      \} else \{", b, re.S)
        self.assertIsNotNone(m, "refusal block not found")
        for step in ("LittleFS.end", "_keepFiles", "fsOtaRtcFill", "fsWriterPause", "bFsImageWriting"):
            self.assertNotIn(step, m.group(1))
        self.assertIn("strlcpy(_updaterError, fsRefusal", m.group(1))

    def test_update_begin_still_runs_only_after_the_unmount(self):
        b = fs_branch()
        self.assertLess(b.index("LittleFS.end();"), b.index("Update.begin(fsSize, U_SPIFFS)"))

    def test_the_three_refusal_texts_are_unchanged(self):
        for t in ("filesystem upload needs size=<bytes> in the query string (POST /update?cmd=100&size=<bytes>)",
                  "filesystem image too large",
                  "filesystem image does not match this unit's partition layout: migrate once over USB (flash_otgw --migrate)"):
            self.assertIn(t, HDR)


if __name__ == "__main__":
    unittest.main()
