"""TASK-1494: ota_flash.py must survive a dropped response after the filesystem write.

A fake device (http.server on 127.0.0.1) accepts the filesystem POST, flips
/version.hash to the new image's githash and closes the connection without a
response, as bench .63 did on 2026-10-30. The script under test is the real
scripts/ota_flash.py (imported, main() called), filesystem step only (--no-app).

Run: python tests/test_ota_flash.py
"""
import http.server
import importlib.util
import io
import json
import os
import sys
import tempfile
import threading
import unittest
from contextlib import redirect_stdout

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCRIPT = os.environ.get("OTGW_OTA_FLASH_PY", os.path.join(REPO, "scripts", "ota_flash.py"))

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
    # TASK-1497: how /v2.js comes back ("whole", "short": Content-Length 256745 but the
    # connection closes after 65536 bytes, as on .63, "empty") and the device/info flag
    # (None = the field is absent, as on firmware before TASK-1495).
    v2js = "whole"
    fs_incomplete = None

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
        elif self.path == "/api/v2/settings":
            self._send(json.dumps({"settings": {"hostname": {"value": "otgw"}}}))
        elif self.path == "/api/v2/device/info":
            dev = {"fwversion": "2.0.0-alpha.542"}
            if type(self).fs_incomplete is not None:
                dev["fsupdateincomplete"] = type(self).fs_incomplete
            self._send(json.dumps({"device": dev}))
        elif self.path == "/v2.js":
            mode = type(self).v2js
            if mode == "busy_once":  # the file-serve gate answers 503 once, then serves it
                type(self).v2js = "whole"
                self.send_error(503)
                return
            full = 256745 if mode != "empty" else 0
            sent = {"whole": full, "short": 65536, "empty": 0}[mode]
            self.send_response(200)
            self.send_header("Content-Type", "application/javascript")
            self.send_header("Content-Length", str(full))
            self.end_headers()
            self.wfile.write(b"x" * sent)
            self.close_connection = True
        else:
            self.send_error(404)

    def do_POST(self):
        n = int(self.headers.get("Content-Length", 0))
        self.rfile.read(n)
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
        FakeDevice.v2js = "whole"
        FakeDevice.fs_incomplete = None
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
        self.assertIn("app step skipped", str(cm.exception.code))

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
    def test_short_v2js_stops_before_the_app_step(self):
        FakeDevice.respond = True
        FakeDevice.v2js = "short"
        with self.assertRaises(SystemExit) as cm:
            self.run_main()
        msg = str(cm.exception.code)
        self.assertIn("/v2.js served short (65536 of 256745 bytes)", msg)
        self.assertIn("app step skipped", msg)

    def test_empty_v2js_stops_before_the_app_step(self):
        FakeDevice.respond = True
        FakeDevice.v2js = "empty"
        with self.assertRaises(SystemExit) as cm:
            self.run_main()
        self.assertIn("/v2.js is empty", str(cm.exception.code))

    def test_device_flag_stops_before_the_app_step(self):
        FakeDevice.respond = True
        FakeDevice.fs_incomplete = True
        with self.assertRaises(SystemExit) as cm:
            self.run_main()
        self.assertIn("fsupdateincomplete", str(cm.exception.code))

    def test_busy_v2js_is_retried(self):
        FakeDevice.respond = True
        FakeDevice.v2js = "busy_once"
        rc, out = self.run_main()
        self.assertEqual(rc, 0, out)
        self.assertIn("/v2.js complete: 256745 bytes", out)

    def test_whole_v2js_and_clear_flag_continue(self):
        FakeDevice.respond = True
        FakeDevice.fs_incomplete = False
        rc, out = self.run_main()
        self.assertEqual(rc, 0, out)
        self.assertIn("/v2.js complete: 256745 bytes", out)


if __name__ == "__main__":
    unittest.main(verbosity=2)
