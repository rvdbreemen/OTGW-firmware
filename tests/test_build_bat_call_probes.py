#!/usr/bin/env python3
"""TASK-1898: build.bat runs its interpreter probes through CALL.

In a worktree without .build-venv, build.bat goes :bootstrap_build_venv -> :find_python and runs
`py -3 -c ...`, `python -c ...` and `python3 -c ...`, then `%BASE_PYTHON% -m venv`. A "python" on
PATH can be a .cmd/.bat shim (on Robert's machine C:\\tools\\Codex\\python.cmd). cmd.exe runs a
batch file named without CALL as a jump, not a call: control never comes back, so build.bat ended
right after that probe with exit 1 and printed nothing (TASK-1885 build attempts, 2026-11-13).

This check reads the two labels and fails when a line there starts an interpreter without CALL.
The behaviour itself (a shim no longer ends build.bat) is shown by the echo-on traces in the task.

Old-vs-fix: OTGW_BUILD_BAT=<path to another build.bat>.
Run: python tests/test_build_bat_call_probes.py
"""
import os
import re
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
BAT = Path(os.environ.get("OTGW_BUILD_BAT") or HERE.parent / "build.bat")
LABELS = (":find_python", ":bootstrap_build_venv")
# The first word of a command line that starts an interpreter (or a shim with that name).
INTERPRETER = re.compile(r"^(py|python|python3|%BASE_PYTHON%)(\s|$)", re.I)


def label_body(lines, label):
    start = next(i for i, l in enumerate(lines) if l.strip().lower() == label.lower())
    body = []
    for l in lines[start + 1:]:
        if re.match(r"^:\w", l.strip()):
            break
        body.append(l)
    return body


class CallProbes(unittest.TestCase):
    def setUp(self):
        self.lines = BAT.read_text(encoding="utf-8").splitlines()

    def test_every_probe_runs_through_call(self):
        bad = []
        for label in LABELS:
            for l in label_body(self.lines, label):
                cmd = l.strip()
                if cmd.lower().startswith("rem") or not cmd:
                    continue
                if INTERPRETER.match(cmd):
                    bad.append(f"{label}: {cmd[:80]}")
        self.assertEqual(bad, [], "interpreter probes without CALL:\n  " + "\n  ".join(bad))

    def test_the_probes_are_still_there(self):
        # The check above passes trivially when the probes move or disappear; keep it honest.
        body = "\n".join(label_body(self.lines, ":find_python"))
        for probe in ("py -3 -c", "python -c", "python3 -c"):
            self.assertIn(probe, body)


if __name__ == "__main__":
    unittest.main(verbosity=2)
