#!/usr/bin/env python3
"""Acts as gateways against the OTGW collection point and checks its rules (TASK-1938, TASK-2903).

    python3 test_client.py http://127.0.0.1:8090 <admin-email> <admin-password> [--db pb_data/data.db]

Checks, each printed as PASS or FAIL:
   1. a new gateway enrolls itself with its uid and a secret, and logs in with them (blocked = false)
   2. a second enrollment of the same uid is refused (unique index)
   3. an enrollment that sends "blocked" is refused (create rule)
   4. a uid that is not otgw-<12 hex> is refused (field pattern)
   5. a gateway updates its own label/fw, but cannot set "blocked" or change its uid
   6. it posts a batch; a resend of the same stream and seq is refused (unique index)
   7. a batch claimed for another device is refused (create rule)
   8. a gateway cannot list batches
   9. the maintainer blocks the gateway: its login, uploads (old token too), token refresh and own record
      update all get HTTP 403 with 'otgw-blocked' (pb_hooks/blocked.pb.js)
  10. with --db: a batch made 31 days old is deleted by the retention cron, a fresh one stays
  11. the admin UI is not reachable (only meaningful through the tunnel: expect 404 from its path rule)
  12. telemetry mode (TASK-2906): a gateway cannot set mode/flags/mode_until/mode_note on enroll or update; after
      the maintainer sets them, the gateway's login and token refresh both return them
Waits out the per-IP rate limits (429). Standard library only.
"""
import base64, json, os, secrets, sqlite3, sys, time, urllib.error, urllib.request


def call(method, url, body=None, token=None):
    data = json.dumps(body).encode() if body is not None else None
    for _ in range(4):
        req = urllib.request.Request(url, data=data, method=method)
        req.add_header("Content-Type", "application/json")
        # Cloudflare's Browser Integrity Check refuses the default "Python-urllib/x.y" agent (error code 1010).
        req.add_header("User-Agent", "otgw-test-client/1.0")
        if token:
            req.add_header("Authorization", token)
        try:
            with urllib.request.urlopen(req, timeout=20) as r:
                raw = r.read()
                status = r.status
        except urllib.error.HTTPError as e:
            raw, status = e.read(), e.code
        except urllib.error.URLError as e:
            return 0, {"error": str(e.reason)}
        if status != 429:
            break
        time.sleep(61)                                  # per-IP rate limit window
    try:
        return status, json.loads(raw or b"{}")
    except ValueError:
        return status, {"raw": raw[:200].decode(errors="replace")}


results = []


def check(name, ok, detail=""):
    results.append(ok)
    print(("PASS " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""))


def enroll(base, uid, pw, **extra):
    body = {"uid": uid, "password": pw, "passwordConfirm": pw, **extra}
    return call("POST", f"{base}/api/collections/devices/records", body)


def login(base, uid, pw):
    return call("POST", f"{base}/api/collections/devices/auth-with-password", {"identity": uid, "password": pw})


def main():
    args = sys.argv[1:]
    db = None
    if "--db" in args:
        i = args.index("--db")
        db = args[i + 1]
        del args[i:i + 2]
    if len(args) != 3:
        sys.exit(__doc__)
    base, admin_email, admin_pw = args[0].rstrip("/"), args[1], args[2]
    uid = "otgw-" + secrets.token_hex(6)
    uid2 = "otgw-" + secrets.token_hex(6)
    pw = secrets.token_urlsafe(18)

    st, _ = enroll(base, uid, pw, label="test gateway", fw="2.0.0-poc.2", board="OTGW32")
    st2, auth = login(base, uid, pw)
    tok = auth.get("token")
    rec = auth.get("record", {})
    check("1 enroll + login by uid", st == 200 and st2 == 200 and tok and rec.get("blocked") is False, f"{st} {st2}")

    st, _ = enroll(base, uid, secrets.token_urlsafe(18))
    check("2 second enrollment of the same uid refused", st == 400, str(st))
    st, _ = enroll(base, uid2, pw, blocked=False)
    check("3 enrollment that sends blocked refused", st == 400, str(st))
    st, _ = enroll(base, "otgw-xyz", pw)
    check("4 uid with the wrong pattern refused", st == 400, str(st))

    url = f"{base}/api/collections/devices/records/{rec.get('id')}"
    st_ok, _ = call("PATCH", url, {"label": "renamed", "fw": "2.0.0-poc.3"}, tok)
    st_bl, _ = call("PATCH", url, {"blocked": True}, tok)
    st_uid, _ = call("PATCH", url, {"uid": uid2}, tok)
    check("5 own label/fw updated; blocked and uid not", st_ok == 200 and st_bl in (400, 403, 404) and st_uid in (400, 403, 404),
          f"{st_ok} {st_bl} {st_uid}")

    payload = base64.b64encode(os.urandom(64)).decode()
    batch = {"device": rec.get("id"), "stream": "mix", "seq": 1, "t0": int(time.time() * 1000), "fmt": 4, "payload": payload}
    st, _ = call("POST", f"{base}/api/collections/batches/records?fields=id", batch, tok)
    st2, _ = call("POST", f"{base}/api/collections/batches/records?fields=id", batch, tok)
    check("6 batch stored, resend of the same seq refused", st == 200 and st2 == 400, f"{st} {st2}")

    st, _ = call("POST", f"{base}/api/collections/batches/records", dict(batch, device="abcdefghijklmno", seq=2), tok)
    check("7 batch for another device refused", st in (400, 403), str(st))
    st, body = call("GET", f"{base}/api/collections/batches/records", None, tok)
    check("8 gateway cannot list batches", st in (403, 404) or (st == 200 and body.get("totalItems") == 0), str(st))

    st, a = call("POST", f"{base}/api/collections/_superusers/auth-with-password", {"identity": admin_email, "password": admin_pw})
    admin = a.get("token")
    st_b, _ = call("PATCH", url, {"blocked": True}, admin)
    blk = lambda r: "otgw-blocked" in json.dumps(r)
    st2, r2 = login(base, uid, pw)                                                   # a new login
    st3, r3 = call("POST", f"{base}/api/collections/batches/records?fields=id", dict(batch, seq=3), tok)   # old token
    st4, r4 = call("POST", f"{base}/api/collections/devices/auth-refresh", {}, tok)
    st5, r5 = call("PATCH", url, {"label": "x"}, tok)
    check("9 blocked: login, upload, refresh and own update all get 403 otgw-blocked",
          st_b == 200 and all(s == 403 and blk(r) for s, r in ((st2, r2), (st3, r3), (st4, r4), (st5, r5))),
          f"{st_b} {st2} {st3} {st4} {st5}")

    if db:
        call("PATCH", url, {"blocked": False}, admin)
        _, auth3 = login(base, uid, pw)
        st, old = call("POST", f"{base}/api/collections/batches/records?fields=id", dict(batch, seq=10), auth3.get("token"))
        st, new = call("POST", f"{base}/api/collections/batches/records?fields=id", dict(batch, seq=11), auth3.get("token"))
        con = sqlite3.connect(db)
        con.execute("UPDATE batches SET created = '2000-01-01 00:00:00.000Z' WHERE id = ?", (old.get("id"),))
        con.commit()
        con.close()
        st_run, _ = call("POST", f"{base}/api/crons/otgwBatchRetention", None, admin)
        time.sleep(2)
        st_old, _ = call("GET", f"{base}/api/collections/batches/records/{old.get('id')}", None, admin)
        st_new, _ = call("GET", f"{base}/api/collections/batches/records/{new.get('id')}", None, admin)
        check("10 retention deletes a 31-day-old batch, keeps a fresh one", st_run == 204 and st_old == 404 and st_new == 200,
              f"run {st_run}, old {st_old}, new {st_new}")

    uid3, pw3 = "otgw-" + secrets.token_hex(6), secrets.token_urlsafe(18)
    st_e, _ = enroll(base, uid3, pw3, mode="full")
    st_e2, _ = enroll(base, uid3, pw3)
    _, a3 = login(base, uid3, pw3)
    url3 = f"{base}/api/collections/devices/records/{a3.get('record', {}).get('id')}"
    refused = [call("PATCH", url3, {k: v}, a3.get("token"))[0] for k, v in
               (("mode", "full"), ("flags", ["sat"]), ("mode_until", "2030-01-01 00:00:00.000Z"), ("mode_note", "x"))]
    st_m, _ = call("PATCH", url3, {"mode": "custom", "flags": ["sat", "mqtt"], "mode_until": "2030-01-01 00:00:00.000Z",
                                   "mode_note": "test"}, admin)
    _, a4 = login(base, uid3, pw3)
    _, r4 = call("POST", f"{base}/api/collections/devices/auth-refresh", {}, a4.get("token"))
    rec4, rr4 = a4.get("record", {}), r4.get("record", {})
    ok = (st_e == 400 and st_e2 == 200 and a3.get("record", {}).get("mode") in ("", None) and
          all(x in (400, 403, 404) for x in refused) and st_m == 200 and
          all(r.get("mode") == "custom" and sorted(r.get("flags") or []) == ["mqtt", "sat"] and
              str(r.get("mode_until", "")).startswith("2030-01-01") for r in (rec4, rr4)))
    check("12 telemetry mode: only the maintainer sets it; login and refresh return it", ok,
          f"enroll {st_e}/{st_e2}, gateway updates {refused}, admin {st_m}, login {rec4.get('mode')}, refresh {rr4.get('mode')}")

    st, _ = call("GET", f"{base}/_/")
    check("11 admin UI not reachable (tunnel only)", st == 404, f"{st}: expected through the tunnel, not on the LAN")
    print(f"{sum(results)}/{len(results)} passed")


if __name__ == "__main__":
    main()
