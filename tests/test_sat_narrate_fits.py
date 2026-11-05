#!/usr/bin/env python3
"""TASK-1663: every SAT narration line fits its buffer, so Telnet and the Web UI live-log never cut it off.

satNarrate_P() / satNarratef_P() format into char buf[SAT_NARRATE_BUF] (SATcontrol.ino), so a line longer than
SAT_NARRATE_BUF - 1 characters is truncated (bench .61, beta.101: "...is at or below the cold setpoint 2").
This test reads every satNarrate_P(PSTR(...)) and satNarratef_P(PSTR(...)) format from the real SAT sources and
computes its longest expansion with these widths per conversion (the values SAT narrates are setpoints, temperatures,
durations, counters and percentages):
  %.Nf -> 4 + 1 + N characters (a value up to -999 with N decimals), %f -> 11
  %d %i %ld %li -> 11, %u %lu -> 10, %x %lx -> 8, %c -> 1, %% -> 1, %s -> the bound listed in KNOWN_S_WIDTHS (a %s without one fails)
It fails when any line can exceed SAT_NARRATE_BUF - 1. The widths are deliberately generous for numbers, so a pass
means the text leaves room for those widths.
Run: python tests/test_sat_narrate_fits.py
"""
import os, re, sys, unittest

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "OTGW-firmware")
SOURCES = [os.path.join(ROOT, f) for f in sorted(os.listdir(ROOT)) if f.startswith("SAT") and f.endswith(".ino")]
CONV = re.compile(r"%(?:(%)|[-+ #0]*(\d+)?(?:\.(\d+))?(hh|h|ll|l|z)?([diuxXfcs]))")
# Every %s needs a known bound: a unique piece of the format text -> the widths of its %s arguments, in order, with the
# source of each bound. A new %s without an entry fails the test, so its length is decided on purpose.
KNOWN_S_WIDTHS = {
    # SATcontrol.ino satTuneRunHourly(): o and n are char[12] (11 characters), the last %s is "above" or "below"
    "Heating curve self-tune: coefficient %s -> %s": [11, 11, 5],
    # SATcontrol.ino satSetControlMode(): "PWM", "Continuous" or "Off"
    "Control mode: %s": [10],
    # SATcycles.ino flame-off narration: the longest _satCycleWord entry, "underheat_pwm"
    "Flame off: cycle %s": [13],
    # SATcycles.ino PWM veto narration: the longer of its two fixed reasons (63 characters)
    "PWM refused (path %u): %s": [63],
}


def buf_size():
    src = open(os.path.join(ROOT, "SATcontrol.ino"), encoding="utf-8", errors="replace").read()
    return int(re.search(r"#define\s+SAT_NARRATE_BUF\s+(\d+)", src).group(1))


def formats():
    """(file, line, format text) for every narration call with a PSTR literal (adjacent literals joined)."""
    out = []
    call = re.compile(r"satNarratef?_P\s*\(\s*PSTR\s*\(((?:\s*\"(?:[^\"\\]|\\.)*\")+)\s*\)", re.S)
    for path in SOURCES:
        src = open(path, encoding="utf-8", errors="replace").read()
        for m in call.finditer(src):
            text = "".join(re.findall(r"\"((?:[^\"\\]|\\.)*)\"", m.group(1)))
            text = bytes(text, "utf-8").decode("unicode_escape")
            out.append((os.path.basename(path), src.count("\n", 0, m.start()) + 1, text))
    return out


def worst_len(fmt):
    n = 0; strings = 0; pos = 0
    known = next((w for key, w in KNOWN_S_WIDTHS.items() if key in fmt), None)
    s_widths = list(known) if known else []
    for m in CONV.finditer(fmt):
        n += len(fmt[pos:m.start()]); pos = m.end()
        pct, width, prec, _len, kind = m.groups()
        if pct: w = 1
        elif kind == "f": w = (4 + 1 + int(prec)) if prec is not None else 11
        elif kind in "di": w = 11
        elif kind == "u": w = 10
        elif kind in "xX": w = 8
        elif kind == "c": w = 1
        else:
            strings += 1
            w = s_widths.pop(0) if s_widths else None
            if w is None: return None, strings   # a %s without a known bound
        if width: w = max(w, int(width))
        n += w
    n += len(fmt[pos:])
    return n, strings


class NarrateFits(unittest.TestCase):
    def test_every_narration_line_fits_the_buffer(self):
        limit = buf_size() - 1
        fm = formats()
        self.assertGreater(len(fm), 20, "the format scan found too few narration calls; the regex no longer matches")
        too_long = []
        for f, line, text in fm:
            n, s = worst_len(text)
            if n is None:
                too_long.append(f"{f}:{line} has a %s without a bound in KNOWN_S_WIDTHS: {text!r}")
            elif n > limit:
                too_long.append(f"{f}:{line} worst {n} > {limit}: {text!r}")
        self.assertEqual(too_long, [], "\n" + "\n".join(too_long))


if __name__ == "__main__":
    unittest.main(verbosity=2)
