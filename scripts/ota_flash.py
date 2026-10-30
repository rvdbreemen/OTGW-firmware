#!/usr/bin/env python3
"""Flash a 2.0.0 ESP32-S3 unit over WiFi OTA (ADR-191), keeping its settings.

Order: filesystem first (main firmware, POST /update?cmd=100&size=<n>; the firmware keeps
settings.ini and its other kept files across the new image), then the app through the
safeboot updater (POST /safeboot, wait for the updater, POST /update), then wait until the
main firmware answers again and report its version and whether the settings survived.

The device can close the connection without an HTTP response after the filesystem write
(during the remount). The script then does not abort: it polls GET /version.hash until it
shows the new image's githash (the "+<githash>" in the .littlefs.bin name; without one, any
hash other than the one before the write), and only then goes on to the app step. If the
hash does not change within FS_VERIFY_TIMEOUT it stops with an error before the app step.
Stopping between the two steps leaves the old app on the new filesystem; that mix can serve
the new web assets cut short (seen on a unit with an alpha.521 app and an alpha.542
filesystem: v2.js cut at 64 KB, the v2 UI does not start), so rerun the
script with --no-fs to flash the app.

Usage:
  python scripts/ota_flash.py --host 192.168.88.61 [--app build/...ino.bin] [--fs build/...littlefs.bin]
                              [--password <admin pw>]
Without --app/--fs it picks the newest esp32-combo .ino.bin and .littlefs.bin in build/.
Stdlib only.
"""
import argparse
import base64
import glob
import http.client
import json
import os
import re
import subprocess
import sys
import time
import urllib.error
import urllib.request

BOUNDARY = "----otgwota7MA4YWxkTrZu0gW"
FS_VERIFY_TIMEOUT = 120


def _headers(host, password, extra=None):
    h = {"Origin": f"http://{host}"}
    if password:
        h["Authorization"] = "Basic " + base64.b64encode(f"admin:{password}".encode()).decode()
    if extra:
        h.update(extra)
    return h


def _get_json(host, path, timeout=5):
    with urllib.request.urlopen(f"http://{host}{path}", timeout=timeout) as r:
        return json.load(r)


def _post(host, path, password, body=b"", ctype=None, timeout=30):
    extra = {"Content-Type": ctype} if ctype else {}
    req = urllib.request.Request(f"http://{host}{path}", data=body, method="POST",
                                 headers=_headers(host, password, extra))
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return r.status, r.read()


def _multipart(path):
    name = os.path.basename(path)
    with open(path, "rb") as f:
        data = f.read()
    head = (f"--{BOUNDARY}\r\nContent-Disposition: form-data; name=\"update\"; filename=\"{name}\"\r\n"
            "Content-Type: application/octet-stream\r\n\r\n").encode()
    tail = f"\r\n--{BOUNDARY}--\r\n".encode()
    return head + data + tail, len(data)


def _wait(fn, what, timeout):
    t0 = time.time()
    while time.time() - t0 < timeout:
        try:
            v = fn()
            if v:
                return v
        except Exception:
            pass
        time.sleep(2)
    raise SystemExit(f"timeout waiting for {what}")


def _main_up(host):
    d = _get_json(host, "/api/v2/device/info")["device"]
    return d.get("fwversion")


def _safeboot_up(host):
    # The safeboot updater serves /info; the main firmware does not.
    with urllib.request.urlopen(f"http://{host}/info", timeout=4) as r:
        return r.status == 200 and b"safeboot" in r.read().lower()


def _version_hash(host):
    with urllib.request.urlopen(f"http://{host}/version.hash", timeout=4) as r:
        return r.read().decode("ascii", "replace").strip()


def _image_githash(path):
    m = re.search(r"\+([0-9a-f]{7,40})\.littlefs\.bin$", os.path.basename(path))
    return m.group(1) if m else None


def flash_fs(host, password, fs):
    """Filesystem OTA, then wait until /version.hash shows the new image."""
    want = _image_githash(fs)
    try:
        old = _version_hash(host)
    except Exception:
        old = None
    body, n = _multipart(fs)
    print(f"filesystem OTA: {os.path.basename(fs)} ({n} bytes)")
    try:
        st, out = _post(host, f"/update?cmd=100&size={n}", password, body,
                        f"multipart/form-data; boundary={BOUNDARY}", timeout=180)
        print("  ->", st, out[:120].decode("latin-1", "replace").strip())
    except urllib.error.HTTPError:
        raise
    except (http.client.HTTPException, OSError) as e:
        # The write can complete and the device drop the connection while it remounts.
        print(f"  -> no response ({type(e).__name__}: {e}); checking /version.hash")
    time.sleep(5)

    def _new_fs():
        h = _version_hash(host)
        return h if (h == want if want else (h and h != old)) else None

    try:
        h = _wait(_new_fs, "the new filesystem", FS_VERIFY_TIMEOUT)
    except SystemExit:
        raise SystemExit(f"filesystem update not confirmed: /version.hash did not change to "
                         f"{want or 'a new hash'} (was {old}); app step skipped")
    print(f"  filesystem confirmed: /version.hash {old} -> {h}")
    _wait(lambda: _main_up(host), "main firmware after filesystem update", 120)


def newest(pattern):
    files = sorted(glob.glob(pattern), key=os.path.getmtime)
    return files[-1] if files else None


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", required=True)
    ap.add_argument("--app")
    ap.add_argument("--fs")
    ap.add_argument("--password", default="")
    ap.add_argument("--no-app", action="store_true")
    ap.add_argument("--no-fs", action="store_true")
    a = ap.parse_args(argv)
    app = None if a.no_app else (a.app or newest("build/*esp32-combo*.ino.bin"))
    fs = None if a.no_fs else (a.fs or newest("build/*esp32-combo*.littlefs.bin"))
    if not app and not fs:
        raise SystemExit("nothing to flash")

    before = _get_json(a.host, "/api/v2/settings")["settings"]
    print("running:", _main_up(a.host))

    if fs:
        flash_fs(a.host, a.password, fs)

    if app:
        print("handover to safeboot")
        _post(a.host, "/safeboot", a.password)
        _wait(lambda: _safeboot_up(a.host), "safeboot updater", 120)
        # The safeboot updater runs the synchronous WebServer; a urllib upload of the
        # whole body gets the connection closed without a response, curl's -F upload
        # (as a browser form) goes through.
        print(f"app OTA: {os.path.basename(app)} ({os.path.getsize(app)} bytes)")
        cmd = ["curl", "-sS", "-m", "300", "-H", f"Origin: http://{a.host}",
               "-F", f"update=@{os.path.abspath(app)};filename={os.path.basename(app)}",
               "-w", " | HTTP %{http_code}", f"http://{a.host}/update"]
        if a.password:
            cmd[1:1] = ["-u", f"admin:{a.password}"]
        # The updater can answer /info a moment before its upload handler accepts
        # a body, so give the upload a few tries before giving up.
        for attempt in range(1, 4):
            time.sleep(3)
            r = subprocess.run(cmd, capture_output=True, text=True)
            out = (r.stdout + " " + r.stderr).strip()
            print(f"  -> try {attempt}: {out}")
            if "HTTP 200" in out:
                break
        else:
            raise SystemExit("app upload to safeboot failed; the unit stays in safeboot")
        time.sleep(5)

    ver = _wait(lambda: _main_up(a.host), "main firmware", 180)
    print("running:", ver)
    after = _get_json(a.host, "/api/v2/settings")["settings"]
    diff = sorted(k for k in set(before) | set(after)
                  if before.get(k, {}).get("value") != after.get(k, {}).get("value"))
    print(f"settings: {len(after)} keys, changed: {diff if diff else 'none'}")
    return 0 if not diff else 2


if __name__ == "__main__":
    sys.exit(main())
