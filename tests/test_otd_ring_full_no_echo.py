#!/usr/bin/env python3
"""TASK-1804 B3e: a command whose frame the full command ring drops gives no success echo (as CS= and SW=).

MH=, MW=, M2=, RR=, RS= and the PM= one-shot read answered "XX: value" even when otCmdEnqueue() refused the frame, so a
client saw success for a frame that never went out. The PIC has no command queue that can fill (each command only sets a
latch for a later cycle: RemoteRequest gateway.asm 6.6 :5245-5250, ResetCounter :4975-4982, SetOperModeHC1/DHW/HC2
:4424-4452 / :5208-5218 with PushOperModes :3260-3267), so it has no token for this case. OT-Direct now does what CS= and SW=
already do for a refused write (TASK-1593): no echo, otdWriteRefused set, the handler returns false, which
addCommandToQueue() reports as Dropped (OTGW-Core.ino). The stored operating-mode nibble stays, as the PIC always stores it.

This is a source check on the real branches of handleOTDirectCommand() (they depend on much gateway state): in each, the
refused-enqueue path returns before the success answer is built.
Old-vs-fix: OTGW_OTD_INO=<older OTDirect.ino> python tests/test_otd_ring_full_no_echo.py
"""
import os
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
OTD_INO = Path(os.environ.get("OTGW_OTD_INO", REPO / "src" / "OTGW-firmware" / "OTDirect.ino"))
SRC = OTD_INO.read_text(encoding="utf-8").replace("\r\n", "\n")

BRANCHES = {
    "MH": "else if (cmd0 == 'M' && cmd1 == 'H') {",
    "MW": "else if (cmd0 == 'M' && cmd1 == 'W') {",
    "M2": "else if (cmd0 == 'M' && cmd1 == '2') {",
    "RR": "else if (cmd0 == 'R' && cmd1 == 'R') {",
    "RS": "else if (cmd0 == 'R' && cmd1 == 'S') {",
    "PM": "else if (cmd0 == 'P' && cmd1 == 'M') {",
}


def branch(start):
    i = SRC.index(start)
    j = SRC.index("{", i + len(start) - 1)
    depth = 0
    for k in range(j, len(SRC)):
        if SRC[k] == "{":
            depth += 1
        elif SRC[k] == "}":
            depth -= 1
            if depth == 0:
                return SRC[i:k + 1]
    raise AssertionError(start)


class RingFullNoEcho(unittest.TestCase):
    def test_a_refused_enqueue_returns_before_the_echo(self):
        for name, start in BRANCHES.items():
            body = branch(start)
            k = body.index("if (!otCmdEnqueue(")
            line = body[k:body.index("\n", k)]
            self.assertIn("otdWriteRefused = true; return false;", line, name)
            self.assertLess(k, body.index("synthesizeResponse("), name + ": the check comes before the answer")

    def test_the_operating_mode_nibble_is_still_stored(self):
        # the PIC always stores it (SetOperModeHC1/DHW/HC2); only the echo goes
        for name in ("MH", "MW", "M2"):
            body = branch(BRANCHES[name])
            self.assertLess(body.index("otOperModeWrite = true;"), body.index("if (!otCmdEnqueue("), name)


if __name__ == "__main__":
    unittest.main(verbosity=2)
