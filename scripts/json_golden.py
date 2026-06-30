#!/usr/bin/env python
"""TASK-867 regression oracle: snapshot every v2 GET JSON response, then after the
ArduinoJson migration re-fetch and compare. Gate = semantic JSON equality
(same keys/values/types) ignoring volatile runtime fields; byte diffs are
reported as informational (ArduinoJson reformats floats/whitespace/key order).

Usage:
  python scripts/json_golden.py capture <host> [outdir]
  python scripts/json_golden.py compare <host> [golddir]
"""
import sys, os, json, urllib.request, re

ENDPOINTS = [
 "/v2/health","/v2/debug","/v2/settings","/v2/sensors","/v2/sensors/status",
 "/v2/sensors/labels","/v2/device/info","/v2/device/time","/v2/device/crashlog",
 "/v2/flash/status","/v2/pic/flash-status","/v2/pic/update-check","/v2/pic/settings",
 "/v2/firmware/files","/v2/filesystem/files","/v2/filesystem/hash-check","/v2/simulate",
 "/v2/otgw/otmonitor","/v2/otgw/telegraf","/v2/discovery","/v2/otdirect/status",
 "/v2/otdirect/settings","/v2/otdirect/overrides","/v2/sat","/v2/sat/status",
 "/v2/sat/status?detail=full","/v2/sat/weather","/v2/sat/weather/needs-setup",
 "/v2/sat/ble/discovery","/v2/sat/markers","/v2/sat/sensor-areas",
]
# WiFi scan + update-check are slow / outbound; skip by default.

# Volatile keys: change run-to-run regardless of serialization. Exact names + prefixes.
VOLATILE_EXACT = {
 "uptime","bootcount","lastreset","compiled","fwversion","coreversion","sdkversion",
 "freeheap","heap","maxfreeblock","minfreeheap","hd_fragmentation_pct","sketchsize",
 "freesketchspace","wifirssi","wifiquality","wifiquality_text","time","datetime",
 "dateTime","epoch","timestamp","millis","now","flashchipmode","lastreset",
 # flash/version artifacts (app-only flash -> fw githash changes, fs unchanged):
 "fw_hash","fs_hash","match","message",
 # filesystem totals + reboot/discovery counters shift on every reboot:
 "usedBytes","freeBytes","pending_ids",
 # /v2/debug uses FLAT dotted-string keys (e.g. "runtime.heap_free"), not nested
 # dicts — these runtime-telemetry keys change every fetch, can never be golden'd
 # (TASK-955). The "runtime.heap" prefix covers heap_free/frag_pct/min_free/max_alloc.
 "runtime.uptime_sec","runtime.wifi_rssi",
}
VOLATILE_PREFIX = ("perf_","hd_","disc_","runtime.heap")

# Strings the manual JSON emitted for booleans; ArduinoJson now emits real bools.
# Normalising both sides means the intended string->bool improvement is transparent
# (a genuine true<->false value flip still fails).
_BOOLSTR = {"true": True, "false": False}

def is_volatile(k):
    return k in VOLATILE_EXACT or any(k.startswith(p) for p in VOLATILE_PREFIX)

def fetch(host, ep):
    url = "http://%s/api%s" % (host, ep)
    try:
        with urllib.request.urlopen(url, timeout=12) as r:
            return r.getcode(), r.read().decode("utf-8","replace")
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode("utf-8","replace")
    except Exception as e:
        return 0, "ERR:%s" % e

def safe_name(ep):
    return re.sub(r"[^a-zA-Z0-9]+","_",ep).strip("_")

def strip_volatile(obj):
    if isinstance(obj, dict):
        return {k: strip_volatile(v) for k,v in obj.items() if not is_volatile(k)}
    if isinstance(obj, list):
        return [strip_volatile(x) for x in obj]
    if isinstance(obj, float) and obj.is_integer():
        return int(obj)  # 20.0 == 20 for semantic compare
    if isinstance(obj, str) and obj.lower() in _BOOLSTR:
        return _BOOLSTR[obj.lower()]  # "false" (old) == false (new): intended improvement
    return obj

def capture(host, outdir):
    os.makedirs(outdir, exist_ok=True)
    idx = {}
    for ep in ENDPOINTS:
        code, body = fetch(host, ep)
        fn = safe_name(ep) + ".json"
        open(os.path.join(outdir, fn), "w", encoding="utf-8", newline="\n").write(body)
        idx[ep] = {"status": code, "bytes": len(body), "file": fn}
        print("  %-36s %s  %d bytes" % (ep, code, len(body)))
    json.dump(idx, open(os.path.join(outdir,"_index.json"),"w"), indent=2)
    print("captured %d endpoints -> %s" % (len(ENDPOINTS), outdir))

def compare(host, golddir):
    idx = json.load(open(os.path.join(golddir,"_index.json")))
    npass=nfail=nbyte=0
    for ep in ENDPOINTS:
        code, body = fetch(host, ep)
        g = idx.get(ep)
        gold = open(os.path.join(golddir, g["file"]), encoding="utf-8").read() if g else None
        if g is None:
            print("  ?? %-36s no golden" % ep); continue
        if code != g["status"]:
            print("  FAIL %-34s status %s -> %s" % (ep, g["status"], code)); nfail+=1; continue
        try:
            a=strip_volatile(json.loads(gold)); b=strip_volatile(json.loads(body))
        except Exception:
            # non-JSON (e.g. plain text) -> exact compare
            if gold==body: print("  ok   %-34s (text identical)"%ep); npass+=1
            else: print("  FAIL %-34s text differs"%ep); nfail+=1
            continue
        if a==b:
            byte = "BYTE-DIFF" if gold!=body else "byte-identical"
            if gold!=body: nbyte+=1
            print("  PASS %-34s semantic-equal  [%s]" % (ep, byte)); npass+=1
        else:
            nfail+=1
            print("  FAIL %-34s SEMANTIC DIFF:" % ep)
            for d in diff_paths(a,b)[:12]: print("         %s" % d)
    print("\n  %d PASS, %d FAIL, %d byte-only-diffs" % (npass,nfail,nbyte))
    sys.exit(1 if nfail else 0)

def diff_paths(a,b,path=""):
    out=[]
    if isinstance(a,dict) and isinstance(b,dict):
        for k in sorted(set(a)|set(b)):
            if k not in a: out.append("+%s/%s (new=%r)"%(path,k,b[k]))
            elif k not in b: out.append("-%s/%s (gold=%r)"%(path,k,a[k]))
            else: out+=diff_paths(a[k],b[k],path+"/"+k)
    elif isinstance(a,list) and isinstance(b,list):
        if len(a)!=len(b): out.append("%s len %d->%d"%(path,len(a),len(b)))
        else:
            for i,(x,y) in enumerate(zip(a,b)): out+=diff_paths(x,y,"%s[%d]"%(path,i))
    elif a!=b:
        out.append("%s: %r -> %r"%(path,a,b))
    return out

if __name__=="__main__":
    if len(sys.argv)<3: print(__doc__); sys.exit(2)
    mode,host=sys.argv[1],sys.argv[2]
    arg=sys.argv[3] if len(sys.argv)>3 else ("scripts/json-golden" )
    if mode=="capture": capture(host,arg)
    elif mode=="compare": compare(host,arg)
    else: print(__doc__); sys.exit(2)
