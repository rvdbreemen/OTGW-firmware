#!/usr/bin/env python3
"""TASK-1870.1: the IPC stack low points after the hardware detection reach device/info.

    python tests/test_ipc_stack_hwdetect.py                       # the working tree
    OTGW_IPC_REV=<sha> python tests/test_ipc_stack_hwdetect.py     # another revision (old vs fix)

The boot line 'boot: ipc stack free min after hw detect' prints in setup() before a telnet client can
connect, so a bench never saw it. The fix stores both values in state.heapdiag at that point and
device/info reports them as hd_ipc0_stack_min_hwdetect / hd_ipc1_stack_min_hwdetect (-1 = not taken).

Source-order test on the revision's files: in setup() the capture sits after the hardware detection
(appendBootDetectLog(), the durable copy of the detection result) and before the full-init boot line
'boot: ipc stack free min ipc0='; the state fields default to -1; device/info emits both fields from
the snapshot of state.
"""
import os
import re
import subprocess
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
REV = os.environ.get("OTGW_IPC_REV")


def read(rel):
    if REV is None:
        return (REPO / rel).read_text(encoding="utf-8").replace("\r\n", "\n")
    return subprocess.run(["git", "-C", str(REPO), "show", f"{REV}:{rel}"], capture_output=True, text=True,
                          encoding="utf-8", check=True).stdout.replace("\r\n", "\n")


class IpcStackHwDetect(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ino = read("src/OTGW-firmware/OTGW-firmware.ino")
        cls.h = read("src/OTGW-firmware/OTGW-firmware.h")
        cls.rest = read("src/OTGW-firmware/restAPI.ino")
        a = cls.ino.find("void setup()")
        cls.setup = cls.ino[a:cls.ino.find("\n}\n", a)]

    def test_capture_after_detection_and_before_full_init_line(self):
        s = self.setup
        det = s.find("appendBootDetectLog();")
        cap0 = s.find("state.heapdiag.iIpc0StackMinHwDetect = platformIpcStackHighWater(\"ipc0\");")
        cap1 = s.find("state.heapdiag.iIpc1StackMinHwDetect = platformIpcStackHighWater(\"ipc1\");")
        full = s.find("boot: ipc stack free min ipc0=")
        self.assertGreater(det, 0, "hardware detection block not found in setup()")
        self.assertGreater(full, 0, "full-init boot line not found in setup()")
        self.assertTrue(cap0 > det and cap1 > det, "capture missing or before the hardware detection")
        self.assertTrue(cap0 < full and cap1 < full, "capture after the full-init boot line")

    def test_state_fields_default_to_not_taken(self):
        self.assertRegex(self.h, r"int32_t\s+iIpc0StackMinHwDetect\s*=\s*-1;")
        self.assertRegex(self.h, r"int32_t\s+iIpc1StackMinHwDetect\s*=\s*-1;")

    def test_device_info_reports_both(self):
        self.assertRegex(self.rest, r'F\("hd_ipc0_stack_min_hwdetect"\),\s*snap->st\.heapdiag\.iIpc0StackMinHwDetect\)')
        self.assertRegex(self.rest, r'F\("hd_ipc1_stack_min_hwdetect"\),\s*snap->st\.heapdiag\.iIpc1StackMinHwDetect\)')


if __name__ == "__main__":
    unittest.main(verbosity=2)
