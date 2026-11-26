#!/usr/bin/env python3
"""Write the three replay fixtures for the TASK-3099 bench: a hot-water tap at the end of the SAT PWM ON phase.

Entry 499 (Sergeant D): after a hot-water draw inside a PWM ON phase the flow stored for the OFF phase stays the flow of the
last tick before the draw, so a tap that heated the boiler flow does not lengthen the pause. These replays give a boiler whose flow
and flame the SAT sees like a real one (same frame style as gen_sat_pwm_replay.py: T/R/B/A lines, 750 ms per line, 12 s per cycle).

  A  tap with the flame staying on: flow 50, from TAP_START the DHW-mode bit is set for TAP_LEN s and the flow rises to 60,
     then the flow falls back from 60 to 50 over RECOVER s.
  B  tap, then the hot-water flame goes out: same tap, but from TAP_END the flame bit is off for B_FLAME_OFF s while the flow
     cools 60 -> 54 (the post-draw wait, entry 103 A), then the flame relights with flow 54 falling back to 50.
  C  control: no tap, flow 50, flame on.

Each period (default 1200 s = one 3 cycles/h PWM cycle) repeats the pattern, REPEATS times. The offsets are relative to the start of
a period; the SAT ON phase does not start at a fixed second of an open-loop replay, so the run recipe tells how to find the cycles in
which the tap fell near the end of an ON phase (pwm_phases age_s/len_s). The host test (tests/test_sat_pwm_tap_replay.py) starts the
PWM ON phase at second 0 and feeds one period to the real PWM block.

Usage: python scripts/gen_sat_pwm_tap_replay.py [out_dir] [--repeats N]
Default out_dir: tests/fixtures/sat_pwm_tap_3099 (A.txt, B.txt, C.txt, SHA256SUMS)
"""
import argparse
import hashlib
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gen_sat_pwm_replay as base  # noqa: E402  (frame(), exchange(), f88())

DEFAULT_OUT = Path(__file__).resolve().parent.parent / "tests" / "fixtures" / "sat_pwm_tap_3099"
CYCLE_S = 12           # four exchanges of 3 s (750 ms per line): MsgID 0, 25, 3 and 28; one state per cycle, so replay time = state time
PERIOD_S = 1200
BASE_FLOW = 50.0
TAP_FLOW = 60.0
TAP_START = 192        # seconds into a period (the ON phase of 264 s ends at about 270)
TAP_LEN = 60           # DHW-mode bit set for 60 s (192..252)
RECOVER_S = 150        # flow 60 -> 50 after the tap (scenario A)
B_FLAME_OFF = 96       # scenario B: flame out for 96 s after the tap (252..348)
B_RELIGHT_FLOW = 54.0
STATUS_MASTER = 0x03   # CH enable + DHW enable
SLAVE_CH_FLAME = 0x0A  # CH active + flame
SLAVE_DHW_FLAME = 0x0C  # DHW mode + flame
SLAVE_IDLE = 0x02      # CH active, flame out (hot-water flame out, wait)
SLAVE_CONFIG = 0x0100  # MsgID 3 read-ack: DHW present, modulating (typical boiler; the SAT needs a valid MsgID 3 reply)


def state_at(scenario, t):
    """(slave status byte, flow) at second t of a period."""
    tap_end = TAP_START + TAP_LEN
    if scenario == "C" or t < TAP_START:
        return SLAVE_CH_FLAME, BASE_FLOW
    if t < tap_end:
        ramp = min(1.0, (t - TAP_START) / 12.0)   # flow climbs to 60 within 12 s of the draw start
        return SLAVE_DHW_FLAME, BASE_FLOW + (TAP_FLOW - BASE_FLOW) * ramp
    if scenario == "A":
        back = min(1.0, (t - tap_end) / RECOVER_S)
        return SLAVE_CH_FLAME, TAP_FLOW - (TAP_FLOW - BASE_FLOW) * back
    off_end = tap_end + B_FLAME_OFF   # scenario B
    if t < off_end:
        cool = (t - tap_end) / B_FLAME_OFF
        return SLAVE_IDLE, TAP_FLOW - (TAP_FLOW - B_RELIGHT_FLOW) * cool
    back = min(1.0, (t - off_end) / RECOVER_S)
    return SLAVE_CH_FLAME, B_RELIGHT_FLOW - (B_RELIGHT_FLOW - BASE_FLOW) * back


def build(scenario, repeats=2):
    lines = []
    for r in range(repeats):
        for t in range(0, PERIOD_S, CYCLE_S):
            slave, flow = state_at(scenario, t)
            lines += base.exchange(0, (STATUS_MASTER << 8) | slave)
            lines += base.exchange(25, base.f88(round(flow, 1)))
            lines += base.exchange(3, SLAVE_CONFIG)
            lines += base.exchange(28, base.f88(round(flow, 1) - base.RETURN_DELTA))
    return lines


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out_dir", nargs="?", default=str(DEFAULT_OUT))
    ap.add_argument("--repeats", type=int, default=2)
    a = ap.parse_args()
    out = Path(a.out_dir)
    out.mkdir(parents=True, exist_ok=True)
    sums = []
    for sc in "ABC":
        text = "\n".join(build(sc, a.repeats)) + "\n"
        p = out / f"{sc}.txt"
        p.write_bytes(text.encode("ascii"))
        sums.append(f"{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}")
        print("wrote %s (%d lines, %d s of replay)" % (p, text.count("\n"), a.repeats * PERIOD_S))
    (out / "SHA256SUMS").write_bytes(("\n".join(sums) + "\n").encode("ascii"))


if __name__ == "__main__":
    main()
