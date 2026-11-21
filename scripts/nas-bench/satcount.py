"""Unique OTGW/value/<uid>/sat/* topics in mqtt.log after the N-th set/<uid>/sat/outdoor_temp (or all lines after a given text).
Usage: satcount.py <mqtt.log> <uid> <nth_set> [window_lines]   -> prints count, then the topics"""
import sys
log, uid, nth = sys.argv[1], sys.argv[2], int(sys.argv[3]); win = int(sys.argv[4]) if len(sys.argv) > 4 else 10**9
lines = open(log, encoding="utf8", errors="replace").read().splitlines()
marks = [i for i, l in enumerate(lines) if ("OTGW/set/%s/sat/outdoor_temp" % uid) in l]
if len(marks) < nth: print("set message #%d not found (%d found)" % (nth, len(marks))); sys.exit(1)
start = marks[nth - 1] + 1; end = marks[nth] if len(marks) > nth else len(lines); end = min(end, start + win)
topics = sorted({l.split()[1] for l in lines[start:end] if len(l.split()) > 1 and l.split()[1].startswith("OTGW/value/%s/sat/" % uid)})
print(len(topics)); print("\n".join(topics))
