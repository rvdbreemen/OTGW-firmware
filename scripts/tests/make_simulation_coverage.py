#!/usr/bin/env python3
"""Generate the OT decode-coverage simulation asset.

Complements otgw_simulation.log (a realistic replay tuned for sustained heap
load, TASK-901). This one optimises for *breadth*: every MsgID the firmware
knows how to decode, every OpenTherm message type, and every source prefix.

Sources, in order of preference:
  1. Real frames harvested from the capture corpus (default: ../../../OTGW-logs).
     These carry genuine field values and genuine request/response pairing.
  2. Synthetic frames for OTmap ids that appear in no capture, because a real
     boiler only implements a subset. Values are chosen per ot_* type so the
     f8.8 / s16 / u16 / u8u8 / flag8 decoders are all exercised.

The replayer feeds every non-empty line to dispatchOTGWInputLine, so the output
must contain frames only. No comments, no blank-line padding.

Parity is not computed here on purpose: the ESP does not verify it. A parity
error is signalled by the PIC with an 'E' prefix (OTGW-Core.ino), so the asset
carries one literal E line to reach that branch.

Usage:
    python make_simulation_coverage.py [--logs DIR] [--header PATH] [--out PATH]
"""
from __future__ import annotations

import argparse
import collections
import glob
import os
import re
import sys

FRAME_RX = re.compile(r"\b([TBRAE])([0-9A-F]{8})\b")
OTMAP_RX = re.compile(r'\{\s*(\d+)\s*,\s*(OT_\w+)\s*,\s*(ot_\w+)\s*,\s*"([^"]*)"')

# One representative value per firmware type, picked so each decoder does real
# work: a negative s16, a fractional f8.8, multi-bit flag8 bytes.
TYPE_VALUE = {
    "ot_f88": "1900",         # 25.00
    "ot_s16": "FFEC",         # -20
    "ot_s8s8": "F00A",        # -16 / 10
    "ot_u16": "0064",         # 100
    "ot_u8u8": "0305",
    "ot_flag8": "0100",
    "ot_flag8flag8": "0102",
    "ot_flag8u8": "0203",
    "ot_u8": "0007",
    "ot_special": "0000",
    "ot_undef": "0000",
}

# Status(0) exchange in two variants. Alternating them keeps the bit and byte
# fan-outs firing on *change* each loop instead of only first-seen, which is
# what exercises the publish gating rather than just the parser.
STATUS_VARIANTS = [("T00000200", "B40000202"), ("T00000A00", "B40000A0A")]

MSG_READ_DATA, MSG_WRITE_DATA = 0, 1
MSG_READ_ACK, MSG_WRITE_ACK = 4, 5
MSG_DATA_INVALID, MSG_UNKNOWN_ID = 6, 7


def frame(prefix: str, msgtype: int, msgid: int, data: str) -> str:
    return f"{prefix}{msgtype << 4:02X}{msgid:02X}{data}"


def load_otmap(header_path: str) -> dict[int, tuple[str, str, str]]:
    src = open(header_path, encoding="utf-8", errors="replace").read()
    start = src.index("const OTlookup_t OTmap[] PROGMEM")
    body = src[start : src.index("};", start)]
    return {
        int(i): (cmd, typ, label) for i, cmd, typ, label in OTMAP_RX.findall(body)
    }


def harvest(logs_dir: str) -> tuple[dict[int, dict], int]:
    """Return {msgid: {(prefix, msgtype): frame}} from every capture found."""
    by_id: dict[int, dict] = collections.defaultdict(dict)
    files = 0
    for path in glob.glob(os.path.join(logs_dir, "**", "*.txt"), recursive=True):
        try:
            data = open(path, encoding="utf-8", errors="replace").read()
        except OSError:
            continue
        hits = FRAME_RX.findall(data)
        if hits:
            files += 1
        for prefix, hexs in hits:
            byte0 = int(hexs[0:2], 16)
            msgid = int(hexs[2:4], 16)
            msgtype = (byte0 >> 4) & 0x07
            by_id[msgid].setdefault((prefix, msgtype), prefix + hexs)
    return by_id, files


def build(otmap: dict, by_id: dict) -> tuple[list[str], dict]:
    out: list[str] = []
    emitted = 0

    def interleave_status() -> None:
        nonlocal emitted
        out.extend(STATUS_VARIANTS[(emitted // 6) % len(STATUS_VARIANTS)])

    real_ids, synth_ids = set(), set()

    for msgid in sorted(by_id):                      # real frames, master then slave
        frames = by_id[msgid]
        out.extend(v for (p, _), v in sorted(frames.items()) if p in "TR")
        out.extend(v for (p, _), v in sorted(frames.items()) if p in "BAE")
        real_ids.add(msgid)
        emitted += 1
        if emitted % 6 == 0:
            interleave_status()

    for msgid in sorted(otmap):                      # synthetic supplement
        if msgid in by_id:
            continue
        cmd, typ, _ = otmap[msgid]
        data = TYPE_VALUE.get(typ, "0000")
        if cmd == "OT_WRITE":
            out += [frame("T", MSG_WRITE_DATA, msgid, data),
                    frame("B", MSG_WRITE_ACK, msgid, data)]
        else:
            out += [frame("T", MSG_READ_DATA, msgid, data),
                    frame("B", MSG_READ_ACK, msgid, data)]
        synth_ids.add(msgid)
        emitted += 1
        if emitted % 6 == 0:
            interleave_status()

    # Paths neither corpus nor supplement reliably provides.
    out += [
        frame("R", MSG_READ_DATA, 120, "0000"),      # R: request-to-boiler
        frame("B", MSG_READ_ACK, 120, "05E7"),
        frame("T", MSG_WRITE_DATA, 7, "0000"),
        frame("A", MSG_UNKNOWN_ID, 7, "0000"),       # A: answer-to-thermostat
        frame("T", MSG_READ_DATA, 131, "0000"),
        frame("B", MSG_DATA_INVALID, 131, "0000"),   # Data-Invalid
        frame("T", MSG_READ_DATA, 240, "0000"),
        frame("B", MSG_UNKNOWN_ID, 240, "0000"),     # Unknown-DataId above 127
        "E10000000",                                 # parity-error prefix
    ]
    out.extend(STATUS_VARIANTS[0])

    stats = {
        "frames": len(out),
        "otmap_total": len(otmap),
        "otmap_real": len(real_ids & set(otmap)),
        "otmap_synth": len(synth_ids),
        "extra_ids": sorted(i for i in real_ids if i not in otmap),
    }
    return out, stats


def main() -> int:
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.abspath(os.path.join(here, "..", ".."))
    ap = argparse.ArgumentParser()
    ap.add_argument("--logs", default=os.path.join(repo, "..", "OTGW-logs"))
    ap.add_argument("--header", default=os.path.join(repo, "src", "OTGW-firmware", "OTGW-Core.h"))
    ap.add_argument("--out", default=os.path.join(here, "otgw_simulation_coverage.log"))
    args = ap.parse_args()

    otmap = load_otmap(args.header)
    by_id, files = harvest(args.logs)
    if not by_id:
        print(f"no frames harvested from {args.logs}; is the capture corpus present?",
              file=sys.stderr)
        return 1

    frames, stats = build(otmap, by_id)
    with open(args.out, "w", newline="\n", encoding="ascii") as fh:
        fh.write("\n".join(frames) + "\n")

    covered = stats["otmap_real"] + stats["otmap_synth"]
    print(f"captures scanned : {files}")
    print(f"frames written   : {stats['frames']}  -> {args.out}")
    print(f"loop duration    : {stats['frames'] * 0.75 / 60:.1f} min at 750ms/frame")
    print(f"OTmap coverage   : {covered}/{stats['otmap_total']} "
          f"({stats['otmap_real']} real, {stats['otmap_synth']} synthetic)")
    print(f"extra ids        : {stats['extra_ids']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
