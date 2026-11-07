#!/usr/bin/env python3
"""
TASK-1690: the capture tool finds the OTGW gateways on the LAN and lets the user pick one.

The firmware announces _http._tcp on port 80 over mDNS (startMDNS() in networkStuff.ino) and
answers GET /api/v2/device/info. scripts/otgw_capture.py sends one mDNS query from an
ephemeral port (so responders answer unicast, RFC 6762 section 6.7), keeps the responders
whose device/info looks like an OTGW, and offers them as a numbered list.

Every case drives the real functions of the tool:
  a  the query asks for PTR _http._tcp.local with the unicast-response bit
  b  a reply with DNS name compression is parsed (PTR, A); queries and junk are ignored
  c  mdns_browse() gets the responder from a fake mDNS responder on loopback
  d  probe_gateway() accepts an OTGW device/info and drops 404, non-JSON and foreign JSON
  e  choose_gateway(): several gateways -> numbered list, saved host is the default,
     a number, Enter, a typed host and an out-of-range number (asked again)
  f  choose_gateway(): one gateway -> used without a prompt; none -> None
  g  resolve_inputs() without --device runs the real discovery and takes the chosen IP
  h  --no-prompt and an explicit --device never run discovery
  i  --discover prints the gateways and exits 0 without starting a capture (subprocess)
  j  the MQTT password: the API gives only its length ("password=N"); N = 0 asks nothing,
     N > 0 reads it from the gateway's /settings.ini when the length matches, a 401 or a
     length mismatch falls back to a saved password of that length or the prompt

Run: python tests/test_capture_discovery.py
"""

import contextlib
import io
import json
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import unittest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from unittest import mock

REPO = Path(__file__).resolve().parent.parent
TOOL = REPO / "scripts" / "otgw_capture.py"
sys.path.insert(0, str(REPO / "scripts"))
import otgw_capture as oc  # noqa: E402

SERVICE = "_http._tcp.local"


def encode_name(name):
    return b"".join(bytes([len(p)]) + p.encode() for p in name.split(".")) + b"\0"


def mdns_reply(instance_label, ip):
    """A response the way a responder sends it: PTR in the answers, A in the additionals,
    both using compression pointers into the service name at offset 12."""
    header = struct.pack("!6H", 0, 0x8400, 0, 1, 0, 1)
    service = encode_name(SERVICE)                    # offset 12: _http(12) _tcp(18) local(23)
    instance = bytes([len(instance_label)]) + instance_label.encode() + b"\xc0\x0c"
    ptr = service + struct.pack("!HHIH", 12, 1, 120, len(instance)) + instance
    a_name = bytes([len(instance_label)]) + instance_label.encode() + b"\xc0\x17"
    a_rec = a_name + struct.pack("!HHIH", 1, 0x8001, 120, 4) + socket.inet_aton(ip)
    return header + ptr + a_rec


class FakeResponder:
    """UDP socket on loopback that answers every query with the given replies, from its own
    address, like a device answering a legacy unicast query."""

    def __init__(self, replies, send_junk=False):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(("127.0.0.1", 0))
        self.sock.settimeout(0.2)
        self.port = self.sock.getsockname()[1]
        self.replies = replies
        self.send_junk = send_junk
        self.queries = []
        self.running = True
        self.thread = threading.Thread(target=self.run, daemon=True)
        self.thread.start()

    def run(self):
        while self.running:
            try:
                data, addr = self.sock.recvfrom(9000)
            except (socket.timeout, OSError):
                continue
            self.queries.append(data)
            if self.send_junk:
                self.sock.sendto(data, addr)        # an echoed query: must be ignored
                self.sock.sendto(b"\x00\x01", addr)  # truncated junk: must be ignored
            for reply in self.replies:
                self.sock.sendto(reply, addr)

    def close(self):
        self.running = False
        self.thread.join(2)
        self.sock.close()


class FakeGateway:
    """HTTP server whose /api/v2/device/info answers like an OTGW (or not)."""

    def __init__(self, body=None, status=200, ctype="application/json"):
        outer = self

        class Handler(BaseHTTPRequestHandler):
            def do_GET(self):
                if self.path != "/api/v2/device/info" or outer.status != 200:
                    self.send_response(404 if outer.status == 200 else outer.status)
                    self.end_headers()
                    return
                payload = outer.body if isinstance(outer.body, bytes) else json.dumps(outer.body).encode()
                self.send_response(200)
                self.send_header("Content-Type", outer.ctype)
                self.send_header("Content-Length", str(len(payload)))
                self.end_headers()
                self.wfile.write(payload)

            def log_message(self, *a):
                pass

        self.body, self.status, self.ctype = body, status, ctype
        self.httpd = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        self.port = self.httpd.server_address[1]
        threading.Thread(target=self.httpd.serve_forever, daemon=True).start()

    def close(self):
        self.httpd.shutdown()
        self.httpd.server_close()


def otgw_info(hostname, fw, hardware_type=None, hardwaremode=None, pic=None):
    """device/info as the firmware sends it; the 2.x combo image reports the same board name
    on both boards, so only hardware_type and hardwaremode tell them apart."""
    device = {"hostname": hostname, "fwversion": fw, "ipaddress": "127.0.0.1"}
    if hardware_type:
        device.update(board="Nodoshop OTGW Combo (ESP32-S3)", hardware_type=hardware_type, hardwaremode=hardwaremode)
    if pic:
        device["picdeviceid"] = pic
    return {"device": device}


class ScriptedAsk:
    """Stands in for the prompt: hands out the scripted answers and records each call."""

    def __init__(self, *answers):
        self.answers = list(answers)
        self.calls = []

    def __call__(self, prompt, default=""):
        self.calls.append((prompt, default))
        answer = self.answers.pop(0) if self.answers else ""
        return answer or default


GATEWAYS = [
    {"host": "192.168.88.16", "hostname": "OTGW1", "firmware": "1.7.5-beta.6", "kind": "PIC pic16f88"},
    {"host": "192.168.88.61", "hostname": "marvin-otgw32", "firmware": "2.0.0-beta.120", "kind": "otgw32 OT-Direct"},
    {"host": "192.168.88.63", "hostname": "zaphod-classic-s3", "firmware": "2.0.0-beta.118", "kind": "otgw-classic PIC"},
]


class MdnsWireCase(unittest.TestCase):
    def test_a_query_asks_ptr_with_unicast_bit(self):
        q = oc.mdns_query(SERVICE)
        self.assertEqual(struct.unpack("!6H", q[:12]), (0, 0, 1, 0, 0, 0))
        self.assertEqual(q[12:12 + len(encode_name(SERVICE))], encode_name(SERVICE))
        qtype, qclass = struct.unpack("!HH", q[-4:])
        self.assertEqual(qtype, 12)
        self.assertEqual(qclass, 0x8001)  # QU bit + class IN

    def test_b_compressed_reply_is_parsed(self):
        recs = oc.parse_mdns_records(mdns_reply("OTGW2", "192.168.88.63"))
        self.assertIn((SERVICE, 12, "OTGW2." + SERVICE), recs)
        self.assertIn(("OTGW2.local", 1, "192.168.88.63"), recs)

    def test_b_junk_raises_value_error(self):
        with self.assertRaises(ValueError):
            oc.parse_mdns_records(b"\x00\x01")


class BrowseCase(unittest.TestCase):
    def test_c_browse_returns_the_responder(self):
        responder = FakeResponder([mdns_reply("OTGW2", "127.0.0.1")], send_junk=True)
        self.addCleanup(responder.close)
        found = oc.mdns_browse(["127.0.0.1"], wait=1.0, target=("127.0.0.1", responder.port))
        self.assertEqual([c["host"] for c in found], ["127.0.0.1"])
        self.assertEqual(found[0]["instance"], "OTGW2")
        self.assertTrue(responder.queries, "the responder never saw a query")

    def test_c_browse_without_answers_is_empty(self):
        responder = FakeResponder([])
        self.addCleanup(responder.close)
        self.assertEqual(oc.mdns_browse(["127.0.0.1"], wait=0.5, target=("127.0.0.1", responder.port)), [])


class ProbeCase(unittest.TestCase):
    def probe(self, **kw):
        gw = FakeGateway(**kw)
        self.addCleanup(gw.close)
        return oc.probe_gateway("127.0.0.1", http_port=gw.port, timeout=3)

    def test_d_otgw_is_recognised(self):
        got = self.probe(body=otgw_info("zaphod-classic-s3", "2.0.0-beta.118", "otgw-classic", "PIC", "pic16f1847"))
        self.assertEqual(got["host"], "127.0.0.1")
        self.assertEqual(got["hostname"], "zaphod-classic-s3")
        self.assertEqual(got["firmware"], "2.0.0-beta.118")
        self.assertEqual(got["kind"], "otgw-classic PIC")

    def test_d_otgw32_kind_differs_from_the_classic(self):
        got = self.probe(body=otgw_info("marvin-otgw32", "2.0.0-beta.120", "otgw32", "OT-Direct"))
        self.assertEqual(got["kind"], "otgw32 OT-Direct")

    def test_d_1x_shows_its_pic(self):
        got = self.probe(body=otgw_info("OTGW1", "1.7.5-beta.6+c71a325", pic="pic16f88"))
        self.assertEqual((got["hostname"], got["kind"]), ("OTGW1", "PIC pic16f88"))

    def test_d_not_found_is_dropped(self):
        self.assertIsNone(self.probe(status=404))

    def test_d_html_is_dropped(self):
        self.assertIsNone(self.probe(body=b"<html>printer</html>", ctype="text/html"))

    def test_d_foreign_json_is_dropped(self):
        self.assertIsNone(self.probe(body={"device": {"name": "shellyplug"}}))


class ChooseCase(unittest.TestCase):
    def choose(self, gateways, saved, *answers):
        ask = ScriptedAsk(*answers)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            host = oc.choose_gateway(gateways, saved, ask)
        return host, ask, out.getvalue()

    def test_e_list_shows_every_gateway_and_saved_is_default(self):
        host, ask, out = self.choose(GATEWAYS, "192.168.88.61", "")
        for gw in GATEWAYS:
            self.assertIn(gw["hostname"], out)
            self.assertIn(gw["host"], out)
            self.assertIn(gw["firmware"], out)
            self.assertIn(gw["kind"], out)
        self.assertIn("1)", out)
        self.assertIn("3)", out)
        self.assertEqual(ask.calls[0][1], "2")
        self.assertEqual(host, "192.168.88.61")

    def test_e_saved_hostname_also_matches(self):
        host, ask, _ = self.choose(GATEWAYS, "ZAPHOD-classic-s3.local", "")
        self.assertEqual(ask.calls[0][1], "3")
        self.assertEqual(host, "192.168.88.63")

    def test_e_number_picks_that_gateway(self):
        host, _, _ = self.choose(GATEWAYS, "", "3")
        self.assertEqual(host, "192.168.88.63")

    def test_e_no_saved_host_defaults_to_the_first(self):
        host, ask, _ = self.choose(GATEWAYS, "", "")
        self.assertEqual(ask.calls[0][1], "1")
        self.assertEqual(host, "192.168.88.16")

    def test_e_out_of_range_is_asked_again(self):
        host, ask, out = self.choose(GATEWAYS, "", "9", "0", "1")
        self.assertEqual(len(ask.calls), 3)
        self.assertEqual(host, "192.168.88.16")
        self.assertIn("1..3", out)

    def test_e_typed_host_is_used(self):
        host, _, _ = self.choose(GATEWAYS, "", "10.0.0.9")
        self.assertEqual(host, "10.0.0.9")

    def test_f_single_gateway_is_used_without_a_prompt(self):
        host, ask, out = self.choose(GATEWAYS[2:], "192.168.88.16")
        self.assertEqual(ask.calls, [])
        self.assertIn("Using the only OTGW", out)
        self.assertIn("zaphod-classic-s3", out)
        self.assertEqual(host, "192.168.88.63")

    def test_f_none_found_returns_none(self):
        host, ask, _ = self.choose([], "192.168.88.16")
        self.assertIsNone(host)
        self.assertEqual(ask.calls, [])


class ResolveCase(unittest.TestCase):
    def setUp(self):
        self.gw = FakeGateway(body=otgw_info("zaphod-classic-s3", "2.0.0-beta.118"))
        self.addCleanup(self.gw.close)
        self.responder = FakeResponder([mdns_reply("zaphod-classic-s3", "127.0.0.1")])
        self.addCleanup(self.responder.close)

    def resolve(self, argv, answers=("",), is_tty=True):
        parser = oc.build_parser()
        args = parser.parse_args(oc.normalize_argv([
            "--http-port", str(self.gw.port), "--mdns-target", f"127.0.0.1:{self.responder.port}",
            "--mdns-interface", "127.0.0.1", "--discover-seconds", "1", "--broker", "", *argv]))
        out = io.StringIO()
        with mock.patch.object(oc, "load_settings", return_value={"DeviceHost": "10.9.9.9"}), \
                mock.patch.object(oc, "interactive", return_value=is_tty), \
                mock.patch("builtins.input", side_effect=list(answers)), \
                contextlib.redirect_stdout(out):
            oc.resolve_inputs(args, parser)
        return args, out.getvalue()

    def test_g_discovery_fills_the_device(self):
        args, out = self.resolve([])
        self.assertEqual(args.device, "127.0.0.1")
        self.assertIn("zaphod-classic-s3", out)

    def test_g_nothing_found_falls_back_to_manual_entry(self):
        silent = FakeResponder([])
        self.addCleanup(silent.close)
        prompts = []

        def typed(prompt):
            prompts.append(prompt)
            return "192.168.1.77"

        parser = oc.build_parser()
        args = parser.parse_args(oc.normalize_argv([
            "--http-port", str(self.gw.port), "--mdns-target", f"127.0.0.1:{silent.port}",
            "--mdns-interface", "127.0.0.1", "--discover-seconds", "1", "--broker", ""]))
        out = io.StringIO()
        with mock.patch.object(oc, "load_settings", return_value={"DeviceHost": "10.9.9.9"}), \
                mock.patch.object(oc, "interactive", return_value=True), \
                mock.patch.object(oc, "read_gateway_mqtt", return_value=(None, "no answer")), \
                mock.patch("builtins.input", side_effect=typed), \
                contextlib.redirect_stdout(out):
            oc.resolve_inputs(args, parser)
        self.assertIn("No OTGW answered", out.getvalue())
        self.assertEqual(prompts, ["OTGW device host [10.9.9.9]: "])   # the old prompt, saved host as default
        self.assertEqual(args.device, "192.168.1.77")

    def test_h_explicit_device_skips_discovery(self):
        with mock.patch.object(oc, "discover_gateways", side_effect=AssertionError("ran")) as d:
            args, _ = self.resolve(["--device", "127.0.0.1"])
        d.assert_not_called()
        self.assertEqual(args.device, "127.0.0.1")

    def test_h_no_prompt_skips_discovery(self):
        with mock.patch.object(oc, "discover_gateways", side_effect=AssertionError("ran")) as d, \
                contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            self.resolve(["--no-prompt"])
        d.assert_not_called()


class DiscoverFlagCase(unittest.TestCase):
    def test_i_discover_lists_and_exits_without_capture(self):
        gw = FakeGateway(body=otgw_info("marvin-otgw32", "2.0.0-beta.120", "otgw32", "OT-Direct"))
        self.addCleanup(gw.close)
        responder = FakeResponder([mdns_reply("marvin-otgw32", "127.0.0.1")])
        self.addCleanup(responder.close)
        with tempfile.TemporaryDirectory() as tmp:
            run = subprocess.run(
                [sys.executable, str(TOOL), "--discover", "--http-port", str(gw.port),
                 "--mdns-target", f"127.0.0.1:{responder.port}", "--mdns-interface", "127.0.0.1",
                 "--discover-seconds", "1", "--output-root", str(Path(tmp) / "logs")],
                capture_output=True, text=True, timeout=60, stdin=subprocess.DEVNULL)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn("marvin-otgw32", run.stdout)
            self.assertIn("2.0.0-beta.120", run.stdout)
            self.assertIn("otgw32 OT-Direct", run.stdout)
            self.assertFalse((Path(tmp) / "logs").exists(), "a capture was started")


class FakeMqttGateway:
    """GET /api/v2/settings with the MQTT fields (the password only as "password=N", as in
    restAPI.ino) and GET /settings.ini with the clear-text password, or a status code."""

    def __init__(self, api_value="password=10", ini_password="s3cret-pw!", ini_status=200, user="otgw"):
        gw = self
        self.paths = []

        class H(BaseHTTPRequestHandler):
            def log_message(self, *a):
                pass

            def do_GET(self):
                gw.paths.append(self.path)
                status = 200
                if self.path == "/api/v2/settings":
                    body = {"settings": {
                        "mqttenable": {"value": True}, "mqttbroker": {"value": "192.168.2.101"},
                        "mqttbrokerport": {"value": 1883}, "mqttuser": {"value": user},
                        "mqttpasswd": {"value": api_value, "type": "p"}}}
                elif self.path == "/settings.ini":
                    status = ini_status
                    body = {"MQTTuser": user, "MQTTpasswd": ini_password} if status == 200 else {}
                else:
                    status, body = 404, {}
                data = json.dumps(body).encode()
                self.send_response(status)
                self.send_header("Content-Length", str(len(data)))
                self.end_headers()
                self.wfile.write(data)

        self.httpd = ThreadingHTTPServer(("127.0.0.1", 0), H)
        self.port = self.httpd.server_address[1]
        threading.Thread(target=self.httpd.serve_forever, daemon=True).start()

    def close(self):
        self.httpd.shutdown()
        self.httpd.server_close()


class PasswordCase(unittest.TestCase):
    def resolve(self, gw, saved=None, tty=False):
        parser = oc.build_parser()
        args = parser.parse_args(oc.normalize_argv(
            ["--device", "127.0.0.1", "--http-port", str(gw.port)] + ([] if tty else ["--no-prompt"])))
        out = io.StringIO()
        with mock.patch.object(oc, "load_settings", return_value=dict(saved or {})),                 mock.patch.object(oc, "interactive", return_value=tty),                 mock.patch("getpass.getpass", return_value="typed-pw") as typed,                 contextlib.redirect_stdout(out):
            oc.resolve_inputs(args, parser)
        return args, out.getvalue(), typed

    def gateway(self, **kw):
        gw = FakeMqttGateway(**kw)
        self.addCleanup(gw.close)
        return gw

    def test_j_api_reports_only_the_length(self):
        found, _ = oc.read_gateway_mqtt("127.0.0.1", self.gateway().port)
        self.assertEqual(found["password_length"], 10)
        found, _ = oc.read_gateway_mqtt("127.0.0.1", self.gateway(api_value="").port)
        self.assertIsNone(found["password_length"])

    def test_j_password_is_read_from_settings_ini(self):
        args, out, typed = self.resolve(self.gateway(), tty=True)
        self.assertEqual(args.password, "s3cret-pw!")
        typed.assert_not_called()
        self.assertIn("read from the gateway", out)
        self.assertNotIn("s3cret-pw!", out)                  # never printed

    def test_j_no_password_on_the_gateway_asks_nothing(self):
        gw = self.gateway(api_value="password=0", ini_password="")
        args, out, typed = self.resolve(gw, saved={"Username": "otgw", "MqttPassword": "old"}, tty=True)
        self.assertIsNone(args.password)
        typed.assert_not_called()
        self.assertNotIn("/settings.ini", gw.paths)
        self.assertIn("without a password", out)

    def test_j_length_mismatch_is_not_used(self):
        args, out, typed = self.resolve(self.gateway(ini_password="short"), tty=True)
        self.assertEqual(args.password, "typed-pw")
        typed.assert_called_once()
        self.assertIn("disagree on its length", out)

    def test_j_admin_password_falls_back_to_a_saved_one(self):
        gw = self.gateway(ini_status=401)
        args, _, typed = self.resolve(gw, saved={"Username": "otgw", "MqttPassword": "saved-pw-1"})
        self.assertEqual(args.password, "saved-pw-1")         # same length as password=10
        typed.assert_not_called()

    def test_j_admin_password_and_a_wrong_saved_length_prompts(self):
        args, out, typed = self.resolve(self.gateway(ini_status=401),
                                        saved={"Username": "otgw", "MqttPassword": "x"}, tty=True)
        self.assertEqual(args.password, "typed-pw")
        self.assertIn("401", out)


if __name__ == "__main__":
    unittest.main(verbosity=2)
