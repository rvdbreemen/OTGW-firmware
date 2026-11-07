#!/usr/bin/env python3
"""TASK-1684: the OT-Thing standalone controller (fixed flow, heating curve, PI room compensation, CH hysteresis) is gone
from OT-Direct. SAT is the only temperature controller there.

Source check: no firmware source (src/OTGW-firmware, every text file type, the web UI and settingsHelp.h included) still names
the controller functions, its quoted write label, the retained ch_suspended topic, the 13 persisted settings keys or the
13 REST/UI setting names. The one allowed mention is the line that empties the old retained ch_suspended topic once
(marked RETIRED-SAT-ENTITY TASK-1684).

OTGW_FW_DIR points at another copy of src/OTGW-firmware (e.g. a `git show HEAD:` export) for the old-vs-fix run.

Run: python tests/test_otd_controller_removed.py
"""
import os
import re
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
FW = Path(os.environ.get("OTGW_FW_DIR", REPO / "src" / "OTGW-firmware"))

SETTING_KEYS = ("OTDchmode OTDflowtemp OTDflowmax OTDroomsetpoint OTDgradient OTDexponent OTDoffset OTDroomcomp "
                "OTDkp OTDki OTDkboost OTDhysteresisenable OTDhysteresis").split()
REST_NAMES = ("otdchmode otdflowtemp otdflowmax otdroomsetpoint otdgradient otdexponent otdoffset otdkp otdki "
              "otdkboost otdhysteresis otdroomcomp otdhysteresisenable").split()

NAMES = re.compile(
    r"\b(?:getFlowTemp|loopPiCtrl|loopCHHysteresis|otPiIntegState|bCHSuspended|ch_suspended)\b|\"heating-curve\"|"
    r"\b(?:" + "|".join(sorted(set(k.lower() for k in SETTING_KEYS + REST_NAMES), key=len, reverse=True)) + r")\b",
)
ALLOWED = "RETIRED-SAT-ENTITY TASK-1684"
SUFFIXES = (".ino", ".cpp", ".h", ".js", ".html", ".css", ".ini", ".json")


class ControllerNamesGone(unittest.TestCase):
    def test_no_controller_name_left_in_firmware_sources(self):
        left = []
        for f in sorted(FW.rglob("*")):
            if not f.is_file() or f.suffix not in SUFFIXES:
                continue
            for n, line in enumerate(f.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
                if (NAMES.search(line) or NAMES.search(line.lower())) and ALLOWED not in line:
                    left.append(f"{f.relative_to(FW).as_posix()}:{n}: {line.strip()[:90]}")
        self.assertEqual(left, [], "\n" + "\n".join(left))

    def test_name_lists_are_complete(self):
        self.assertEqual(len(SETTING_KEYS), 13)
        self.assertEqual(len(REST_NAMES), 13)


if __name__ == "__main__":
    unittest.main(verbosity=2)
