"""TASK-1792: the self-tune help text and the SATtypes.h comment state the real cadence.

The tuner decides once a day, but a change empties the 3-counted-day window (satTuneCloseDay in
SATcontrol.ino), so two steps are at least 3 counted days apart. "At most once a day" alone suggests
a step every day. The test reads the real settingsHelp.h and SATtypes.h and the real constants in
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
        days, hours = const("SAT_CT_WINDOW_DAYS"), const("SAT_CT_MIN_CLEAN_HOURS")
        self.assertIn(f"{days} counted days", t)
        self.assertIn(f"at least {hours} clean heating hours", t)
        self.assertIn(f"at least {days} counted days apart", t)
        self.assertNotIn("at most once a day", t)

    def test_code_empties_the_window_on_a_change(self):
        code = (SRC / "SATcontrol.ino").read_text(encoding="utf-8")
        body = re.search(r"static void satTuneCloseDay\(.*?\n}\n", code, re.S).group(0)
        self.assertRegex(body, r'out\.reason = PSTR\("phases_agree"\);[^\n]*satTuneEmptyWindow\(t\)')

    def test_types_comment_matches(self):
        line = next(l for l in (SRC / "SATtypes.h").read_text(encoding="utf-8").splitlines()
                    if "bCurveSelfTune" in l and "=" in l)
        self.assertIn("at least 3 counted days apart", line)
        self.assertNotIn("at most once a day", line)


if __name__ == "__main__":
    unittest.main()
