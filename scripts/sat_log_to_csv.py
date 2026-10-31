#!/usr/bin/env python3
"""TASK-1521: turn the SAT telnet lines of a capture into CSV for analysis and simulation.

The firmware logs (telnet debug key '5', on by default):
  SATTICK room=.. target=.. out=.. curve=.. p=.. i=.. d=.. pid=.. cs=.. mm=.. ch=.. mode=.. pwm=.. duty=.. flame=.. flow=.. ret=..
  SATBOOT phase=decide|takeover ms=.. ...; SATMODE from= to=; SATTC push= | refused=; SATFOLLOW recorded=;
  SATDHW start|end (event column)
Each line starts with the device time "HH:MM:SS.uuuuuu". Output: one CSV per record type, columns = "time" plus the
keys in the order of their first appearance. A capture transcript (capture-otgw / capture-mqtt-debug) works as input.

Run: python scripts/sat_log_to_csv.py <transcript.txt> [--out-dir DIR]
"""
import argparse
import csv
import re
import sys
from pathlib import Path

KINDS = ("SATTICK", "SATBOOT", "SATMODE", "SATTC", "SATFOLLOW", "SATDHW")
LINE = re.compile(r"^(\d\d:\d\d:\d\d\.\d+) .*?\b(" + "|".join(KINDS) + r") (.*)$")
KV = re.compile(r"(\w+)=(\S+)")


def parse(lines):
    """-> {kind: [row, ...]} for every kind in KINDS, each row a dict with "time" first."""
    out = {k: [] for k in KINDS}
    for raw in lines:
        m = LINE.match(raw.rstrip("\r\n"))
        if not m:
            continue
        row = {"time": m.group(1)}
        kv = KV.findall(m.group(3))
        row.update(kv if kv else [("event", m.group(3).strip())])
        out[m.group(2)].append(row)
    return out


def write_csv(rows, path):
    keys = []
    for r in rows:
        for k in r:
            if k not in keys:
                keys.append(k)
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=keys)
        w.writeheader()
        w.writerows(rows)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("transcript")
    ap.add_argument("--out-dir", default=".")
    a = ap.parse_args(argv)
    with open(a.transcript, encoding="utf-8", errors="replace") as f:
        recs = parse(f)
    out = Path(a.out_dir)
    out.mkdir(parents=True, exist_ok=True)
    stem = Path(a.transcript).stem
    for kind, rows in recs.items():
        if rows:
            p = out / f"{stem}-{kind.lower()}.csv"
            write_csv(rows, p)
            print(f"{kind}: {len(rows)} rows -> {p}")
        else:
            print(f"{kind}: no lines")
    return 0 if recs["SATTICK"] else 1


if __name__ == "__main__":
    sys.exit(main())
