"""flash_esp.py never writes ESP32 component images without the partition table (TASK-2980).

The table of the +64K repartition has the same partition names as the ADR-191 table but another app0 size and LittleFS
offset. flash_esp.py cannot read the unit's table, so a firmware or filesystem image written alone (-f / -s, or build
artifacts without esp32-partitions.bin) could land at offsets the unit does not use. The tool refuses that case and
passes the merged images and the components that carry the table.
"""
import importlib
import subprocess
import sys
import unittest
from pathlib import Path
from unittest import mock

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO))


def load():
    sys.modules.pop("flash_esp", None)
    return importlib.import_module("flash_esp")


class TableGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.fe = load()

    def test_firmware_alone_is_refused(self):
        self.assertIn("partition table", self.fe.components_without_table_problem({"firmware": Path("a.ino.bin")}))

    def test_filesystem_alone_is_refused(self):
        self.assertIsNotNone(self.fe.components_without_table_problem({"filesystem": Path("a.littlefs.bin")}))

    def test_a_table_without_the_filesystem_is_refused(self):
        # a new table moves the filesystem; the old one would stay behind at an offset the unit no longer uses
        for a in ({"firmware": Path("a"), "partitions": Path("p")}, {"partitions": Path("p")},
                  {"filesystem": Path("b"), "partitions": Path("p")}):
            self.assertIn("without both the firmware and the filesystem", self.fe.components_without_table_problem(a))

    def test_components_with_the_table_are_allowed(self):
        a = {"firmware": Path("a"), "filesystem": Path("b"), "partitions": Path("p"), "bootloader": Path("bl")}
        self.assertIsNone(self.fe.components_without_table_problem(a))

    def test_flash_device_does_not_call_esptool_for_a_table_less_component_write(self):
        art = {"firmware": Path(__file__)}   # an existing file so size lookups work
        with mock.patch.object(self.fe.subprocess, "run") as run:
            ok = self.fe.flash_device("esp32", "COM1", art)
        self.assertFalse(ok)
        run.assert_not_called()

    def test_a_merged_image_is_still_written(self):
        art = {"merged_full": Path(__file__)}
        with mock.patch.object(self.fe.subprocess, "run") as run:
            ok = self.fe.flash_device("esp32", "COM1", art)
        self.assertTrue(ok)
        write = [c for c in run.call_args_list if "0x0" in c[0][0]]
        self.assertEqual(len(write), 1, run.call_args_list)


if __name__ == "__main__":
    unittest.main()
