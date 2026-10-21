#!/usr/bin/env python3
"""TASK-1277: the SAT PWM suppression replay fixture must hold valid OT frames.

The firmware drops a frame with odd parity, so a bad status or flow frame would never
reach SAT. The fixture must also match its generator, keep the flame on throughout,
and carry a rising flow, so the PWM ON phase reaches the flow-1 suppression step.

Run: python tests/test_sat_pwm_replay_fixture.py
"""
import importlib.util
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
FIXTURE = REPO / "tests" / "webui" / "fixtures" / "sat-pwm-suppression-replay.txt"
GEN = REPO / "scripts" / "gen_sat_pwm_replay.py"


def load_generator():
    spec = importlib.util.spec_from_file_location("gen_sat_pwm_replay", GEN)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def frames():
    return [l.strip() for l in FIXTURE.read_text().splitlines() if l.strip()]


def acks(msg_id):
    out = []
    for f in frames():
        v = int(f[1:], 16)
        if f[0] == "B" and (v >> 16) & 0xFF == msg_id:
            out.append(v & 0xFFFF)
    return out


class SatPwmReplayFixture(unittest.TestCase):
    def test_fixture_matches_generator(self):
        self.assertEqual(frames(), load_generator().build())

    def test_every_frame_has_even_parity(self):
        odd = [f for f in frames() if bin(int(f[1:], 16)).count("1") % 2]
        self.assertEqual(odd, [])

    def test_flame_on_throughout(self):
        status = acks(0)
        self.assertTrue(status)
        self.assertTrue(all((s & 0x08) for s in status))  # slave status bit 3 = flame

    def test_burn_lasts_at_least_four_minutes(self):
        self.assertGreaterEqual(len(frames()) * 0.75, 240)  # 750 ms per line

    def test_flow_rises_and_return_below_flow(self):
        flow = [v / 256 for v in acks(25)]
        self.assertEqual(flow, sorted(flow))
        self.assertGreater(flow[-1] - flow[0], 10)
        ret = [v / 256 for v in acks(28)]
        self.assertTrue(ret and max(ret) < max(flow))


if __name__ == "__main__":
    unittest.main(verbosity=2)
