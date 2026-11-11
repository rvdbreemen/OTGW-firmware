#!/usr/bin/env python3
"""TASK-1759 / ADR-224: source guards for the OT-Direct MsgID 28 slot.

ADR-224 (Accepted, binding: false) has no Enforcement block; these are its two checkable rules, kept here as a
regular test (the coordinator's decision of 2026-11-10: an Accepted ADR is not edited):
  1. The interval is the 30 s of Sergeant D's entry 291 (OTD_RET_SLOT_INTERVAL_MS = 30000UL).
  2. In loopOTDirect()'s relay path the cache-answered MsgID 28 slot comes right after the TASK-1707 slot
     (otdAltSlotTaken), never before it, so an unsupported-ID slot keeps priority.
Behaviour: tests/test_otd_return_temp_slot.py.
OTGW_OTD_INO points at another copy of OTDirect.ino (old code: both fail).
"""
import os
import re
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
OTD_INO = Path(os.environ.get("OTGW_OTD_INO", REPO / "src" / "OTGW-firmware" / "OTDirect.ino"))


class ReturnTempSlotSource(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.src = OTD_INO.read_text(encoding="utf-8").replace("\r\n", "\n")

    def test_interval_is_30s(self):
        self.assertIn("OTD_RET_SLOT_INTERVAL_MS = 30000UL", self.src,
                      "ADR-224 / entry 291: the MsgID 28 slot runs every 30 s; change it only with a new decision")

    def test_cache_slot_comes_right_after_the_task_1707_slot(self):
        m = re.search(r"if \(otdAltSlotTaken\(otSlaveFrame\)\) \{\s*otSlaveFramePending = false;\s*\}\s*"
                      r"(?://[^\n]*\n\s*)*else if \(otdRetSlotTaken\(otSlaveFrame\)\)", self.src)
        self.assertIsNotNone(m, "ADR-224: otdRetSlotTaken() must be the branch right after otdAltSlotTaken() in the relay path")
        self.assertEqual(self.src.count("otdRetSlotTaken(otSlaveFrame)"), 1, "the slot is taken in one place only")


if __name__ == "__main__":
    unittest.main()
