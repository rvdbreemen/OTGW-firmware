#!/usr/bin/env python3
"""TASK-1412.7 / QD-8: comparison traces of today's behaviour after a hot-water draw against the OpenTherm
application note AFS 2.2.3 recovery (master side: hold the control setpoint at the boiler temperature, restore it after 1 minute).

NOTHING IS IMPLEMENTED HERE. The hold is not in the firmware. Rows marked "today" come from the REAL satApplyContinuous()
(SATcontrol.ino, cut out by tests/test_sat_previous_cs_clamp.py and compiled on the host). Rows marked "model" feed the same real
function with the CS a one-minute hold would have left behind, to show how the existing previous-CS clamp (entry 112) would
follow it. The flow temperatures after the draw are SYNTHETIC (nothing in the repository records a flow decay after a draw);
they are inputs of the scenario, not a boiler model, and the test does not claim to know whether the boiler stops.

What the traces establish, and the tests pin:
  1. During a draw SAT sends no CS and does not move the previous CS (the DHW branch of the control loop; MM goes to the maximum).
  2. The first control tick after a draw therefore uses the CS sent BEFORE the draw as the "previous CS". With the request
     unchanged (or lower) and the flame lit, the clamp of entry 112 returns max(request, min(flow - 2, previous CS)) = previous CS:
     the CS stays at the request level, far below a flow that is still hot from the draw. The crosswalk wording "CS >= flow - 2
     with the flame on" holds only when the request rose above the previous CS (see test_rising_request...).
  3. A one-minute hold (CS = boiler temperature for the ticks inside 60 s, then the request again) would not snap back: the
     previous-CS clamp makes the CS follow flow - 2 down until it meets the request.

Control tick: 30 s (the SAT default), so "1 minute" is the ticks at +0 s and +30 s after the draw end; the tick at +60 s restores.

Run: python tests/test_sat_post_dhw_traces.py
"""
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_sat_previous_cs_clamp as pc  # noqa: E402
from test_sat_pwm_override import compile_cpp  # noqa: E402

OFFSET = 2.0   # settings.sat.fFlowOffset default


class PostDhwTraces(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.src = pc.SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        cpp = d / "t.cpp"
        cpp.write_text(pc.build_cpp(cls.src), encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / "t.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def cs(self, flame, flow, prev, req):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe), str(int(flame)), str(flow), str(OFFSET), str(prev), str(req)],
                             capture_output=True, text=True, check=True).stdout
        return float(out.strip())

    def trace_today(self, req, prev, flows, flame=True):
        """The CS of each control tick after the draw, previous CS fed forward as the firmware does."""
        rows = []
        for flow in flows:
            cs = self.cs(flame, flow, prev, req)
            rows.append((flow, cs))
            prev = cs
        return rows

    def trace_hold_model(self, req, prev, flows, hold_ticks=2):
        """MODEL, not firmware: the first hold_ticks ticks send CS = boiler temperature (AFS 2.2.3, master side), then the request
        returns through the real clamp with the held CS as the previous CS."""
        rows = []
        for i, flow in enumerate(flows):
            cs = flow if i < hold_ticks else self.cs(True, flow, prev, req)
            rows.append((flow, cs))
            prev = cs
        return rows

    @staticmethod
    def show(title, rows, req):
        print("\n%s (request %.1f)" % (title, req))
        print("  t after draw  flow   CS    CS - flow")
        for i, (flow, cs) in enumerate(rows):
            print("  +%3d s        %5.1f  %5.1f  %+5.1f" % (30 * i, flow, cs, cs - flow))

    # Synthetic scenario: the draw ends with the supply at 62 and a pump that cools it 4 K per 30 s tick.
    FLOWS = [62, 58, 54, 50, 46, 43]

    def test_first_tick_after_a_draw_keeps_the_pre_draw_cs_when_the_request_did_not_rise(self):
        rows = self.trace_today(req=41, prev=41, flows=self.FLOWS)
        self.show("today (real satApplyContinuous), flame on, request = previous CS", rows, 41)
        self.assertEqual([cs for _, cs in rows], [41.0] * 6)
        self.assertEqual(rows[0][1] - rows[0][0], -21.0, "CS is 21 K below the supply temperature on the first tick")

    def test_a_lower_request_after_the_draw_also_keeps_the_previous_cs_until_flow_minus_offset_meets_it(self):
        # The room warmed during the draw, the request fell 45 -> 41; previous CS 45.
        rows = self.trace_today(req=41, prev=45, flows=self.FLOWS)
        self.show("today, flame on, request fell 45 -> 41", rows, 41)
        self.assertEqual([cs for _, cs in rows], [45.0, 45.0, 45.0, 45.0, 44.0, 41.0])

    def test_rising_request_follows_the_supply_temperature(self):
        # The room cooled during the draw, the request rose 41 -> 48: the clamp gives max(48, flow - 2) = flow - 2 while flow is high.
        rows = self.trace_today(req=48, prev=41, flows=self.FLOWS)
        self.show("today, flame on, request rose 41 -> 48", rows, 48)
        self.assertEqual([cs for _, cs in rows], [60.0, 56.0, 52.0, 48.0, 48.0, 48.0])

    def test_flame_out_at_the_draw_end_sends_the_request(self):
        rows = self.trace_today(req=41, prev=41, flows=self.FLOWS, flame=False)
        self.show("today, flame OFF at the draw end", rows, 41)
        self.assertEqual([cs for _, cs in rows], [41.0] * 6)

    def test_hold_model_then_real_clamp_ramps_down_with_the_flow(self):
        rows = self.trace_hold_model(req=41, prev=41, flows=self.FLOWS)
        self.show("MODEL (not firmware): 60 s hold at CS = supply, then the real clamp", rows, 41)
        self.assertEqual([cs for _, cs in rows], [62.0, 58.0, 52.0, 48.0, 44.0, 41.0])

    def test_the_gap_to_the_supply_temperature_today_versus_the_hold_model(self):
        today = self.trace_today(req=41, prev=41, flows=self.FLOWS)
        hold = self.trace_hold_model(req=41, prev=41, flows=self.FLOWS)
        gap_today = [round(cs - f, 1) for f, cs in today]
        gap_hold = [round(cs - f, 1) for f, cs in hold]
        print("\nCS minus supply temperature per tick: today %s, hold model %s" % (gap_today, gap_hold))
        self.assertEqual(gap_today, [-21.0, -17.0, -13.0, -9.0, -5.0, -2.0])
        self.assertEqual(gap_hold, [0.0, 0.0, -2.0, -2.0, -2.0, -2.0])

    def test_a_draw_sends_no_cs_and_does_not_move_the_previous_cs(self):
        s = self.src
        a = s.index("  if (state.sat.bDhwActive) {\n    // DHW has priority")
        b = s.index("    return;\n  }", a)
        branch = s[a:b]
        self.assertNotIn("_sat_lastControlCS", branch)
        self.assertNotIn("satSendCS(", branch)
        self.assertIn("satSendMMDuringDhw()", branch, "MM to the maximum during the draw (entry 220)")


if __name__ == "__main__":
    unittest.main(verbosity=2)
