"""TASK-2028: flash_otgw.sh --update writes the app and/or the filesystem without an erase, like flash_otgw.bat.

The real flash_otgw.sh runs from a temporary copy with a fake esptool at tools/esptool/esptool (the place the
script uses, so it downloads nothing). The fake logs its arguments; for read_flash it writes a partition table
that does or does not name the safeboot partition (FAKE_LAYOUT), as the unit's 0x8000 would. The checks are the
ones flash_otgw.bat makes for --update (offsets app0 0x100000, spiffs 0x330000, no -e, a refusal on a non-safeboot
layout, explicit --app/--fs win over auto-detection: TASK-1160). Without the -e erase and without a write at NVS
(0x9000) the WiFi credentials stay: AC2 by construction of the write list.
"""
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
from gitbash import find_bash  # noqa: E402

SCRIPT = Path(os.environ.get("OTGW_FLASH_SH", REPO / "flash_otgw.sh"))
BASH = find_bash()

FAKE_ESPTOOL = r"""#!/usr/bin/env bash
echo "$*" >> "$FAKE_LOG"
prev=""; out=""
for a in "$@"; do out="$a"; done
case " $* " in
  *" read_flash "*)
    if [ "${FAKE_LAYOUT:-safeboot}" = "safeboot" ]; then printf 'nvs\0otadata\0safeboot\0app0\0spiffs\0' > "$out";
    else printf 'nvs\0otadata\0app0\0app1\0spiffs\0' > "$out"; fi ;;
esac
exit 0
"""


@unittest.skipUnless(BASH, "bash not found")
class FlashShUpdate(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix="flashsh-"))
        shutil.copy(SCRIPT, self.tmp / "flash_otgw.sh")
        tools = self.tmp / "tools" / "esptool"
        tools.mkdir(parents=True)
        (tools / "esptool").write_text(FAKE_ESPTOOL, encoding="utf-8", newline="\n")
        (tools / "esptool").chmod(0o755)
        self.log = self.tmp / "esptool.log"
        self.app = self.tmp / "OTGW-firmware-esp32-combo-2.0.0-beta.1+abc1234.ino.bin"
        self.fs = self.tmp / "OTGW-firmware-esp32-combo-2.0.0-beta.1+abc1234.littlefs.bin"
        self.app.write_bytes(b"\xe9app")
        self.fs.write_bytes(b"fs")

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def run_sh(self, *args, layout="safeboot"):
        env = dict(os.environ, FAKE_LOG=self.log.as_posix(), FAKE_LAYOUT=layout, NO_COLOR="1")
        r = subprocess.run([BASH, (self.tmp / "flash_otgw.sh").as_posix(), "--port", "/dev/null", *args],
                           capture_output=True, text=True, env=env, cwd=str(self.tmp), timeout=60)
        calls = self.log.read_text(encoding="utf-8").splitlines() if self.log.exists() else []
        return r.returncode, r.stdout + r.stderr, calls

    def writes(self, calls):
        return [c for c in calls if " write_flash " in " " + c + " "]

    def test_update_with_app_and_fs_writes_both_offsets_without_erase(self):
        rc, out, calls = self.run_sh("--update", "--app", self.app.as_posix(), "--fs", self.fs.as_posix())
        self.assertEqual(rc, 0, out)
        w = self.writes(calls)
        self.assertEqual(len(w), 1, calls)
        self.assertIn("0x100000 " + self.app.as_posix(), w[0])
        self.assertIn("0x330000 " + self.fs.as_posix(), w[0])
        for c in calls:
            self.assertNotIn(" -e", " " + c, "an erase in update mode: %s" % c)
            self.assertNotIn("erase_flash", c)
            self.assertNotIn("0x9000", c, "NVS (WiFi credentials) written: %s" % c)
        self.assertTrue(any(" read_flash 0x8000 " in " " + c for c in calls), "the partition table was not read: %r" % calls)

    def test_explicit_app_writes_the_app_only_and_ignores_a_nearby_filesystem_image(self):
        rc, out, calls = self.run_sh("--update", "--app", self.app.as_posix())
        self.assertEqual(rc, 0, out)
        w = self.writes(calls)
        self.assertEqual(len(w), 1, calls)
        self.assertIn("0x100000", w[0])
        self.assertNotIn("0x330000", w[0], "TASK-1160: an explicit --app must not pick up the filesystem image")

    def test_explicit_fs_writes_the_filesystem_only(self):
        rc, out, calls = self.run_sh("--update", "--fs", self.fs.as_posix())
        self.assertEqual(rc, 0, out)
        w = self.writes(calls)
        self.assertIn("0x330000", w[0])
        self.assertNotIn("0x100000", w[0])

    def test_bare_update_auto_picks_both_release_images(self):
        rc, out, calls = self.run_sh("--update")
        self.assertEqual(rc, 0, out)
        w = self.writes(calls)
        self.assertEqual(len(w), 1, calls)
        self.assertIn(self.app.name, w[0])
        self.assertIn(self.fs.name, w[0])

    def test_a_unit_on_the_legacy_layout_is_refused_before_any_write(self):
        rc, out, calls = self.run_sh("--update", "--app", self.app.as_posix(), layout="legacy")
        self.assertNotEqual(rc, 0, out)
        self.assertEqual(self.writes(calls), [], "written to a unit on the wrong layout: %r" % calls)
        self.assertIn("--migrate", out)

    def test_update_without_any_image_stops(self):
        self.app.unlink(); self.fs.unlink()
        rc, out, calls = self.run_sh("--update")
        self.assertNotEqual(rc, 0, out)
        self.assertEqual(self.writes(calls), [])


if __name__ == "__main__":
    unittest.main()
