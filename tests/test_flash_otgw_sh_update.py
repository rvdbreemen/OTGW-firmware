"""TASK-2028: flash_otgw.sh --update writes the app and/or the filesystem without an erase, like flash_otgw.bat.

The real flash_otgw.sh runs from a temporary copy with a fake esptool at tools/esptool/esptool (the place the
script uses, so it downloads nothing). The fake logs its arguments; for read_flash it writes a partition table
that does or does not name the safeboot partition (FAKE_LAYOUT), as the unit's 0x8000 would. The checks are the
ones flash_otgw.bat makes for --update (offsets app0 0x100000, spiffs 0x330000, no -e, a refusal on a non-safeboot
layout, explicit --app/--fs win over auto-detection: TASK-1160). Without the -e erase and without a write at NVS
(0x9000) the WiFi credentials stay: AC2 by construction of the write list.
"""
import os
import re
import shutil
import struct
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
  *" read_flash "*) cp "$FAKE_TABLE" "$out" ;;
esac
exit 0
"""


def partition_table(layout):
    """The 0x1000 bytes the unit holds at 0x8000: 32-byte entries (magic AA 50, type, subtype, offset, size, label[16], flags).
    'safeboot' = the ADR-191 table (app0 2240K), 'safeboot2' = app0 2304K and a 704K LittleFS, 'legacy' = the table before safeboot."""
    def ent(label, typ, sub, off, size):
        return b"\xaa\x50" + bytes([typ, sub]) + struct.pack("<II", off, size) + label.encode().ljust(16, b"\0") + b"\0\0\0\0"
    if layout == "safeboot":
        rows = [("nvs", 1, 2, 0x9000, 0x5000), ("otadata", 1, 0, 0xE000, 0x2000), ("safeboot", 0, 0, 0x10000, 0xF0000),
                ("app0", 0, 0x10, 0x100000, 0x230000), ("spiffs", 1, 0x82, 0x330000, 0xC0000), ("coredump", 1, 3, 0x3F0000, 0x10000)]
    elif layout == "safeboot2":
        rows = [("nvs", 1, 2, 0x9000, 0x5000), ("otadata", 1, 0, 0xE000, 0x2000), ("safeboot", 0, 0, 0x10000, 0xF0000),
                ("app0", 0, 0x10, 0x100000, 0x240000), ("spiffs", 1, 0x82, 0x340000, 0xB0000), ("coredump", 1, 3, 0x3F0000, 0x10000)]
    else:
        rows = [("nvs", 1, 2, 0x9000, 0x5000), ("otadata", 1, 0, 0xE000, 0x2000), ("app0", 0, 0x10, 0x10000, 0x1E0000),
                ("app1", 0, 0x11, 0x1F0000, 0x1E0000), ("spiffs", 1, 0x82, 0x3D0000, 0x20000)]
    return b"".join(ent(*r) for r in rows).ljust(0x1000, b"\xff")


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

    def run_sh(self, *args, layout="safeboot", tool=None):
        if tool:
            # The table this package's images are built for, pinned in the temporary copy of the script as the app0 size
            # (TOOL_APP0_SIZE). A script from before the guard has no such line: the pin then changes nothing, which is
            # exactly the old tool handed new images (red on the old script, green on the fixed one).
            p = self.tmp / "flash_otgw.sh"
            t = p.read_text(encoding="utf-8")
            size = {"safeboot": "0x230000", "safeboot2": "0x240000"}[tool]
            p.write_text(re.sub(r'(?m)^TOOL_APP0_SIZE=".*"$', 'TOOL_APP0_SIZE="%s"' % size, t), encoding="utf-8", newline="\n")
        table = self.tmp / ("table-%s.bin" % layout)
        table.write_bytes(partition_table(layout))
        env = dict(os.environ, FAKE_LOG=self.log.as_posix(), FAKE_TABLE=table.as_posix(), NO_COLOR="1")
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

    # The layout is told by the size of app0, not by the name "safeboot": the table of the +64K repartition has the
    # same names as the old one, and a write at the new offsets onto the old table lands inside the old filesystem.
    def test_new_images_on_a_unit_with_the_old_safeboot_table_are_refused_before_any_write(self):
        rc, out, calls = self.run_sh("--update", "--app", self.app.as_posix(), "--fs", self.fs.as_posix(),
                                     layout="safeboot", tool="safeboot2")
        self.assertNotEqual(rc, 0, out)
        self.assertEqual(self.writes(calls), [], "written to a unit on the old table: %r" % calls)
        self.assertIn("--migrate", out)

    def test_old_images_on_a_unit_with_the_new_table_are_refused_before_any_write(self):
        rc, out, calls = self.run_sh("--update", "--app", self.app.as_posix(), layout="safeboot2", tool="safeboot")
        self.assertNotEqual(rc, 0, out)
        self.assertEqual(self.writes(calls), [], "written to a unit on the new table: %r" % calls)

    def test_new_images_on_a_unit_with_the_new_table_are_written(self):
        rc, out, calls = self.run_sh("--update", "--app", self.app.as_posix(), layout="safeboot2", tool="safeboot2")
        self.assertEqual(rc, 0, out)
        self.assertEqual(len(self.writes(calls)), 1, calls)

    def test_the_unit_layout_is_named_in_the_output(self):
        rc, out, calls = self.run_sh("--update", "--app", self.app.as_posix(), layout="safeboot2", tool="safeboot2")
        self.assertIn("Unit layout: safeboot2", out)

    def test_update_without_any_image_stops(self):
        self.app.unlink(); self.fs.unlink()
        rc, out, calls = self.run_sh("--update")
        self.assertNotEqual(rc, 0, out)
        self.assertEqual(self.writes(calls), [])


if __name__ == "__main__":
    unittest.main()
