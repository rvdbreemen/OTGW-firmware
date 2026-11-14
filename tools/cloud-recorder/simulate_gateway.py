#!/usr/bin/env python3
"""Uploads a generated OT stream to the collection point the way a gateway would (TASK-1938 test tool).

    python3 simulate_gateway.py <base> <device-email> <password> --minutes 30 --batch 300 \
        [--start 2026-11-14T08:00:00+01:00] [--skip-seq 3] [--frames-out ref.tsv]

The frames are the codec study's generated stream, shifted to --start. --skip-seq leaves one batch out,
so the export must mark a gap. --frames-out writes the reference (epoch_ms, letter, frame) of everything
that was sent, for the round-trip check. A 429 (rate limit) is waited out and retried.
"""
import argparse, base64, datetime, json, os, sys, time, urllib.error, urllib.request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import otrec  # noqa: E402


def call(base, path, body, token=None):
    req = urllib.request.Request(base + path, data=json.dumps(body).encode(), method="POST")
    req.add_header("Content-Type", "application/json")
    if token:
        req.add_header("Authorization", token)
    try:
        with urllib.request.urlopen(req, timeout=30) as r:
            return r.status, json.loads(r.read() or b"{}")
    except urllib.error.HTTPError as e:
        return e.code, json.loads(e.read() or b"{}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("base")
    ap.add_argument("email")
    ap.add_argument("password")
    ap.add_argument("--minutes", type=float, default=30)
    ap.add_argument("--batch", type=int, default=300, help="seconds per batch")
    ap.add_argument("--start", default="2026-11-14T08:00:00+01:00")
    ap.add_argument("--skip-seq", type=int, action="append", default=[])
    ap.add_argument("--frames-out")
    a = ap.parse_args()
    base = a.base.rstrip("/")
    st, auth = call(base, "/api/collections/devices/auth-with-password", {"identity": a.email, "password": a.password})
    if st != 200:
        sys.exit(f"login failed: HTTP {st}")
    token, dev = auth["token"], auth["record"]["id"]

    t_start = int(datetime.datetime.fromisoformat(a.start).timestamp() * 1000)
    frames = [(t + t_start, L, f) for t, L, f in otrec.codec.synthetic(a.minutes / 60)]
    sent, nbytes, ref = 0, 0, []
    for seq, batch in enumerate(otrec.split_batches(frames, a.batch)):
        if seq in a.skip_seq:
            continue
        t0, payload = otrec.pack_ot(batch)
        body = {"device": dev, "stream": "ot", "seq": seq, "t0": t0, "fmt": otrec.FMT_OT_V1,
                "payload": base64.b64encode(payload).decode()}
        while True:
            st, r = call(base, "/api/collections/batches/records", body, token)
            if st != 429:
                break
            time.sleep(5)
        if st != 200:
            sys.exit(f"batch {seq}: HTTP {st} {r}")
        sent += 1
        nbytes += len(payload)
        ref += batch
    print(f"sent {sent} batches, {len(ref)} frames, {nbytes} B payload ({nbytes / max(1, len(ref)):.2f} B/frame)")
    if a.frames_out:
        with open(a.frames_out, "w") as f:
            for t, L, fr in ref:
                f.write(f"{t}\t{L}\t{fr:08X}\n")


if __name__ == "__main__":
    main()
