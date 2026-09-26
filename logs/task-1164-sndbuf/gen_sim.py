import json, sys
def f88(x):
    v = int(round(x * 256)) & 0xFFFF
    return v
def frame(kind, mtype, mid, val):
    w = (mtype << 28) | (mid << 16) | (val & 0xFFFF)
    if bin(w).count("1") % 2: w |= 1 << 31
    return "%s%08X" % (kind, w)
lines, expected = [], []
status_lb = [0x00, 0x0A, 0x02, 0x0E, 0x08, 0x0B, 0x03, 0x0A]
k = 0
def status(i, j):
    global k
    lb = status_lb[(i * 5 + j) % len(status_lb)]
    lines.append(frame("T", 0, 0, 0x0300)); lines.append(frame("B", 4, 0, 0x0300 | lb))
    expected.append({"id": 0, "lb": lb})
def rd(mid, x):
    v = f88(x); lines.append(frame("T", 0, mid, 0)); lines.append(frame("B", 4, mid, v)); expected.append({"id": mid, "val": round(x, 2)})
def wr(mid, x):
    v = f88(x); lines.append(frame("T", 1, mid, v)); lines.append(frame("B", 5, mid, v)); expected.append({"id": mid, "val": round(x, 2)})
for i in range(40):
    status(i, 0); wr(1, 40 + i * 0.5); rd(25, 45 + i * 0.25)
    status(i, 1); rd(28, 35 + i * 0.25); rd(17, (i * 2) % 100)
    status(i, 2); rd(18, 1.5 + (i % 10) * 0.01); rd(26, 50 + i * 0.1)
    status(i, 3); wr(24, 20 + i * 0.05); wr(16, 20.5 + (i % 4) * 0.5)
    status(i, 4); rd(27, 10 - i * 0.1); rd(56, 55 + (i % 3)); rd(57, 80 - (i % 5))
open(sys.argv[1], "w", newline="\n").write("\n".join(lines) + "\n")
json.dump(expected, open(sys.argv[2], "w"))
print(len(lines), "lines,", len(lines) * 0.75 / 60, "min per loop")
