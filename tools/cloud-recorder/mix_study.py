#!/usr/bin/env python3
"""One record per fixed interval with all three streams ("mix", fmt 4), per board profile (TASK-1938).

    python3 mix_study.py --work /tmp/mix [--image otgw-pb-test]

Builds one hour of mixed data the way a gateway would: OT frames (codec study stream), a SAT tick every 38 s
(16 B, a stand-in until the SAT encoder exists) and debug address tokens (dbg_size_study.py), all written into
one record buffer. The buffer is sent at every interval boundary, or earlier when the next item would not fit.
Each profile is uploaded to a fresh local PocketBase (batch_size_study.py) and measured; the OT section of
every record is decoded again and compared with the frames that went in.
"""
import argparse, base64, sys, os, zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import otrec  # noqa: E402
import dbg_size_study  # noqa: E402
import batch_size_study as bss  # noqa: E402

T_START = 1763103600000
PROFILES = [
    # name, interval s, buffer bytes, debug text rate B/s, compress debug
    ("PSRAM, full debug, zlib", 60, otrec.MAX_PAYLOAD_BYTES, 3000, True),
    ("no PSRAM, debug 0.3 KB/s", 60, otrec.MIX_BUFFER_NO_PSRAM, 300, False),
    ("no PSRAM, full debug", 60, otrec.MIX_BUFFER_NO_PSRAM, 3000, False),
]


def build(interval, cap, dbg_rate, compress):
    frames = [(t + T_START, L, f) for t, L, f in otrec.codec.synthetic(1.0)]
    dbg = [(T_START + t0, k) for t0, _, k in dbg_size_study.generate(dbg_rate, 3600, 1)[0]]
    sat = [(T_START + s * 38000, bytes(16)) for s in range(3600 // 38 + 1)]
    events = sorted([(t, 0, fr) for fr in frames for t in [fr[0]]] + [(t, 1, b) for t, b in sat]
                    + [(t, 2, b) for t, b in dbg], key=lambda e: (e[0], e[1]))
    records, cur, size = [], None, 0
    early = 0

    def flush():
        nonlocal cur, size
        if cur and (cur["ot"] or cur["sat"] or cur["dbg"]):
            d = bytes(cur["dbg"])
            payload = otrec.pack_mix(cur["t0"], cur["ot"], bytes(cur["sat"]),
                                     zlib.compress(d, 6) if (compress and d) else d, compress and bool(d))
            records.append((cur["t0"], payload, list(cur["ot"])))
        cur, size = None, 0

    for t, kind, item in events:
        if cur and t - cur["start"] >= interval * 1000:
            flush()
        add = 3 if kind == 0 else len(item)          # an OT frame costs about 1-3 B in the coded section
        if cur and size + add > cap:
            flush()
            early += 1
        if cur is None:
            cur = {"start": (t - T_START) // (interval * 1000) * interval * 1000 + T_START, "t0": t,
                   "ot": [], "sat": bytearray(), "dbg": bytearray()}
            size = 16
        if kind == 0:
            cur["ot"].append(item)
        elif kind == 1:
            cur["sat"] += item
        else:
            cur["dbg"] += item
        size += add
    flush()
    return records, frames, early


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--image", default="otgw-pb-test")
    ap.add_argument("--work", required=True)
    ap.add_argument("--port", type=int, default=8091)
    a = ap.parse_args()
    os.makedirs(a.work, exist_ok=True)
    empty = bss.run_variant(a.image, a.work, a.port, "empty", [], 1)["db"]
    print(f"{'profile':28s} {'records/h':>9s} {'early':>5s} {'max rec B':>9s} {'sent B':>9s} {'recv B':>7s} "
          f"{'db growth B':>11s} {'OT identical':>12s}")
    for name, interval, cap, rate, compress in PROFILES:
        records, frames, early = build(interval, cap, rate, compress)
        decoded = []
        for t0, payload, _ in records:
            decoded += otrec.unpack_mix(t0, payload).get(otrec.SEC_OT, [])
        recs = [("mix", t0, otrec.FMT_MIX_V1, base64.b64encode(p).decode()) for t0, p, _ in records]
        r = bss.run_variant(a.image, a.work, a.port, name.split(",")[0].replace(" ", "") + str(rate), recs, 1)
        print(f"{name:28s} {len(records):9d} {early:5d} {max(len(p) for _, p, _ in records):9d} {r['sent']:9d} "
              f"{r['recv']:7d} {r['db'] - empty:11d} {str(decoded == frames):>12s}")


if __name__ == "__main__":
    main()
