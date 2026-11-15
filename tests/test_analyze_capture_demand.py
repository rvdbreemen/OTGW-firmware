#!/usr/bin/env python3
"""TASK-1987: the analyze-capture skill counts a heat request from the master, not from the boiler's pump overrun.

gruijter's boiler runs its pump 10 minutes after every burn; its CH-mode status bit stays on meanwhile. The old
capture_summary.py counted that as 'demand without flame' (about 600 s per run), while SAT had no CH request on.
The request is the master status CH-enable bit (MsgID 0 HB bit 0) as the boiler receives it: the gateway's R frame
when it rewrites the request, else the thermostat's T frame.

    python tests/test_analyze_capture_demand.py
"""
import datetime as dt
import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / ".claude" / "skills" / "analyze-capture" / "scripts"))
import capture_summary as cs  # noqa: E402

T0 = dt.datetime(2026, 11, 15, 7, 0, 0)
READ_DATA, READ_ACK = 0, 4


def frames(gateway_rewrites=False):
    """Every 10 s one MsgID 0 exchange. Run 1: request at 0 s, flame 20 s to 300 s. Then the request ends and the
    boiler reports CH mode (overrun) until the next request at 1200 s, flame 1220 s to 1500 s."""
    out = []
    for s in range(0, 1800, 10):
        t = T0 + dt.timedelta(seconds=s)
        ask = 0 <= s < 300 or 1200 <= s < 1500
        flame = 20 <= s < 300 or 1220 <= s < 1500
        ch_mode = flame or 300 <= s < 1200 or ask   # the overrun runs straight into the next request (gruijter)
        lb = (0x02 if ch_mode else 0) | (0x08 if flame else 0)
        if gateway_rewrites:   # the thermostat never asks; the gateway (SAT) sets CH enable in its R frame
            out.append((t, "T", READ_DATA, 0, 0x00, 0))
            out.append((t + dt.timedelta(milliseconds=300), "R", READ_DATA, 0, 0x01 if ask else 0x00, 0))
        else:
            out.append((t, "T", READ_DATA, 0, 0x01 if ask else 0x00, 0))
        out.append((t + dt.timedelta(milliseconds=600), "B", READ_ACK, 0, 0, lb))
    return out


class DemandIsTheMasterRequest(unittest.TestCase):
    def check(self, fr):
        runs = cs.build_runs(fr)
        self.assertEqual(len(runs), 2)
        second = runs[1]
        self.assertEqual(second["kind"], "CH")
        self.assertLessEqual(second["demand_without_flame_s"], 30)       # the request 20 s before ignition
        self.assertGreaterEqual(runs[0]["overrun_after_s"], 590)         # the 10 minute pump overrun after run 1
        kinds = {k for _, _, k in cs.demand_spans(fr)}
        self.assertEqual(kinds, {"demand", "overrun"})

    def test_thermostat_request(self):
        self.check(frames())

    def test_gateway_rewritten_request_counts(self):
        self.check(frames(gateway_rewrites=True))


if __name__ == "__main__":
    unittest.main(verbosity=2)
