#!/usr/bin/env python3
"""Compares a decoded debug.log with a telnet capture of the same gateway (bench check 4, TASK-2901).

    python3 compare_telnet.py out/debug.log telnet-capture.txt

The recording starts and ends at other moments than the capture, so the script finds the stretch both have: from
the first line of debug.log that is also in the capture to the last one. Inside that stretch every line of the
capture must be in debug.log, in the same order, byte for byte (CR LF and LF are treated alike). It prints how many
lines matched and the first differences. Telnet itself can drop lines when its send buffer is full (the firmware's
512 B ring); such a line is in debug.log but not in the capture and is reported as "only in the recording", which
is not a recorder fault. A line that is in the capture but not in the recording, or that differs, is one.
"""
import sys


def lines(path):
    data = open(path, "rb").read().replace(b"\r\n", b"\n")
    return [l for l in data.split(b"\n") if l.strip()]


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    rec, cap = lines(sys.argv[1]), lines(sys.argv[2])
    capset = set(cap)
    first = next((i for i, l in enumerate(rec) if l in capset), None)
    last = next((i for i in range(len(rec) - 1, -1, -1) if rec[i] in capset), None)
    if first is None:
        sys.exit("no line of the recording is in the capture: other gateway, other time, or no debug output")
    rec = rec[first:last + 1]
    c0, c1 = cap.index(rec[0]), len(cap) - 1 - cap[::-1].index(rec[-1])
    cap = cap[c0:c1 + 1]
    i = j = matched = only_rec = 0
    problems = []
    while i < len(rec) and j < len(cap):
        if rec[i] == cap[j]:
            matched += 1; i += 1; j += 1
        elif cap[j] in rec[i:i + 50]:                 # telnet dropped some lines the recorder kept
            k = rec.index(cap[j], i)
            only_rec += k - i; i = k
        else:
            problems.append((j, cap[j][:160], rec[i][:160] if i < len(rec) else b""))
            j += 1
    problems += [(jj, cap[jj][:160], b"") for jj in range(j, len(cap))]
    print(f"common stretch: {len(cap)} capture lines, {len(rec)} recording lines")
    print(f"identical: {matched}   only in the recording (telnet dropped them): {only_rec}   "
          f"in the capture but missing or different in the recording: {len(problems)}")
    for jj, c, r in problems[:10]:
        print(f"  capture line {jj + c0 + 1}: {c!r}\n    recording : {r!r}")
    sys.exit(1 if problems else 0)


if __name__ == "__main__":
    main()
