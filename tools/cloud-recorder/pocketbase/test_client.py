#!/usr/bin/env python3
"""Acts as one gateway against the OTGW collection point and checks the rules (TASK-1938).

    python3 test_client.py https://rec.example.nl gw-test@devices.invalid <password>

Checks, each printed as PASS or FAIL:
  1. the device logs in (auth-with-password)
  2. it posts a batch (a real encoded 300 s chunk when the codec study script is found)
  3. a resend of the same stream and seq is refused (unique index)
  4. a batch claimed for another device is refused (create rule)
  5. the device cannot list batches (locked rule, 403)
  6. a wrong password is refused
  7. the admin UI is not reachable through the tunnel (expect 404 from the tunnel path rule)
Standard library only.
"""
import base64, json, os, sys, time, urllib.error, urllib.request

def call(method, url, body=None, token=None):
    data = json.dumps(body).encode() if body is not None else None
    req = urllib.request.Request(url, data=data, method=method)
    req.add_header("Content-Type", "application/json")
    if token:
        req.add_header("Authorization", token)
    try:
        with urllib.request.urlopen(req, timeout=20) as r:
            return r.status, json.loads(r.read() or b"{}")
    except urllib.error.HTTPError as e:
        raw = e.read()
        try:
            return e.code, json.loads(raw or b"{}")
        except ValueError:
            return e.code, {"raw": raw[:200].decode(errors="replace")}
    except urllib.error.URLError as e:
        return 0, {"error": str(e.reason)}

def sample_payload():
    """One 300 s OT chunk from the codec study, or random bytes of the same size."""
    here = os.path.dirname(os.path.abspath(__file__))
    sys.path.insert(0, os.path.join(here, "..", "..", "..", "scripts"))
    try:
        import ot_stream_codec_study as oc
        frames = oc.synthetic(300 / 3600)
        start, n, data = oc.model_codec(frames, 300)[0]
        return data, f"codec chunk, {n} frames"
    except Exception:
        return os.urandom(330), "random bytes"

def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    base, ident, pw = sys.argv[1].rstrip("/"), sys.argv[2], sys.argv[3]
    results = []
    def check(name, ok, detail=""):
        results.append(ok)
        print(f"{'PASS' if ok else 'FAIL'}  {name}  {detail}")

    st, auth = call("POST", f"{base}/api/collections/devices/auth-with-password",
                    {"identity": ident, "password": pw})
    check("1 login", st == 200 and "token" in auth, f"HTTP {st}")
    if st != 200:
        sys.exit(1)
    token, dev = auth["token"], auth["record"]["id"]

    payload, kind = sample_payload()
    seq = int(time.time())
    batch = {"device": dev, "stream": "ot", "seq": seq, "t0": int(time.time() * 1000), "fmt": 1,
             "payload": base64.b64encode(payload).decode()}
    st, r = call("POST", f"{base}/api/collections/batches/records", batch, token)
    check("2 post batch", st == 200, f"HTTP {st}, {len(payload)} B ({kind})")

    st, r = call("POST", f"{base}/api/collections/batches/records", batch, token)
    check("3 resend same seq refused", st == 400, f"HTTP {st}")

    other = dict(batch, device="aaaaaaaaaaaaaaa", seq=seq + 1)
    st, r = call("POST", f"{base}/api/collections/batches/records", other, token)
    check("4 batch for another device refused", st in (400, 403, 404), f"HTTP {st}")

    st, r = call("GET", f"{base}/api/collections/batches/records", None, token)
    check("5 list batches refused", st == 403, f"HTTP {st}")

    st, r = call("POST", f"{base}/api/collections/devices/auth-with-password",
                 {"identity": ident, "password": pw + "x"})
    check("6 wrong password refused", st == 400, f"HTTP {st}")

    st, r = call("GET", f"{base}/_/")
    check("7 admin UI not reachable via tunnel", st == 404, f"HTTP {st}")

    print(f"\n{sum(results)}/{len(results)} passed")
    sys.exit(0 if all(results) else 1)

if __name__ == "__main__":
    main()
