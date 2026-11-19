#!/usr/bin/env python3
"""Checks that a remote telemetry download holds a replayed capture losslessly (TASK-2901 AC #6).

    python3 compare_replay.py <replay-input.log> <downloaded otframes.log> [--report out.md]

<replay-input.log> is the file the gateway replayed as /otgw_simulation.log (otframes.log lines, 3 or 4 columns);
<downloaded otframes.log> is what decode_all.py wrote from the collection point. The replay starts again at the top
when its file ends, so the download may hold the input more than once: every complete pass found in the download is
checked frame by frame (source letter + frame, in order) and its timing against the recorded gaps.

Exit 0 when at least one complete pass matches exactly, 1 otherwise. Standard library only.
"""
import argparse
import sys
from datetime import datetime


def frames(path):
    """(time_s or None, letter, frame) per data line; '#' lines are notes. 3- or 4-column otframes.log layout."""
    out = []
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.rstrip("\r\n")
            if not line or line.startswith("#"):
                continue
            cols = line.split("\t")
            if len(cols) < 3:
                continue
            dev, letter, frame = cols[-3], cols[-2], cols[-1].upper()
            t = None
            try:
                hh, mm, rest = dev.split(":")
                t = int(hh) * 3600 + int(mm) * 60 + float(rest)
            except ValueError:
                pass
            out.append((t, letter, frame))
    return out


def gaps(seq):
    return [b[0] - a[0] for a, b in zip(seq, seq[1:]) if a[0] is not None and b[0] is not None]


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("replay")
    ap.add_argument("download")
    ap.add_argument("--report")
    a = ap.parse_args(argv)
    src, got = frames(a.replay), frames(a.download)
    key_src = [(l, f) for _, l, f in src]
    key_got = [(l, f) for _, l, f in got]
    n = len(key_src)
    lines = [f"# Replay vs remote telemetry download ({datetime.now():%Y-%m-%d %H:%M})", "",
             f"- replay input: `{a.replay}`: {n} frames",
             f"- download: `{a.download}`: {len(key_got)} frames", ""]
    # every start of the input's first frame in the download is a candidate pass
    starts = [i for i in range(len(key_got) - n + 1) if key_got[i] == key_src[0] and key_got[i:i + n] == key_src]
    ok = bool(starts)
    if ok:
        for k, i in enumerate(starts, 1):
            g_src, g_got = gaps(src), gaps(got[i:i + n])
            dev = [abs(x - y) for x, y in zip(g_src, g_got)]
            worst = max(dev) if dev else 0.0
            late = sum(1 for d in dev if d > 1.0)
            lines.append(f"- pass {k}: download frames {i}..{i + n - 1}: all {n} frames equal and in order; "
                         f"gap deviation max {worst:.3f} s, {late} gaps off by more than 1 s")
    else:
        # longest prefix match, to show where it breaks
        best, at = 0, -1
        for i in range(len(key_got)):
            m = 0
            while m < n and i + m < len(key_got) and key_got[i + m] == key_src[m]:
                m += 1
            if m > best:
                best, at = m, i
        lines.append(f"- NO complete pass. Longest run from the top of the input: {best} of {n} frames "
                     f"(download index {at}).")
        if 0 <= at and best < n:
            exp = key_src[best]
            seen = key_got[at + best] if at + best < len(key_got) else None
            lines.append(f"- first difference at input frame {best}: expected {exp}, got {seen}")
    text = "\n".join(lines) + "\n"
    print(text)
    if a.report:
        with open(a.report, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
