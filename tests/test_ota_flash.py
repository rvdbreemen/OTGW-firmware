"""TASK-1372: ota_flash.py flashes the app first, then the filesystem (OtaFlashOrder).
TASK-1494: ota_flash.py must survive a dropped response after the filesystem write.

A fake device (http.server on 127.0.0.1) accepts the filesystem POST, flips
/version.hash to the new image's githash and closes the connection without a
response, as bench .63 did on 2026-10-30. The script under test is the real
scripts/ota_flash.py (imported, main() called), filesystem step only (--no-app).

Run: python tests/test_ota_flash.py
"""
import gzip
import http.server
import importlib.util
import io
import json
import os
import random
import re
import sys
import tempfile
import threading
import unittest
from contextlib import redirect_stdout

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCRIPT = os.environ.get("OTGW_OTA_FLASH_PY", os.path.join(REPO, "scripts", "ota_flash.py"))

# The page at / as the device serves it: a real gzip stream (ota_flash gunzips it).
PAGE_GZ = gzip.compress(random.Random(1).randbytes(49000), mtime=0)
PAGE_LEN = len(PAGE_GZ)
OLD_HASH = "aaaaaaa"
NEW_HASH = "c00c5f0"


def load_script():
    spec = importlib.util.spec_from_file_location("ota_flash_under_test", SCRIPT)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    mod.time.sleep = lambda s: None  # no real waiting; _wait still bounds on time.time()
    return mod


class FakeDevice(http.server.BaseHTTPRequestHandler):
    # Class attributes set per test: whether the POST answers, whether the write lands.
    respond = False
    write_lands = True
    fs_hash = OLD_HASH
    # TASK-1497: how the page at / (v2.html.gz) comes back ("whole", "short": Content-Length
    # PAGE_LEN but the connection closes after 16384 bytes, as on .63, "empty") and the
    # device/info flag (None = the field is absent, as on firmware before TASK-1495).
    page = "whole"
    fs_incomplete = None
    events = []  # TASK-1372: order of the OTA steps the device saw
    # TASK-1640: --webui. Pages the device received through /upload (file name -> bytes);
    # tamper makes it serve something else than it stored.
    uploads = {}
    tamper = False

    def log_message(self, *args):
        pass

    def _send(self, body, ctype="application/json"):
        data = body.encode()
        self.send_response(200)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        if self.path == "/version.hash":
            self._send(type(self).fs_hash + "\n", "text/plain")
        elif self.path == "/info":
            self._send("safeboot updater", "text/plain")
        elif self.path == "/api/v2/settings":
            self._send(json.dumps({"settings": {"hostname": {"value": "otgw"}}}))
        elif self.path == "/api/v2/device/info":
            dev = {"fwversion": "2.0.0-alpha.542"}
            if type(self).fs_incomplete is not None:
                dev["fsupdateincomplete"] = type(self).fs_incomplete
            self._send(json.dumps({"device": dev}))
        elif self.path == "/" and not type(self).uploads:
            mode = type(self).page
            if mode == "busy_once":  # the file-serve gate answers 503 once, then serves it
                type(self).page = "whole"
                self.send_error(503)
                return
            # TASK-1687 (PR #698 head 7bcae1b97): "fsexplorer" is the gzipped recovery page the
            # firmware serves at / when v2.html.gz is missing, "corrupt" a v2.html.gz whose gzip
            # trailer (CRC and size) is damaged, "noetag" a valid v2.html.gz without its ETag.
            body = {"whole": PAGE_GZ, "short": PAGE_GZ, "empty": b"", "plain": b"<html>old v2.html</html>",
                    "badgzip": b"x" * 5000, "fsexplorer": gzip.compress(b"<html>FSexplorer</html>", mtime=0),
                    "corrupt": PAGE_GZ[:-8] + bytes(8), "noetag": PAGE_GZ,
                    "deflate": PAGE_GZ[:10] + b"\xff" * 40 + PAGE_GZ[50:]}[mode]
            sent = 16384 if mode == "short" else len(body)
            name = "FSexplorer.html.gz" if mode == "fsexplorer" else "v2.html.gz"
            self.send_response(200)
            self.send_header("Content-Type", "text/html")
            if mode != "plain":  # "plain": the pre-gzip v2.html fallback, no Content-Encoding
                self.send_header("Content-Encoding", "gzip")
            if mode not in ("plain", "noetag"):  # as webSendGzipPage(): "<githash>-/<file>"
                self.send_header("ETag", f'"{type(self).fs_hash}-/{name}"')
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body[:sent])
            self.close_connection = True
        elif self.path in ("/", "/FSexplorer.html") and type(self).uploads:
            name = "v2.html.gz" if self.path == "/" else "FSexplorer.html.gz"
            data = type(self).uploads.get(name, b"")
            if type(self).tamper:
                data = data[:-1] + b"!"
            self.send_response(200)
            self.send_header("Content-Encoding", "gzip")
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)
        else:
            self.send_error(404)

    def do_POST(self):
        n = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(n)
        if self.path == "/upload?path=/":
            m = re.search(rb'filename="([^"]+)"\r\nContent-Type: [^\r]*\r\n\r\n(.*)\r\n--[^\r]*--\r\n$', body, re.S)
            type(self).uploads[m.group(1).decode()] = m.group(2)
            self._send("OK", "text/plain")
            return
        if self.path == "/safeboot":
            type(self).events.append("safeboot")
            self._send("OK", "text/plain")
            return
        type(self).events.append("fs")
        if type(self).write_lands:
            type(self).fs_hash = NEW_HASH
        if type(self).respond:
            self._send("OK", "text/plain")
        else:
            self.close_connection = True  # drop: no status line, socket closed


class OtaFlashDroppedResponse(unittest.TestCase):
    def setUp(self):
        FakeDevice.respond = False
        FakeDevice.write_lands = True
        FakeDevice.fs_hash = OLD_HASH
        FakeDevice.page = "whole"
        FakeDevice.fs_incomplete = None
        FakeDevice.events = []
        FakeDevice.uploads = {}
        FakeDevice.tamper = False
        self.srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), FakeDevice)
        threading.Thread(target=self.srv.serve_forever, daemon=True).start()
        self.host = f"127.0.0.1:{self.srv.server_address[1]}"
        self.tmp = tempfile.TemporaryDirectory()
        self.fs = os.path.join(self.tmp.name,
                               f"OTGW-firmware-esp32-combo-2.0.0-alpha.542+{NEW_HASH}.littlefs.bin")
        with open(self.fs, "wb") as f:
            f.write(b"\xff" * 4096)
        self.mod = load_script()
        if hasattr(self.mod, "FS_VERIFY_TIMEOUT"):
            self.mod.FS_VERIFY_TIMEOUT = 2

    def tearDown(self):
        self.srv.shutdown()
        self.srv.server_close()
        self.tmp.cleanup()

    def run_main(self):
        out = io.StringIO()
        with redirect_stdout(out):
            rc = self.mod.main(["--host", self.host, "--fs", self.fs, "--no-app"]) \
                if self.mod.main.__code__.co_argcount else self._legacy_main()
        return rc, out.getvalue()

    def _legacy_main(self):
        sys.argv = ["ota_flash.py", "--host", self.host, "--fs", self.fs, "--no-app"]
        return self.mod.main()

    def test_dropped_response_continues_when_new_hash_is_served(self):
        rc, out = self.run_main()
        self.assertEqual(rc, 0, out)
        self.assertIn(f"/version.hash {OLD_HASH} -> {NEW_HASH}", out)

    def test_dropped_response_without_new_hash_fails_clearly(self):
        FakeDevice.write_lands = False
        with self.assertRaises(SystemExit) as cm:
            self.run_main()
        self.assertIn("filesystem update not confirmed", str(cm.exception.code))
        
    def test_normal_response_still_checks_the_hash(self):
        FakeDevice.respond = True
        rc, out = self.run_main()
        self.assertEqual(rc, 0, out)
        self.assertIn(f"filesystem confirmed: /version.hash {OLD_HASH} -> {NEW_HASH}", out)

    def test_githash_is_taken_from_the_image_name(self):
        if not hasattr(self.mod, "_image_githash"):
            self.fail("ota_flash.py has no _image_githash()")
        self.assertEqual(self.mod._image_githash(self.fs), NEW_HASH)
        self.assertIsNone(self.mod._image_githash("my.littlefs.bin"))

    # TASK-1497: a new /version.hash is not proof that the whole image arrived.
    def test_short_page_stops_with_an_error(self):
        FakeDevice.respond = True
        FakeDevice.page = "short"
        with self.assertRaises(SystemExit) as cm:
            self.run_main()
        msg = str(cm.exception.code)
        self.assertIn(f"/ (v2.html.gz) served short (16384 of {PAGE_LEN} bytes)", msg)

    def test_empty_page_stops_with_an_error(self):
        FakeDevice.respond = True
        FakeDevice.page = "empty"
        with self.assertRaises(SystemExit) as cm:
            self.run_main()
        self.assertIn("/ (v2.html.gz) is empty", str(cm.exception.code))

    # A missing or stale v2.html.gz: the device answers / with the plain old page.
    def test_plain_page_without_gzip_stops_with_an_error(self):
        FakeDevice.respond = True
        FakeDevice.page = "plain"
        with self.assertRaises(SystemExit) as cm:
            self.run_main()
        self.assertIn("no Content-Encoding: gzip", str(cm.exception.code))

    def test_corrupt_gzip_page_stops_with_an_error(self):
        FakeDevice.respond = True
        FakeDevice.page = "badgzip"
        with self.assertRaises(SystemExit) as cm:
            self.run_main()
        self.assertIn("not valid gzip", str(cm.exception.code))

    # TASK-1687 (PR #698 head 7bcae1b97): a gzipped, decodable answer on / is not enough.
    # With v2.html.gz missing the firmware serves the gzipped FSexplorer recovery page at /
    # (FSexplorer.ino sendIndex()); only the ETag names the file that was served.
    def test_fsexplorer_fallback_stops_with_an_error(self):
        FakeDevice.respond = True
        FakeDevice.page = "fsexplorer"
        with self.assertRaises(SystemExit) as cm:
            self.run_main()
        self.assertIn("does not serve the gzipped web UI", str(cm.exception.code))
        self.assertIn("FSexplorer.html.gz", str(cm.exception.code))

    def test_page_without_etag_stops_with_an_error(self):
        FakeDevice.respond = True
        FakeDevice.page = "noetag"
        with self.assertRaises(SystemExit) as cm:
            self.run_main()
        self.assertIn("does not serve the gzipped web UI", str(cm.exception.code))

    # PR #698's corrupt-page case: the gzip trailer (CRC32 and size) is damaged.
    def test_corrupt_gzip_trailer_stops_with_an_error(self):
        FakeDevice.respond = True
        FakeDevice.page = "corrupt"
        with self.assertRaises(SystemExit) as cm:
            self.run_main()
        self.assertIn("is not valid gzip", str(cm.exception.code))

    # Kept from dev on purpose (Robert 2026-11-07): damaged deflate data raises zlib.error,
    # which is not an OSError, so the check must catch it to stop with a clear message.
    def test_damaged_deflate_data_stops_with_an_error(self):
        FakeDevice.respond = True
        FakeDevice.page = "deflate"
        with self.assertRaises(SystemExit) as cm:
            self.run_main()
        self.assertIn("is not valid gzip", str(cm.exception.code))

    def test_device_flag_stops_with_an_error(self):
        FakeDevice.respond = True
        FakeDevice.fs_incomplete = True
        with self.assertRaises(SystemExit) as cm:
            self.run_main()
        self.assertIn("fsupdateincomplete", str(cm.exception.code))

    def test_busy_page_is_retried(self):
        FakeDevice.respond = True
        FakeDevice.page = "busy_once"
        rc, out = self.run_main()
        self.assertEqual(rc, 0, out)
        self.assertIn(f"/ (v2.html.gz) complete: {PAGE_LEN} bytes", out)

    def test_whole_page_and_clear_flag_continue(self):
        FakeDevice.respond = True
        FakeDevice.fs_incomplete = False
        rc, out = self.run_main()
        self.assertEqual(rc, 0, out)
        self.assertIn(f"/ (v2.html.gz) complete: {PAGE_LEN} bytes", out)

    # TASK-1640: --webui pushes the gzipped pages through /upload and reads them back.
    def _run_webui(self):
        out = io.StringIO()
        with redirect_stdout(out):
            rc = self.mod.main(["--host", self.host, "--webui"])
        return rc, out.getvalue()

    def test_webui_uploads_both_pages_and_verifies_them(self):
        rc, out = self._run_webui()
        self.assertEqual(rc, 0, out)
        self.assertEqual(sorted(FakeDevice.uploads), ["FSexplorer.html.gz", "v2.html.gz"])
        for data in FakeDevice.uploads.values():
            self.assertIn(b"<style>", gzip.decompress(data))
        self.assertEqual(FakeDevice.events, [])

    def test_webui_fails_when_the_device_serves_other_bytes(self):
        FakeDevice.tamper = True
        with self.assertRaises(SystemExit) as cm:
            self._run_webui()
        self.assertIn("serves", str(cm.exception.code))

    # TASK-1372: the app goes first, so a newer filesystem never lands on an older app.
    def _run_both(self, extra=()):
        app = os.path.join(self.tmp.name, "OTGW-firmware-esp32-combo-2.0.0-alpha.542+c00c5f0.ino.bin")
        with open(app, "wb") as f:
            f.write(bytes(1024))

        class Done:
            returncode = 0
            stdout = " | HTTP 200"
            stderr = ""

        def fake_run(cmd, **kw):
            FakeDevice.events.append("app")
            return Done()

        # mod.subprocess is the global module: restore it, or later test modules get the fake.
        self.addCleanup(setattr, self.mod.subprocess, "run", self.mod.subprocess.run)
        self.mod.subprocess.run = fake_run
        FakeDevice.respond = True
        out = io.StringIO()
        with redirect_stdout(out):
            rc = self.mod.main(["--host", self.host, "--fs", self.fs, "--app", app, *extra])
        return rc, out.getvalue()

    def test_app_is_flashed_before_the_filesystem(self):
        rc, out = self._run_both()
        self.assertEqual(rc, 0, out)
        self.assertEqual(FakeDevice.events, ["safeboot", "app", "fs"], out)

    def test_no_fs_flashes_the_app_only(self):
        rc, out = self._run_both(["--no-fs"])
        self.assertEqual(rc, 0, out)
        self.assertEqual(FakeDevice.events, ["safeboot", "app"], out)

    def test_no_app_flashes_the_filesystem_only(self):
        rc, out = self._run_both(["--no-app"])
        self.assertEqual(rc, 0, out)
        self.assertEqual(FakeDevice.events, ["fs"], out)


if __name__ == "__main__":
    unittest.main(verbosity=2)
