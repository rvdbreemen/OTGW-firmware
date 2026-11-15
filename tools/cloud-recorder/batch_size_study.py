#!/usr/bin/env python3
"""Measures batch sizes against a local PocketBase (TASK-1938).

    python3 batch_size_study.py --image otgw-pb-test --work /tmp/bss

Needs Docker and the image built from pocketbase/Dockerfile. For every variant it starts a fresh PocketBase
with the repo's migration, uploads one hour of data as one device over one keep-alive HTTP connection, then
stops the container and measures the vacuumed database. Rate limits are switched off for the measurement only.

Every create asks for ?fields=id, so the reply carries only the new record's id.

Variants:
  ot  per line   one record per OT frame, 50 records per request through PocketBase's /api/batch
  ot  N s        one record per N seconds, fmt 1 (range coder, model reset per record)
  dbg N s        one record per N seconds of debug output, address token + zlib (dbg_size_study.py)

Reported per variant, for one hour of data: records, HTTP requests, payload bytes, request and response bytes
on the wire (HTTP headers + body, TLS and TCP not counted), database growth, mean server time per request.
"""
import argparse, base64, http.client, json, os, shutil, sqlite3, subprocess, sys, time, zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import otrec  # noqa: E402
import dbg_size_study  # noqa: E402

ADMIN, ADMIN_PW = "admin@test.invalid", "Adm1n-test-pass-123"
DEV, DEV_PW = "gw-test@devices.invalid", "gw-test-pass-123456"


class Conn:
    """One keep-alive connection that counts the bytes it sends and receives."""

    def __init__(self, port):
        self.c = http.client.HTTPConnection("127.0.0.1", port, timeout=60)
        self.sent = self.recv = self.n = 0
        self.secs = 0.0
        self.token = None

    def call(self, method, path, body=None):
        data = json.dumps(body, separators=(",", ":")).encode() if body is not None else b""
        headers = {"Content-Type": "application/json", "Content-Length": str(len(data))}
        if self.token:
            headers["Authorization"] = self.token
        head = f"{method} {path} HTTP/1.1\r\nHost: 127.0.0.1\r\n" + "".join(f"{k}: {v}\r\n" for k, v in headers.items()) + "\r\n"
        t = time.perf_counter()
        self.c.request(method, path, body=data, headers=headers)
        r = self.c.getresponse()
        raw = r.read()
        self.secs += time.perf_counter() - t
        self.sent += len(head) + len(data)
        self.recv += len(f"HTTP/1.1 {r.status} {r.reason}\r\n") + len(str(r.msg)) + 2 + len(raw)
        self.n += 1
        return r.status, (json.loads(raw) if raw else {})


def docker(*args, check=True):
    return subprocess.run(["docker", *args], check=check, capture_output=True, text=True)


def start(image, data_dir, port):
    repo_pb = os.path.join(HERE, "pocketbase")
    docker("rm", "-f", "pbss", check=False)
    docker("run", "-d", "--name", "pbss", "-p", f"{port}:8090", "-v", f"{data_dir}:/pb/pb_data",
           "-v", f"{repo_pb}/pb_hooks:/pb/pb_hooks:ro", "-v", f"{repo_pb}/pb_migrations:/pb/pb_migrations:ro", image)
    for _ in range(60):
        try:
            c = http.client.HTTPConnection("127.0.0.1", port, timeout=2)
            c.request("GET", "/api/health")
            if c.getresponse().status == 200:
                break
        except OSError:
            time.sleep(0.5)
    docker("exec", "pbss", "/pb/pocketbase", "superuser", "upsert", ADMIN, ADMIN_PW, "--dir=/pb/pb_data")
    adm = Conn(port)
    adm.token = adm.call("POST", "/api/collections/_superusers/auth-with-password", {"identity": ADMIN, "password": ADMIN_PW})[1]["token"]
    adm.call("PATCH", "/api/settings", {"rateLimits": {"enabled": False},
                                        "batch": {"enabled": True, "maxRequests": 100, "timeout": 10, "maxBodySize": 0}})
    adm.call("POST", "/api/collections/devices/records",
             {"email": DEV, "password": DEV_PW, "passwordConfirm": DEV_PW, "enabled": True, "label": "study"})
    dev = Conn(port)
    st, a = dev.call("POST", "/api/collections/devices/auth-with-password", {"identity": DEV, "password": DEV_PW})
    dev.token = a["token"]
    dev.sent = dev.recv = dev.n = 0
    dev.secs = 0.0
    return dev, a["record"]["id"]


def db_bytes(data_dir, work):
    docker("stop", "pbss")
    out = os.path.join(work, "vacuum.db")
    if os.path.exists(out):
        os.remove(out)
    c = sqlite3.connect(os.path.join(data_dir, "data.db"))
    c.execute(f"VACUUM INTO '{out}'")
    c.close()
    docker("rm", "-f", "pbss", check=False)
    return os.path.getsize(out)


def run_variant(image, work, port, name, records, per_request):
    """records: list of (stream, t0, fmt, payload str). Returns a result dict."""
    data_dir = os.path.join(work, "data_" + name.replace(" ", "_"))
    shutil.rmtree(data_dir, ignore_errors=True)
    os.makedirs(data_dir)
    dev, dev_id = start(image, data_dir, port)
    payload = 0
    for i in range(0, len(records), per_request):
        chunk = records[i:i + per_request]
        bodies = [{"device": dev_id, "stream": s, "seq": i + k, "t0": t0, "fmt": fmt, "payload": p}
                  for k, (s, t0, fmt, p) in enumerate(chunk)]
        payload += sum(len(p) for _, _, _, p in chunk)
        if per_request == 1:
            st, r = dev.call("POST", "/api/collections/batches/records?fields=id", bodies[0])
        else:
            st, r = dev.call("POST", "/api/batch", {"requests": [
                {"method": "POST", "url": "/api/collections/batches/records?fields=id", "body": b} for b in bodies]})
        if st != 200:
            sys.exit(f"{name}: HTTP {st} {str(r)[:300]}")
    size = db_bytes(data_dir, work)
    return {"name": name, "records": len(records), "requests": dev.n, "payload": payload, "sent": dev.sent,
            "recv": dev.recv, "db": size, "ms": 1000 * dev.secs / max(1, dev.n)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--image", default="otgw-pb-test")
    ap.add_argument("--work", required=True)
    ap.add_argument("--port", type=int, default=8091)
    a = ap.parse_args()
    os.makedirs(a.work, exist_ok=True)
    t_start = 1763103600000
    frames = [(t + t_start, L, f) for t, L, f in otrec.codec.synthetic(1.0)]
    results = []

    base = run_variant(a.image, a.work, a.port, "empty", [], 1)
    results.append(base)
    per_line = [("ot", t, 3, f"{L}{f:08X}") for t, L, f in frames]
    results.append(run_variant(a.image, a.work, a.port, "ot per line", per_line, 50))
    for secs in (10, 60, 300, 900):
        recs = []
        for b in otrec.split_batches(frames, secs):
            t0, p = otrec.pack_ot(b)
            recs.append(("ot", t0, otrec.FMT_OT_V1, base64.b64encode(p).decode()))
        results.append(run_variant(a.image, a.work, a.port, f"ot {secs} s", recs, 1))
    for secs in (10, 30, 60, 120):
        batches, lines, _ = dbg_size_study.generate(3000, 3600, secs)
        recs = [("dbg", t_start + t0, 2, base64.b64encode(zlib.compress(k, 6)).decode()) for t0, _, k in batches]
        results.append(run_variant(a.image, a.work, a.port, f"dbg {secs} s", recs, 1))

    empty = results[0]["db"]
    print(f"one hour of data, {len(frames)} OT frames; database growth measured against an empty database ({empty} B)")
    print(f"{'variant':12s} {'records':>8s} {'requests':>8s} {'payload B':>10s} {'sent B':>10s} {'recv B':>10s} "
          f"{'db growth B':>12s} {'ms/req':>7s}")
    for r in results[1:]:
        print(f"{r['name']:12s} {r['records']:8d} {r['requests']:8d} {r['payload']:10d} {r['sent']:10d} {r['recv']:10d} "
              f"{r['db'] - empty:12d} {r['ms']:7.1f}")


if __name__ == "__main__":
    main()
