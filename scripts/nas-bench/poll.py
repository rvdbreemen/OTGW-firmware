"""REST poller for an OTGW unit (Python 3.8+, stdlib only). One JSON line per sample.
kind=info   -> /api/v2/device/info heap/boot fields -> <out>/info.jsonl
kind=health -> /api/v2/health (full)                -> <out>/health.jsonl
kind=snap   -> device/info + health + reboot_log + crashlog status (whole) -> <out>/snap.jsonl (for long soaks, e.g. every 3600 s)
Usage: poll.py <kind> <ip> <outdir> <minutes> <interval_s>"""
import json, os, sys, time, urllib.request, urllib.error
KIND, IP, OUT = sys.argv[1], sys.argv[2], sys.argv[3]
END, IV = time.time() + float(sys.argv[4]) * 60, float(sys.argv[5])
os.makedirs(OUT, exist_ok=True)
def get(path, raw=False):
    try:
        with urllib.request.urlopen("http://%s%s" % (IP, path), timeout=5) as r:
            b = r.read().decode("utf8", "replace"); return r.status, (b if raw else json.loads(b))
    except urllib.error.HTTPError as e: return e.code, None
    except Exception as e: return type(e).__name__, None
out = open(os.path.join(OUT, KIND + ".jsonl"), "a")
while time.time() < END:
    t0 = time.time(); rec = {"t": time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(t0)) + ("%.3f" % (t0 % 1))[1:]}
    if KIND in ("info", "snap"):
        st, j = get("/api/v2/device/info"); rec["info_code"] = st
        if isinstance(j, dict):
            d = j.get("device", j)
            rec.update(d if KIND == "snap" else {k: v for k, v in d.items() if k in ("bootcount", "uptime", "mqttconnected", "fwversion") or k.startswith("hd_") or "heap" in k.lower() or "blk" in k.lower() or "block" in k.lower()})
    if KIND in ("health", "snap"):
        st, j = get("/api/v2/health"); rec["health_code"] = st; rec["health"] = j
    if KIND == "snap":
        st, b = get("/reboot_log.txt", raw=True); rec["reboot_log_head"] = (b or "").splitlines()[:3]
        st, _ = get("/crashlog.log", raw=True); rec["crashlog_code"] = st
    out.write(json.dumps(rec) + "\n"); out.flush()
    time.sleep(max(0, IV - (time.time() - t0)))
