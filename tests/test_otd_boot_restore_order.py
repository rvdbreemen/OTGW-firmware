#!/usr/bin/env python3
"""TASK-1944: at boot the persisted OT-Direct command state is restored after the thermostat auto-detect.

initOTDirect() switches to master for the boot when the auto-detect sees no thermostat (applyOTDirectMode(OTD_MODE_MASTER)),
and a switch into master runs the full resetTransientState() (TASK-1839 scope): HW back to auto, the UI= list and the write
caches and overrides cleared. Restoring the persisted HW, SW, SH, IT, OH, UI= list (otdRestorePersistedCommands, TASK-1836)
and the vent setpoint (TASK-584) before that switch lost them on every boot of a standalone unit. The restore now comes after
the auto-detect and before the boot handshake (TASK-1835), so the handshake already sees the restored state.

Source-order test on the real initOTDirect() (initOTDirect waits 5 s on real OpenTherm hardware and cannot run on the host);
the reset itself is tested on the real code in test_otd_mode_change_keeps_state.py. OTGW_OTD_INO points at another copy for
old-vs-fix.
Run: python tests/test_otd_boot_restore_order.py
"""
import os
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
OTD_INO = Path(os.environ.get("OTGW_OTD_INO", REPO / "src" / "OTGW-firmware" / "OTDirect.ino"))


def init_body():
    src = OTD_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
    a = src.index("void initOTDirect() {")
    return src[a:src.index("\n}\n", a)]


class BootRestoreOrder(unittest.TestCase):
    def setUp(self):
        self.body = init_body()
        self.i_switch = self.body.index("applyOTDirectMode(OTD_MODE_MASTER);")
        self.i_shake = self.body.index("if (IS_MASTER_MODE()) otdBootHandshake();")

    def test_persisted_commands_are_restored_after_the_auto_detect_switch(self):
        i = self.body.index("otdRestorePersistedCommands();")
        self.assertGreater(i, self.i_switch, "the restore runs before the auto-detect switch, whose full reset wipes it")
        self.assertLess(i, self.i_shake, "the restore must come before the boot handshake")
        self.assertEqual(self.body.count("otdRestorePersistedCommands();"), 1)

    def test_vent_setpoint_is_restored_after_the_auto_detect_switch(self):
        i = self.body.index("updateWriteCache(71, ventData);")
        self.assertGreater(i, self.i_switch)
        self.assertLess(i, self.i_shake)
        self.assertGreater(self.body.index("setOverride(71, ventData);"), self.i_switch)

    def test_the_restore_does_not_depend_on_the_mode_switch(self):
        # the restore is not inside the auto-detect block: it also runs in gateway, monitor, bypass and a persisted master mode
        detect = self.body[self.body.index("if (settings.otd.bAutoDetect && otCurrentMode == OTD_MODE_GATEWAY) {"):]
        detect = detect[:detect.index("\n  }\n") + 4]
        self.assertNotIn("otdRestorePersistedCommands();", detect)


if __name__ == "__main__":
    unittest.main(verbosity=2)
