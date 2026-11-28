"""The unit's partition layout is told by the size of app0, in flash_otgw.bat as in flash_otgw.sh (TASK-2980).

The batch file cannot run here without a unit, so the PowerShell command that classifies the table dump is cut out of
check_device_layout and run on real 4 KB table dumps (32-byte entries, built like the ones in
test_flash_otgw_sh_update.py). The names must agree with the shell script; the refusal text and the tool size are pinned
as source facts.
"""
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_flash_otgw_sh_update import partition_table  # noqa: E402

BAT = (REPO / "flash_otgw.bat").read_text(encoding="utf-8")
POWERSHELL = shutil.which("powershell") or shutil.which("pwsh")


def classifier():
    m = re.search(r"""for /f "delims=" %%L in \('powershell -NoProfile -Command "(.*?)"'\) do set "DEVICE_LAYOUT=%%L"\s""", BAT)
    assert m, "the layout classifier line of check_device_layout is gone"
    return m.group(1)


class BatLayout(unittest.TestCase):
    def classify(self, layout):
        with tempfile.TemporaryDirectory(prefix="batlayout-") as d:
            p = Path(d) / "pt.bin"
            p.write_bytes(partition_table(layout))
            cmd = classifier().replace("%PT_FILE%", str(p))
            r = subprocess.run([POWERSHELL, "-NoProfile", "-Command", cmd], capture_output=True, text=True, timeout=60)
            self.assertEqual(r.returncode, 0, r.stderr)
            return r.stdout.strip()

    @unittest.skipUnless(POWERSHELL, "powershell not found")
    def test_the_three_tables_are_told_apart_by_the_app0_size(self):
        self.assertEqual(self.classify("legacy"), "legacy")
        self.assertEqual(self.classify("safeboot"), "safeboot")
        self.assertEqual(self.classify("safeboot2"), "safeboot2")

    def test_the_tool_expects_a_table_by_size_and_a_mismatch_points_to_migrate(self):
        self.assertRegex(BAT, r'set "TOOL_APP0_SIZE=0x[0-9A-F]+"')
        self.assertIn("goto :layout_needs_migrate", BAT)
        self.assertIn("flash_otgw.bat --migrate --host", BAT)

    def test_batch_and_shell_name_the_layouts_alike(self):
        sh = (REPO / "flash_otgw.sh").read_text(encoding="utf-8")
        for size, name in (("0x230000", "safeboot"), ("0x240000", "safeboot2")):
            self.assertIn(size, BAT)
            self.assertRegex(sh, r"%s\)\s+echo \"%s\"" % (size, name))
        self.assertRegex(sh, r'TOOL_APP0_SIZE="(0x230000|0x240000)"')
        self.assertEqual(re.search(r'set "TOOL_APP0_SIZE=(0x[0-9A-F]+)"', BAT).group(1),
                         re.search(r'TOOL_APP0_SIZE="(0x[0-9A-F]+)"', sh).group(1))


if __name__ == "__main__":
    unittest.main()
