"""TASK-1792: the self-tune help text and the SATtypes.h comment state the real cadence.

TASK-1889 (ADR-260): the tuner decides after every counted hour on the last 12 counted hours, and a change
empties the window (satTuneCloseDay in SATcontrol.ino), so two steps are at least 12 counted hours apart.
"At most once a day" is no longer true at all. The test reads the real settingsHelp.h and SATtypes.h and the real constants in
SATcurveTune.h, so a changed window length or clean-hour minimum also turns it red.
"""
import json
import re
import unittest
from pathlib import Path

SRC = Path(__file__).resolve().parent.parent / "src" / "OTGW-firmware"


def help_json():
    text = (SRC / "settingsHelp.h").read_text(encoding="utf-8")
    body = re.search(r'R"json\((.*?)\)json"', text, re.S).group(1)
    return json.loads(body)


def const(name):
    text = (SRC / "SATcurveTune.h").read_text(encoding="utf-8")
    return int(re.search(rf"#define\s+{name}\s+(\d+)", text).group(1))


class SelfTuneHelpCadence(unittest.TestCase):
    def test_help_states_the_window_and_the_restart(self):
        t = help_json()["satcurveselftune"]
        hours = const("SAT_CT_WINDOW_DAYS") * const("SAT_CT_HOURS_PER_DAY")   # ADR-260: one-hour decision blocks
        self.assertEqual(const("SAT_CT_HOURS_PER_DAY"), 1)
        self.assertIn(f"the last {hours} counted hours", t)
        self.assertIn(f"at least {hours} counted hours apart", t)
        self.assertIn("by 0.05", t)
        self.assertNotIn("at most once a day", t)

    def test_code_empties_the_window_on_a_change(self):
        code = (SRC / "SATcontrol.ino").read_text(encoding="utf-8")
        body = re.search(r"static void satTuneCloseDay\(.*?\n}\n", code, re.S).group(0)
        self.assertRegex(body, r'out\.reason = PSTR\("phases_agree"\);[^\n]*satTuneEmptyWindow\(t\)')

    def test_types_comment_matches(self):
        line = next(l for l in (SRC / "SATtypes.h").read_text(encoding="utf-8").splitlines()
                    if "bCurveSelfTune" in l and "=" in l)
        self.assertIn("at least 12 counted hours apart", line)   # TASK-1889 (ADR-260)
        self.assertNotIn("at most once a day", line)


if __name__ == "__main__":
    unittest.main()
