#!/usr/bin/env python3
"""TASK-1909: an OTA filesystem upload during setup() must not lose /reboot_count.txt or the boot line.

What went wrong (.63, beta.224 release flash 2026-11-13 19:37): the reboot counter came back as 1 and
the app-boot line of /reboot_log.txt was missing. The flasher starts the filesystem upload as soon as
the new app answers HTTP, which is while setup() still runs (09's captures b227/b228: "[OTA] Flash start"
before initSAT and before the "boot:" signature):

  1. setup() started the web server (OTGW-firmware.ino startWebserver()) BEFORE it wrote the reboot
     counter and log. An upload in that window copies both files to the heap (_keepFiles) and unmounts
     LittleFS; the new count and boot line written after it are not in the copy, and the restore puts the
     old log back without them.
  2. updateRebootCount()/updateRebootLog() call LittleFS.begin(), which MOUNTS the partition while the
     Updater overwrites it, and write into it.
  3. At the end of the upload _remountAfterFsWrite() calls LittleFS.begin(), which returns true WITHOUT
     mounting when a mount is already there (framework LittleFS.cpp:58-61), so the restore writes the
     new image through the stale mount from step 2. That is the likely way the counter ended up as 1
     (an interpretation: the on-flash result of step 3 cannot be reproduced on the host).

This test checks the three fixes on the real sources (static, no host compile: the setup() order, the
framework mount semantics and the Updater do not run on the host). It is red on the code before the fix.
The bench check (09) is an FS upload started on the first HTTP 200 after the app boot.

Old-vs-fix: OTGW_FW_DIR=<directory with OTGW-firmware.ino, helperStuff.ino, OTGW-ModUpdateServer-esp32.h>.
Run: python tests/test_fs_upload_boot_writes.py
"""
import os
import re
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
FW = Path(os.environ.get("OTGW_FW_DIR") or HERE.parent / "src" / "OTGW-firmware")


def code(name):
    """The file without // comments, so a comment that names a call does not count as the call."""
    text = (FW / name).read_text(encoding="utf-8").replace("\r\n", "\n")
    return re.sub(r"//[^\n]*", "", text)


def body(text, signature):
    i = text.index(signature)
    j = text.index("\n}\n", i) if text.find("\n}\n", i) >= 0 else len(text)
    return text[i:j]


class FsUploadBootWrites(unittest.TestCase):
    def test_reboot_counter_and_log_are_written_before_the_web_server_starts(self):
        setup = body(code("OTGW-firmware.ino"), "void setup()")
        web = setup.index("startWebserver();")
        self.assertLess(setup.index("updateRebootCount()"), web, "an FS upload can start before the counter is written")
        self.assertLess(setup.index("updateRebootLog("), web, "an FS upload can start before the boot line is written")
        self.assertLess(setup.index("platformResetReason(lastReset"), setup.index("updateRebootLog("))

    def test_the_remount_drops_any_mount_made_during_the_write(self):
        upd = code("OTGW-ModUpdateServer-esp32.h")
        remount = body(upd, "bool _remountAfterFsWrite(bool partial)").split("\n  }\n")[0]
        self.assertIn("LittleFS.end()", remount, "begin() on an existing mount does not mount the new image")
        self.assertLess(remount.index("LittleFS.end()"), remount.index("LittleFS.begin()"))
        self.assertIn("bFsImageWriting = false", remount)

    def test_the_upload_marks_the_image_write_right_after_unmounting(self):
        upd = code("OTGW-ModUpdateServer-esp32.h")
        m = re.search(r"_keepFiles\(\);\s*LittleFS\.end\(\);\s*::state\.flash\.bFsImageWriting = true;", upd)
        self.assertIsNotNone(m, "the flag is not set where the upload unmounts LittleFS")
        # The error response path (image too large, Update.begin() failed) clears it again.
        self.assertRegex(upd, r"bESPactive = false;\s*_dropKeptFiles\(\);\s*::state\.flash\.bFsImageWriting = false;")

    def test_no_helper_mounts_littlefs_while_an_image_is_written(self):
        helper = code("helperStuff.ino")
        calls = [m.start() for m in re.finditer(r"LittleFS\.begin\(\)", helper)]
        self.assertTrue(calls, "helperStuff.ino has no LittleFS.begin() calls any more; revisit this test")
        unguarded = []
        for pos in calls:
            line = helper[helper.rfind("\n", 0, pos) + 1:helper.find("\n", pos)]
            if "bFsImageWriting" not in line:
                unguarded.append(line.strip())
        self.assertEqual(unguarded, [], "LittleFS.begin() without the image-write guard:\n  " + "\n  ".join(unguarded))

    def test_the_flag_exists_in_the_flash_state(self):
        self.assertRegex(code("Flashtypes.h"), r"bool bFsImageWriting\s*= false;")


if __name__ == "__main__":
    unittest.main(verbosity=2)
