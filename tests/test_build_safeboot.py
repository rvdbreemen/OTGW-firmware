#!/usr/bin/env python3
"""
TASK-1223.2/.3 (ADR-191): partition-table invariants and the safeboot build hook.

The 4MB partition table is read by build.py (read_partition_table) and drives
the safeboot build and the merged image. These tests check, on the real CSV:
- app partitions start on a 64K boundary (ESP32 MMU page);
- partitions do not overlap and fit in 4 MB;
- with a "safeboot" partition, the layout is the one ADR-191 records;
- without one, build_safeboot() is a no-op (layout before ADR-191).

Run: python tests/test_build_safeboot.py
"""

import io
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

if hasattr(sys.stdout, "buffer"):
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")

REPO_ROOT = Path(__file__).resolve().parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

import build  # noqa: E402 - after the sys.path change

FLASH_4MB = 0x400000
APP_NAMES = ("safeboot", "app0")


class TestPartitionTable(unittest.TestCase):
    def setUp(self):
        self.table = build.read_partition_table(REPO_ROOT)
        self.assertTrue(self.table, "partition CSV not found or empty")

    def test_app_partitions_64k_aligned(self):
        for name in APP_NAMES:
            if name in self.table:
                offset, size = self.table[name]
                self.assertEqual(offset % 0x10000, 0, f"{name} offset {offset:#x}")
                self.assertEqual(size % 0x10000, 0, f"{name} size {size:#x}")

    def test_no_overlap_and_fits_4mb(self):
        spans = sorted(self.table.values())
        for (o1, s1), (o2, _) in zip(spans, spans[1:]):
            self.assertLessEqual(o1 + s1, o2, f"overlap at {o2:#x}")
        last_offset, last_size = spans[-1]
        self.assertLessEqual(last_offset + last_size, FLASH_4MB)

    def test_safeboot_layout_matches_adr_191(self):
        if "safeboot" not in self.table:
            self.skipTest("layout before ADR-191")
        self.assertEqual(self.table["safeboot"], (0x10000, 0xF0000))
        self.assertEqual(self.table["app0"], (0x100000, 0x230000))
        self.assertEqual(self.table["spiffs"], (0x330000, 0xC0000))


class TestParseCsv(unittest.TestCase):
    def test_parses_comments_and_hex(self):
        with tempfile.TemporaryDirectory() as tmp:
            Path(tmp, build.PARTITIONS_CSV).write_text(
                "# comment\n"
                "nvs,      data, nvs,     0x9000,   0x5000,\n"
                "safeboot, app,  factory, 0x10000,  0xF0000,  # trailing\n",
                encoding="utf-8")
            table = build.read_partition_table(Path(tmp))
        self.assertEqual(table, {"nvs": (0x9000, 0x5000), "safeboot": (0x10000, 0xF0000)})


class TestBuildSafebootHook(unittest.TestCase):
    def test_no_safeboot_partition_is_a_noop(self):
        with mock.patch.object(build, "read_partition_table", return_value={"app0": (0x10000, 0x260000)}), \
             mock.patch.object(build, "run_command") as run:
            self.assertIsNone(build.build_safeboot(REPO_ROOT, "0.0.0"))
            run.assert_not_called()

    def test_oversize_image_fails(self):
        with tempfile.TemporaryDirectory() as tmp:
            proj = Path(tmp)
            image = proj / build.SAFEBOOT_DIR / ".pio" / "build" / "safeboot" / "firmware.bin"
            image.parent.mkdir(parents=True)
            image.write_bytes(b"\0" * 0x1001)
            with mock.patch.object(build, "read_partition_table", return_value={"safeboot": (0x10000, 0x1000)}), \
                 mock.patch.object(build, "run_command"):
                with self.assertRaises(SystemExit) as ctx:
                    build.build_safeboot(proj, "0.0.0")
            self.assertEqual(ctx.exception.code, 1)


class TestMigrationSlices(unittest.TestCase):
    """TASK-1223.8: the two migration images skip exactly the NVS partition."""

    def test_slices_skip_nvs(self):
        with tempfile.TemporaryDirectory() as tmp:
            merged = Path(tmp) / "OTGW-firmware-esp32-combo-1.0.0-merged-full.bin"
            data = bytes(i % 251 for i in range(0x20000))
            merged.write_bytes(data)
            table = {"nvs": (0x9000, 0x5000), "safeboot": (0x10000, 0xF0000)}
            with mock.patch.object(build, "read_partition_table", return_value=table):
                first, rest, offset = build.write_migration_slices(Path(tmp), merged)
            self.assertEqual(offset, 0xE000)
            self.assertEqual(first.name, "OTGW-firmware-esp32-combo-1.0.0-migrate-0x0.bin")
            self.assertEqual(rest.name, "OTGW-firmware-esp32-combo-1.0.0-migrate-rest.bin")
            self.assertEqual(first.read_bytes(), data[:0x9000])
            self.assertEqual(rest.read_bytes(), data[0xE000:])

    def test_no_safeboot_no_slices(self):
        with tempfile.TemporaryDirectory() as tmp:
            merged = Path(tmp) / "x-merged-full.bin"
            merged.write_bytes(b"\0" * 0x10000)
            with mock.patch.object(build, "read_partition_table", return_value={"nvs": (0x9000, 0x5000)}):
                self.assertIsNone(build.write_migration_slices(Path(tmp), merged))


if __name__ == "__main__":
    unittest.main(verbosity=2)
