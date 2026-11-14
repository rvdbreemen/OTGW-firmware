#!/usr/bin/env python3
"""Exports one gateway's OT stream from the OTGW collection point as a replay file (TASK-1938).

    python3 export_batches.py http://<nas>:8090 <superuser-email> <password> <device-email> out/otframes.log \
        [--since 2026-11-14T00:00] [--until ...] [--tz Europe/Amsterdam]

Run it on the LAN: the superuser API is not exposed through the tunnel.
The output uses the otframes.log layout that the replay reads (ADR-209):
    host_time TAB device_time TAB source TAB frame
host_time is the frame time in ISO-8601 with ms and the UTC offset, device_time is HH:MM:SS.uuuuuu in local time.
A missing batch (a gap in seq) becomes a "#" note line, which the replay skips.
Standard library only (plus otrec.py and the codec next to it in the repo).
"""
import argparse, base64, datetime, json, os, sys, urllib.parse, urllib.request
from zoneinfo import ZoneInfo

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import otrec  # noqa: E402

HEADER = ("# OTGW frames exported from the collection point (TASK-1938). Columns, tab separated: "
          "host_time (frame time, ISO-8601, ms, local time with UTC offset) | device_time (HH:MM:SS.uuuuuu, local) | "
          "source letter | 8 hex digits. Lines starting with # are notes.")


def api(base, path, token=None, body=None):
    req = urllib.request.Request(base + path, data=json.dumps(body).encode() if body is not None else None,
                                 method="POST" if body is not None else "GET")
    req.add_header("Content-Type", "application/json")
    if token:
        req.add_header("Authorization", token)
    with urllib.request.urlopen(req, timeout=30) as r:
        return json.loads(r.read())


def to_pb_time(s):
    """'2026-11-14T00:00' (local or with offset) -> PocketBase 'YYYY-MM-DD HH:MM:SS.sssZ' in UTC."""
    t = datetime.datetime.fromisoformat(s)
    if t.tzinfo is None:
        t = t.astimezone()
    return t.astimezone(datetime.timezone.utc).strftime("%Y-%m-%d %H:%M:%S.000Z")


def fetch_batches(base, token, device_id, stream, since=None, until=None):
    flt = f'device="{device_id}" && stream="{stream}"'
    if since:
        flt += f' && t0>={int(datetime.datetime.fromisoformat(since).astimezone().timestamp() * 1000)}'
    if until:
        flt += f' && t0<{int(datetime.datetime.fromisoformat(until).astimezone().timestamp() * 1000)}'
    page, out = 1, []
    while True:
        q = urllib.parse.urlencode({"filter": flt, "sort": "seq", "perPage": 500, "page": page,
                                    "fields": "seq,t0,fmt,payload", "skipTotal": 1})
        items = api(base, f"/api/collections/batches/records?{q}", token)["items"]
        out += items
        if len(items) < 500:
            return out
        page += 1


def write_otframes(batches, path, tz):
    frames_out = gaps = 0
    with open(path, "w", newline="\n") as f:
        f.write(HEADER + "\n")
        prev = None
        for b in batches:
            if prev is not None and b["seq"] != prev + 1:
                f.write(f"# gap: batches {prev + 1}..{b['seq'] - 1} missing\n")
                gaps += 1
            prev = b["seq"]
            if b["fmt"] != otrec.FMT_OT_V1:
                f.write(f"# batch {b['seq']}: unknown fmt {b['fmt']}, skipped\n")
                continue
            for t, letter, fr in otrec.unpack_ot(b["t0"], base64.b64decode(b["payload"])):
                ts = datetime.datetime.fromtimestamp(t / 1000, tz)
                f.write(f"{ts.isoformat(timespec='milliseconds')}\t{ts.strftime('%H:%M:%S.%f')}\t{letter}\t{fr:08X}\n")
                frames_out += 1
    return frames_out, gaps


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("base")
    ap.add_argument("admin_email")
    ap.add_argument("admin_password")
    ap.add_argument("device_email")
    ap.add_argument("out")
    ap.add_argument("--since")
    ap.add_argument("--until")
    ap.add_argument("--tz", default="Europe/Amsterdam")
    a = ap.parse_args()
    base = a.base.rstrip("/")
    token = api(base, "/api/collections/_superusers/auth-with-password", None,
                {"identity": a.admin_email, "password": a.admin_password})["token"]
    q = urllib.parse.urlencode({"filter": f'email="{a.device_email}"', "fields": "id"})
    found = api(base, f"/api/collections/devices/records?{q}", token)["items"]
    if not found:
        sys.exit(f"device {a.device_email} not found")
    batches = fetch_batches(base, token, found[0]["id"], "ot", a.since, a.until)
    n, gaps = write_otframes(batches, a.out, ZoneInfo(a.tz))
    print(f"{len(batches)} batches, {n} frames, {gaps} gaps -> {a.out}")


if __name__ == "__main__":
    main()
