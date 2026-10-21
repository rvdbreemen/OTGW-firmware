#!/usr/bin/env python3
"""Write a /simulate replay fixture that keeps a boiler burning with a slowly rising flow.

Purpose (TASK-1277): drive SAT's PWM ON phase on the bench without a boiler, so the
modulation-suppression step (CS = flow - fModSupOffset after fModSupDelay) and its 3 s
refresh (TASK-1268) can be measured. The SAT simulation heats the flow within seconds,
so a burn there never outlasts the 20 s hold; this replay does.

Each frame is written as the four monitor lines T/R/B/A (thermostat request, gateway
request, boiler answer, answer to the thermostat), like the other replay fixtures. The
replay plays 750 ms per line, so one frame takes 3 s.

Per cycle (2 frames = 6 s): MsgID 0 status (CH and DHW enabled, CH active + flame) and
MsgID 25 flow temperature, rising FLOW_STEP C per cycle. Every 4th cycle also MsgID 28
return temperature (flow - 10) and MsgID 17 relative modulation (100 %).

Usage: python scripts/gen_sat_pwm_replay.py [out_path]
Default out: tests/webui/fixtures/sat-pwm-suppression-replay.txt
"""
import sys
from pathlib import Path

DEFAULT_OUT = Path(__file__).resolve().parent.parent / "tests" / "webui" / "fixtures" / "sat-pwm-suppression-replay.txt"

CYCLES = 60          # 60 x 6 s = 6 minutes of burning
FLOW_START = 40.0    # C
FLOW_STEP = 0.3      # C per cycle
RETURN_DELTA = 10.0  # return = flow - 10
STATUS_MASTER = 0x03  # CH enable + DHW enable
STATUS_SLAVE = 0x0A   # CH active + flame on

READ_DATA = 0
READ_ACK = 4


def with_parity(frame: int) -> int:
    """Set bit 31 so the 32-bit frame has even parity (OpenTherm)."""
    return frame | (1 << 31) if bin(frame).count("1") % 2 else frame


def frame(msg_type: int, msg_id: int, value: int) -> str:
    return "%08X" % with_parity((msg_type << 28) | (msg_id << 16) | (value & 0xFFFF))


def f88(celsius: float) -> int:
    return int(round(celsius * 256)) & 0xFFFF


def exchange(msg_id: int, ack_value: int) -> list:
    req = frame(READ_DATA, msg_id, 0)
    ack = frame(READ_ACK, msg_id, ack_value)
    return ["T" + req, "R" + req, "B" + ack, "A" + ack]


def build() -> list:
    lines = []
    for c in range(CYCLES):
        flow = FLOW_START + c * FLOW_STEP
        lines += exchange(0, (STATUS_MASTER << 8) | STATUS_SLAVE)
        lines += exchange(25, f88(flow))
        if c % 4 == 0:
            lines += exchange(28, f88(flow - RETURN_DELTA))
            lines += exchange(17, f88(100.0))
    return lines


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_OUT
    out.write_text("\n".join(build()) + "\n", encoding="ascii")
    print("wrote %s (%d lines)" % (out, len(build())))


if __name__ == "__main__":
    main()
