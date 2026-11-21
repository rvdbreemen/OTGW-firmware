#!/usr/bin/env python3
"""
TASK-2965 (CLI review C2): the Windows tools keep their window open after a double-click.

Explorer starts a .bat through the batfile open command "%1" %*, so cmd.exe runs
  cmd.exe /c ""<full path of the .bat>" "
and closes the window as soon as the script ends: the last message ("Upload this one file", or an
error) disappears before a tester can read it. Both launchers now recognise that command line,
run themselves once more as a child and end with "Press any key to close this window.".

These tests run the REAL launchers (copied into a temp folder, so no unit, no download and no real
capture is involved) with the exact command line Explorer uses, and with the command lines that
PowerShell and cmd produce:
  a  capture-mqtt-debug.bat, double-click, otgw_capture.py missing (error path): waits, exit 1
  b  capture-mqtt-debug.bat, double-click, a stub otgw_capture.py exits 3: waits, exit 3, the
     tool runs once (not twice: the child guard)
  c  flash_otgw.bat, double-click, no image next to it (error path): waits, exit 1
  d  the same runs started like PowerShell (cmd /c ""x.bat"") or like cmd (cmd /c x.bat): no wait;
     the capture launcher keeps its exit code

Windows only (cmd.exe). Run: python tests/test_bat_doubleclick_pause.py
"""

import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
CAPTURE_BAT = REPO / "scripts" / "capture-mqtt-debug.bat"
FLASH_BAT = REPO / "flash_otgw.bat"
PROMPT = "Press any key to close this window."
CMD = os.path.join(os.environ.get("SystemRoot", r"C:\Windows"), "System32", "cmd.exe")


def run(command_line, cwd):
    """Run cmd.exe with this exact command line (a str is passed to CreateProcess verbatim).
    stdin carries one line, so a pause returns at once instead of hanging the test."""
    env = dict(os.environ)
    env.pop("OTGW_PAUSE_CHILD", None)
    proc = subprocess.run(command_line, cwd=cwd, input=b"\r\n", capture_output=True, timeout=120, env=env)
    return proc.returncode, (proc.stdout + proc.stderr).decode("utf-8", "replace")


def double_click(bat):
    return f'"{CMD}" /c ""{bat}" "'


def like_powershell(bat):
    return f'"{CMD}" /c ""{bat}""'


def like_cmd(bat):
    return f'"{CMD}" /c "{bat}"'


@unittest.skipUnless(os.name == "nt", "cmd.exe launcher behaviour: Windows only")
class DoubleClickPauseTest(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix="otgw-bat-"))

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def capture_copy(self, stub_exit=None):
        bat = self.tmp / "capture-mqtt-debug.bat"
        shutil.copyfile(CAPTURE_BAT, bat)
        if stub_exit is not None:
            (self.tmp / "otgw_capture.py").write_text(
                f"print('STUB-CAPTURE-RAN')\nraise SystemExit({stub_exit})\n", encoding="utf-8")
        return bat

    def flash_copy(self):
        bat = self.tmp / "flash_otgw.bat"
        shutil.copyfile(FLASH_BAT, bat)
        # A placeholder esptool.exe keeps the tool from downloading; with no image next to it the
        # tool stops at "locate image(s)" before it would ever run esptool.
        tools = self.tmp / "tools" / "esptool"
        tools.mkdir(parents=True)
        (tools / "esptool.exe").write_bytes(b"")
        return bat

    def test_a_capture_missing_tool_waits(self):
        rc, out = run(double_click(self.capture_copy()), self.tmp)
        self.assertIn("otgw_capture.py was not found", out)
        self.assertIn(PROMPT, out)
        self.assertEqual(rc, 1)

    def test_b_capture_stub_waits_and_runs_once(self):
        rc, out = run(double_click(self.capture_copy(stub_exit=3)), self.tmp)
        self.assertEqual(out.count("STUB-CAPTURE-RAN"), 1, out)
        self.assertIn(PROMPT, out)
        self.assertEqual(rc, 3)

    def test_c_flash_no_image_waits(self):
        rc, out = run(double_click(self.flash_copy()), self.tmp)
        self.assertIn("[ERROR]", out)
        self.assertIn(PROMPT, out)
        self.assertEqual(rc, 1)

    def test_d_no_wait_from_powershell_or_cmd(self):
        cases = [(self.capture_copy(), 1)]
        for bat, want in cases:
            for form in (like_powershell, like_cmd):
                rc, out = run(form(bat), self.tmp)
                self.assertNotIn(PROMPT, out, form.__name__)
                self.assertEqual(rc, want, form.__name__)
        stub = self.capture_copy(stub_exit=3)
        for form in (like_powershell, like_cmd):
            rc, out = run(form(stub), self.tmp)
            self.assertNotIn(PROMPT, out, form.__name__)
            self.assertEqual(rc, 3, form.__name__)
        # flash_otgw.bat: no wait. Its exit code is not checked here: started with cmd /c (not call)
        # it already returned 0 on this error path before TASK-2965 (exit /b inside an if block).
        flash = self.flash_copy()
        for form in (like_powershell, like_cmd):
            rc, out = run(form(flash), self.tmp)
            self.assertIn("[ERROR]", out, form.__name__)
            self.assertNotIn(PROMPT, out, form.__name__)


if __name__ == "__main__":
    unittest.main(verbosity=2)
