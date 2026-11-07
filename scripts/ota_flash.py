#!/usr/bin/env python3
"""Flash a 2.0.0 ESP32-S3 unit over WiFi OTA (ADR-191), keeping its settings.

Order: app first, then filesystem (TASK-1372). The app goes through the safeboot updater
(POST /safeboot, wait for the updater, POST /update); once the new app answers, the
filesystem follows on the main firmware (POST /update?cmd=100&size=<n>; the firmware keeps
settings.ini and its other kept files across the new image). A newer filesystem therefore
never lands on an older running app; the other way round (new app, old filesystem) only
shows the "Flash your littleFS with matching version" warning. The script ends by waiting
until the main firmware answers and reports its version and whether the settings survived.
The filesystem restart is a planned restart, which also confirms the new app (ADR-191).

The device can close the connection without an HTTP response after the filesystem write
(during the remount). The script then does not abort: it polls GET /version.hash until it
shows the new image's githash (the "+<githash>" in the .littlefs.bin name; without one, any
hash other than the one before the write). If the hash does not change within
FS_VERIFY_TIMEOUT it stops with an error.

A new /version.hash does not prove the whole image arrived: it sits in the first blocks, and
an interrupted upload leaves the later blocks of large files old, so they read back short
(seen on .63: v2.js cut at 64 KB of 251 KB, the v2 UI does not start; a clean re-upload
served it whole). After the hash the script therefore stops with an error when
device/info reports fsupdateincomplete (firmware with TASK-1495; an absent field is not
checked) or when GET / (the gzipped web UI, v2.html.gz) returns fewer bytes than its
Content-Length or none at all. Then upload the filesystem again: rerun with --no-app.

--webui (TASK-1640, debugging): build the gzipped web pages from src/OTGW-firmware/data/
(scripts/webui_pages.py, the same inlining as the filesystem build, without minifying)
and upload them to the running unit through /upload, without flashing an image. The
filesystem hash, and so the page ETag, does not change: reload with the cache bypassed.

Usage:
  python scripts/ota_flash.py --host 192.168.88.61 [--app build/...ino.bin] [--fs build/...littlefs.bin]
                              [--password <admin pw>]
  python scripts/ota_flash.py --host 192.168.88.61 --webui [--password <admin pw>]
--no-fs flashes the app only, --no-app the filesystem only.
Without --app/--fs it picks the newest esp32-combo .ino.bin and .littlefs.bin in build/.
Stdlib only.
"""
import argparse
import base64
import glob
import gzip
import http.client
import json
import os
import re
import subprocess
import sys
import time
import urllib.error
import urllib.request
import zlib

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


def _multipart(path, field="update", data=None):
    name = os.path.basename(path)
    if data is None:
        with open(path, "rb") as f:
            data = f.read()
    head = (f"--{BOUNDARY}\r\nContent-Disposition: form-data; name=\"{field}\"; filename=\"{name}\"\r\n"
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
        print("  ->", st, f"({len(out)} bytes)")  # the success page is gzipped (TASK-1640)
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
                         f"{want or 'a new hash'} (was {old})")
    print(f"  filesystem confirmed: /version.hash {old} -> {h}")
    _wait(lambda: _main_up(host), "main firmware after filesystem update", 120)
    _check_fs_complete(host, password)


def _get_raw(host, path, password, timeout=60):
    """GET path as sent (the gzipped pages stay gzipped); returns (body, response headers)."""
    req = urllib.request.Request(f"http://{host}{path}", headers=_headers(host, password))
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return r.read(), r.headers


def _check_fs_complete(host, password=""):
    """/version.hash is in the first blocks; the largest file shows whether the rest arrived."""
    try:
        dev = _get_json(host, "/api/v2/device/info")["device"]
    except Exception:
        dev = {}
    if dev.get("fsupdateincomplete") is True:
        raise SystemExit("device reports fsupdateincomplete: the filesystem update did not finish; "
                         "upload the filesystem again")
    for attempt in range(1, 4):
        try:
            body, headers = _get_raw(host, "/", password)
            n = len(body)
            break
        except http.client.IncompleteRead as e:
            got = len(e.partial)
            raise SystemExit(f"/ (v2.html.gz) served short ({got} of {got + (e.expected or 0)} bytes): the "
                             "filesystem image is incomplete; upload the filesystem again")
        except urllib.error.HTTPError as e:
            # the file-serve gate (ADR-147) answers 503 under load: try again
            if attempt == 3:
                raise SystemExit(f"/ (v2.html.gz) could not be checked (HTTP {e.code})")
            time.sleep(3)
    if n == 0:
        raise SystemExit("/ (v2.html.gz) is empty: the filesystem image is incomplete; upload the "
                         "filesystem again")
    # A missing or stale v2.html.gz makes the firmware answer with a plain page (the old
    # v2.html) or with the gzipped FSexplorer recovery page (sendIndex() in FSexplorer.ino).
    # Both can be non-empty and even valid gzip, so require the gzipped UI by its ETag
    # ("<githash>-/v2.html.gz", webSendGzipPage/serveGzipPage) as PR #698 does (TASK-1687),
    # then a gzip body that decompresses.
    encoding = headers.get("Content-Encoding") or ""
    etag = headers.get("ETag") or ""
    if encoding.lower() != "gzip":
        raise SystemExit("/ was not served as v2.html.gz (no Content-Encoding: gzip): the filesystem "
                         "image is incomplete or stale; upload the filesystem again")
    if "-/v2.html.gz" not in etag:
        raise SystemExit(f"/ does not serve the gzipped web UI (ETag {etag or 'missing'}; v2.html.gz is "
                         "missing from the filesystem image): upload the filesystem again")
    try:
        html = gzip.decompress(body)
    # zlib.error is not an OSError: damaged deflate data needs its own catch (TASK-1687).
    except (OSError, EOFError, zlib.error) as e:
        raise SystemExit(f"/ (v2.html.gz) is not valid gzip ({e}): the filesystem image is incomplete; "
                         "upload the filesystem again")
    length = headers.get("Content-Length")
    print(f"  / (v2.html.gz) complete: {n} bytes, {len(html)} bytes unpacked, ETag {etag}"
          + (f" (Content-Length {length})" if length else ""))


# Where each gzipped page is served, to read it back after a --webui push.
WEBUI_ROUTES = {"v2.html.gz": "/", "FSexplorer.html.gz": "/FSexplorer.html"}


def push_webui(host, password):
    """Build the gzipped pages from data/ and upload them through /upload (TASK-1640)."""
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    sys.path.insert(0, os.path.join(root, "scripts"))
    import webui_pages
    pages, _ = webui_pages.build_pages(os.path.join(root, "src", "OTGW-firmware", "data"))
    for name, data in pages.items():
        body, n = _multipart(name, field="upload", data=data)
        st, _ = _post(host, "/upload?path=/", password, body, f"multipart/form-data; boundary={BOUNDARY}",
                      timeout=120)
        served, _ = _get_raw(host, WEBUI_ROUTES[name], password)
        if served != data:
            raise SystemExit(f"{name}: uploaded {n} bytes, but {WEBUI_ROUTES[name]} serves {len(served)} "
                             "different bytes")
        print(f"  {name}: {n} bytes uploaded, served at {WEBUI_ROUTES[name]}")
    print("web pages pushed; reload with the cache bypassed (the ETag did not change)")


def flash_app(host, password, app):
    """App OTA through the safeboot updater; returns once the main firmware answers again."""
    print("handover to safeboot")
    _post(host, "/safeboot", password)
    _wait(lambda: _safeboot_up(host), "safeboot updater", 120)
    # The safeboot updater runs the synchronous WebServer; a urllib upload of the
    # whole body gets the connection closed without a response, curl's -F upload
    # (as a browser form) goes through.
    print(f"app OTA: {os.path.basename(app)} ({os.path.getsize(app)} bytes)")
    cmd = ["curl", "-sS", "-m", "300", "-H", f"Origin: http://{host}",
           "-F", f"update=@{os.path.abspath(app)};filename={os.path.basename(app)}",
           "-w", " | HTTP %{http_code}", f"http://{host}/update"]
    if password:
        cmd[1:1] = ["-u", f"admin:{password}"]
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
    ver = _wait(lambda: _main_up(host), "main firmware after app update", 180)
    print("  app running:", ver)


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
    ap.add_argument("--webui", action="store_true", help="push the gzipped web pages only (debugging)")
    a = ap.parse_args(argv)
    if a.webui:
        print("running:", _main_up(a.host))
        push_webui(a.host, a.password)
        return 0
    app = None if a.no_app else (a.app or newest("build/*esp32-combo*.ino.bin"))
    fs = None if a.no_fs else (a.fs or newest("build/*esp32-combo*.littlefs.bin"))
    if not app and not fs:
        raise SystemExit("nothing to flash")

    before = _get_json(a.host, "/api/v2/settings")["settings"]
    print("running:", _main_up(a.host))

    if app:
        flash_app(a.host, a.password, app)

    if fs:
        flash_fs(a.host, a.password, fs)

    ver = _wait(lambda: _main_up(a.host), "main firmware", 180)
    print("running:", ver)
    after = _get_json(a.host, "/api/v2/settings")["settings"]
    diff = sorted(k for k in set(before) | set(after)
                  if before.get(k, {}).get("value") != after.get(k, {}).get("value"))
    print(f"settings: {len(after)} keys, changed: {diff if diff else 'none'}")
    return 0 if not diff else 2


if __name__ == "__main__":
    sys.exit(main())
