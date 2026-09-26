import json, sys, collections
S, sub = sys.argv[1], sys.argv[2]
exp = json.load(open(S + "/expected.json"))
topic = {1: "TSet", 25: "Tboiler", 28: "Tret", 17: "RelModLevel", 18: "CHPressure", 26: "Tdhw",
         24: "Tr", 16: "TrSet", 27: "Toutside", 56: "TdhwSet", 57: "MaxTSet"}
recv = collections.defaultdict(list); blocks = collections.OrderedDict()
for l in open(S + "/" + sub, encoding="utf-8", errors="replace"):
    p = l.rstrip("\n").split(" ", 2)
    if len(p) < 3 or "homeassistant/" in p[1]: continue
    t = p[1].replace("OTGW/value/otgw-84F3EB22B8E1/", "")
    recv[t].append(p[2])
    if t.startswith(("otgw-firmware/", "otgw-pic/")) and "/stats/" not in t:
        blocks.setdefault(p[0][11:16], set()).add(t)
gaps_total = n_total = 0
for mid, name in sorted(topic.items(), key=lambda x: x[1]):
    cyc = [round(e["val"], 2) for e in exp if e["id"] == mid]          # one value per cycle, 40 cycles
    cyc = [v for i, v in enumerate(cyc) if v != cyc[i - 1]] if cyc[0] != cyc[-1] else cyc  # on-change ring
    got = [round(float(x), 2) for x in recv.get(name, [])]
    gaps = 0
    for a, b in zip(got, got[1:]):
        ia = [i for i, v in enumerate(cyc) if abs(v - a) < 0.011]
        if not any(abs(cyc[(i + 1) % len(cyc)] - b) < 0.011 for i in ia): gaps += 1
    gaps_total += gaps; n_total += len(got)
    print("%-12s received %3d  sequence gaps %d" % (name, len(got), gaps))
print("VALUE TOPICS: %d updates, %d gaps" % (n_total, gaps_total))
full = set().union(*blocks.values()) if blocks else set()
settings = sorted(t for t in full if t.startswith("otgw-pic/settings/"))
print("5-min blocks (topics received / settings topics present of %d):" % len(settings))
for m, ts in blocks.items():
    if len(ts) < 5: continue
    print("  %s  %2d topics, settings %2d/%d" % (m, len(ts), sum(1 for t in settings if t in ts), len(settings)))
