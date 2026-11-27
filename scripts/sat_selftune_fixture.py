#!/usr/bin/env python3
"""Build an anonymised replay fixture for the SAT self-tune regression gate (TASK-3106, P4) from a capture.

The fixture is the per-tick input stream of scripts/sat_selftune_replay.py plus what the capture itself recorded per hour, gzipped:
  # name ...        free text, no host data
  C <coeff>         the coefficient the capture ran with
  R <idx> <counted> <reason>   the capture's own hour record (sattuneh.csv): counted flag and first-seen reason
  T ...             one control tick: sun valid, sun elevation, pwm mode, pwm gas, ch, flame, room lost, diag event, dhw,
                    relative modulation, minimum modulation, signal, error, satActive (numbers only)
  M <reason id>     whole-hour marker
  E <idx>           hour end (the hour index; no time of day, no date)
No host names, addresses, MAC, SSID, names, coordinates or clock times are written: the stream has numbers only.

Usage: python scripts/sat_selftune_fixture.py <capture dir> <name> <coeff> <out dir>
"""
import gzip
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sat_selftune_replay as R


def build(capture, name, coeff):
    ticks, hours, frames = R.gather(capture)
    stream = R.build_stream(ticks, hours, frames).splitlines()
    out = ["# self-tune replay fixture '%s' (anonymised: numbers only)" % name, "C %.2f" % coeff]
    for i, (_, hr) in enumerate(hours):
        out.append("R %d %s %s" % (i, hr["counted"], hr["reason"]))
    idx = 0
    for ln in stream:
        if ln.startswith("E "):
            out.append("E %d" % idx)
            idx += 1
        else:
            out.append(ln)
    return ("\n".join(out) + "\n").encode("ascii")


def main():
    cap, name, coeff, outdir = sys.argv[1], sys.argv[2], float(sys.argv[3]), Path(sys.argv[4])
    outdir.mkdir(parents=True, exist_ok=True)
    data = build(cap, name, coeff)
    f = outdir / ("%s.txt.gz" % name)
    with open(f, "wb") as raw:
        with gzip.GzipFile(filename="", mode="wb", fileobj=raw, mtime=0) as gz:   # no file name, no time stamp
            gz.write(data)
    print("%s: %d bytes raw, %d bytes gz" % (f, len(data), f.stat().st_size))


if __name__ == "__main__":
    main()
