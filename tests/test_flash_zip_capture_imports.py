#!/usr/bin/env python3
"""
TASK-2917: the flash zip carries every module that otgw_capture.py imports from scripts/.

otgw_capture.py imports sat_log_to_csv (scripts/otgw_capture.py:250) for the telnet.log -> sattick.csv conversion,
but build.py packed only otgw_capture.py, the launchers and the paho wheel. Every field capture then logged
"ModuleNotFoundError: No module named 'sat_log_to_csv'" (indigo_light, beta.360 Windows and beta.382 macOS).

The test calls the real create_distribution_zip() on a temp project that holds the REAL scripts/otgw_capture.py and
the real scripts/*.py modules next to it, then:
1. checks that every top-level import of otgw_capture.py that exists as scripts/<name>.py is packed beside it;
2. unpacks the zip into an empty folder and runs convert_sat_csv() from the unpacked copy, in a separate isolated
   interpreter (-I, cwd elsewhere), on a telnet.log with one SATTICK line: sattick.csv must come out.
Both fail on the build.py from before the fix.

Run: python tests/test_flash_zip_capture_imports.py
"""

import ast
import io
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

if hasattr(sys.stdout, "buffer"):
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")

REPO_ROOT = Path(__file__).resolve().parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

import build  # noqa: E402 - after the sys.path change

SEMVER = "9.9.9-test.1+abc1234"
TARGET = "esp32-combo"
SCRIPTS = REPO_ROOT / "scripts"


def local_imports(py):
    """Top-level module names that py imports and that exist as scripts/<name>.py (independent of build.py)."""
    names = set()
    for node in ast.walk(ast.parse(py.read_text(encoding="utf-8"))):
        if isinstance(node, ast.Import):
            names.update(a.name.split(".")[0] for a in node.names)
        elif isinstance(node, ast.ImportFrom) and node.module and not node.level:
            names.add(node.module.split(".")[0])
    return sorted(n for n in names if (SCRIPTS / f"{n}.py").is_file() and n != "otgw_capture")


def make_project(root):
    stem = f"OTGW-firmware-{build.asset_slug(TARGET)}-{SEMVER}"
    bd = root / "build"
    bd.mkdir()
    for name in (f"{stem}-merged-full.bin", f"{stem}.ino.bin", f"{stem}.littlefs.bin"):
        (bd / name).write_bytes(b"\x00\x01binary")
    for name in ("flash_otgw.bat", "flash_otgw.sh"):
        shutil.copy2(REPO_ROOT / name, root / name)
    sd = root / "scripts"
    (sd / "vendor").mkdir(parents=True)
    for py in SCRIPTS.glob("*.py"):           # the real tool and every module that could sit next to it
        shutil.copy2(py, sd / py.name)
    for name in ("capture-mqtt-debug.bat", "capture-mqtt-debug.sh", "capture-mqtt-debug-macos.sh"):
        shutil.copy2(SCRIPTS / name, sd / name)
    (sd / "vendor" / "paho_mqtt-9.9.9-py3-none-any.whl").write_bytes(b"PK-not-really")


SATTICK = ("10:04:38.127074 (  55700| 31732) satControlLo(6192): SATTICK room=19.25 raw=19.31 rv=1 target=20.0 "
           "out=7.5 osrc=W curve=29.31 p=0.50 i=1.25 d=-0.03 praw=31.03 pid=31.06 cs=37.8 mm=100 ch=1 mode=2 pwm=1 "
           "duty=0.54 pon=900 poff=780 starts=2 flame=1 flow=38.8 ret=30.1 fc=V fage=1200 rc=S rage=301000 "
           "rsrc=BRD dhw=0\n")

RUNNER = r"""
import sys
from pathlib import Path
here = Path(sys.argv[1])
sys.path.insert(0, str(here))
import otgw_capture
out = otgw_capture.convert_sat_csv(sys.argv[2], sys.argv[3])
print("WROTE", " ".join(sorted(Path(p).name for p in out)))
"""


class FlashZipCaptureImports(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        root = Path(cls.tmp.name) / "proj"
        root.mkdir()
        make_project(root)
        cls.zip_path = build.create_distribution_zip(root, SEMVER, TARGET)
        if cls.zip_path is None:
            raise AssertionError("create_distribution_zip returned None")
        with zipfile.ZipFile(cls.zip_path) as zf:
            cls.names = set(zf.namelist())

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_the_tool_imports_a_local_module(self):
        self.assertIn("sat_log_to_csv", local_imports(SCRIPTS / "otgw_capture.py"),
                      "the test premise changed: otgw_capture.py no longer imports sat_log_to_csv")

    def test_every_local_import_is_packed_next_to_the_tool(self):
        self.assertIn("otgw_capture.py", self.names)
        missing = [n for n in local_imports(SCRIPTS / "otgw_capture.py") if f"{n}.py" not in self.names]
        self.assertEqual(missing, [], f"flash zip lacks {missing}; it holds {sorted(self.names)}")

    def test_the_unpacked_tool_converts_telnet_log_to_csv(self):
        base = Path(self.tmp.name)
        unpacked = base / "unpacked"
        with zipfile.ZipFile(self.zip_path) as zf:
            zf.extractall(unpacked)
        log = base / "telnet.log"
        log.write_text(SATTICK, encoding="utf-8")
        out = base / "csv"
        out.mkdir()
        runner = base / "runner" / "run.py"         # outside the unpacked folder
        runner.parent.mkdir()
        runner.write_text(RUNNER, encoding="utf-8")
        r = subprocess.run([sys.executable, "-I", str(runner), str(unpacked), str(log), str(out)],
                           capture_output=True, text=True, cwd=str(base / "runner"))
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn("sattick.csv", r.stdout)
        self.assertTrue((out / "sattick.csv").is_file())


if __name__ == "__main__":
    unittest.main(verbosity=2)
