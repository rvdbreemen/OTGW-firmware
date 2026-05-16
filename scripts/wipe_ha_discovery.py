#!/usr/bin/env python3
"""Standalone wiper for OTGW Home Assistant MQTT discovery (config) topics.

Clears the *retained* Home Assistant discovery topics that an OTGW publishes
(`<ha_prefix>/<component>/<unique_id>/.../config`). Home Assistant removes the
matching entities once the retained config payloads are gone.

The device identity (unique_id / ha_prefix) and the broker address are
discovered automatically through the OTGW REST API (default host
``OTGW.local``). CLI flags override every discovered value.

Stdlib only - no pip install required. The MQTT 3.1.1 client below speaks just
the subset needed: CONNECT / SUBSCRIBE / PUBLISH / DISCONNECT.

Scope safety: only retained topics whose path ends in ``config`` AND that
contain the resolved unique_id as a path segment are touched, so other devices
sharing the broker are never affected.
"""

import argparse
import base64
import getpass
import json
import os
import socket
import struct
import sys
import urllib.error
import urllib.request

DEFAULT_HOST = "OTGW.local"
DEFAULT_HA_PREFIX = "homeassistant"
DEFAULT_MQTT_PORT = 1883


# --------------------------------------------------------------------------
# REST discovery
# --------------------------------------------------------------------------
def _http_get_json(url, http_user, http_pass, timeout):
    req = urllib.request.Request(url)
    if http_pass:
        token = base64.b64encode(
            f"{http_user}:{http_pass}".encode("utf-8")
        ).decode("ascii")
        req.add_header("Authorization", f"Basic {token}")
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        raw = resp.read().decode("utf-8", "replace")
    return json.loads(raw)


def discover_from_rest(host, http_user, http_pass, timeout):
    """Return a dict with any of unique_id/ha_prefix/broker/port/mqtt_user
    that could be read from the OTGW REST API. Best effort - missing keys are
    simply absent from the returned dict."""
    base = f"http://{host}"
    found = {}

    # /api/v2/debug - flat keys, has everything (auth-protected only if an
    # HTTP password is configured on the OTGW).
    try:
        dbg = _http_get_json(
            f"{base}/api/v2/debug", http_user, http_pass, timeout
        ).get("debug", {})
        if dbg.get("settings.mqtt.unique_id"):
            found["unique_id"] = str(dbg["settings.mqtt.unique_id"])
        if dbg.get("settings.mqtt.ha_prefix"):
            found["ha_prefix"] = str(dbg["settings.mqtt.ha_prefix"])
        if dbg.get("settings.mqtt.broker"):
            found["broker"] = str(dbg["settings.mqtt.broker"])
        if dbg.get("settings.mqtt.port"):
            found["port"] = int(dbg["settings.mqtt.port"])
        if dbg.get("settings.mqtt.user"):
            found["mqtt_user"] = str(dbg["settings.mqtt.user"])
    except (urllib.error.URLError, ValueError, OSError, KeyError):
        pass

    # /api/v2/settings - nested {"value": ...} form, same auth rules.
    if not {"unique_id", "ha_prefix", "broker"} <= found.keys():
        try:
            st = _http_get_json(
                f"{base}/api/v2/settings", http_user, http_pass, timeout
            ).get("settings", {})

            def sval(key):
                v = st.get(key)
                return v.get("value") if isinstance(v, dict) else None

            if "unique_id" not in found and sval("mqttuniqueid"):
                found["unique_id"] = str(sval("mqttuniqueid"))
            if "ha_prefix" not in found and sval("mqtthaprefix"):
                found["ha_prefix"] = str(sval("mqtthaprefix"))
            if "broker" not in found and sval("mqttbroker"):
                found["broker"] = str(sval("mqttbroker"))
            if "port" not in found and sval("mqttbrokerport") is not None:
                found["port"] = int(sval("mqttbrokerport"))
            if "mqtt_user" not in found and sval("mqttuser"):
                found["mqtt_user"] = str(sval("mqttuser"))
        except (urllib.error.URLError, ValueError, OSError, KeyError):
            pass

    # /api/v2/device/info - never auth-protected. Gives hostname + MAC so we
    # can reconstruct the firmware's default unique_id (otgw-<MAC>).
    try:
        dev = _http_get_json(
            f"{base}/api/v2/device/info", http_user, http_pass, timeout
        ).get("device", {})
        if dev.get("hostname"):
            found["hostname"] = str(dev["hostname"])
        if "unique_id" not in found and dev.get("macaddress"):
            mac = str(dev["macaddress"]).replace(":", "").upper()
            if mac:
                found["unique_id"] = f"otgw-{mac}"
                found["unique_id_is_default"] = True
    except (urllib.error.URLError, ValueError, OSError, KeyError):
        pass

    return found


# --------------------------------------------------------------------------
# Minimal MQTT 3.1.1 client (stdlib sockets only)
# --------------------------------------------------------------------------
class MqttError(Exception):
    pass


def _enc_str(s):
    b = s.encode("utf-8")
    return struct.pack("!H", len(b)) + b


def _enc_remaining_length(n):
    out = bytearray()
    while True:
        digit = n & 0x7F
        n >>= 7
        if n:
            digit |= 0x80
        out.append(digit)
        if not n:
            break
    return bytes(out)


class MiniMQTT:
    def __init__(self, host, port, timeout):
        self._timeout = timeout
        self._sock = socket.create_connection((host, port), timeout=timeout)
        self._buf = b""

    def _send(self, data):
        self._sock.sendall(data)

    def _recv_exact(self, n):
        while len(self._buf) < n:
            chunk = self._sock.recv(4096)
            if not chunk:
                raise MqttError("broker closed the connection")
            self._buf += chunk
        out, self._buf = self._buf[:n], self._buf[n:]
        return out

    def _recv_remaining_length(self):
        mult = 1
        value = 0
        while True:
            (byte,) = struct.unpack("!B", self._recv_exact(1))
            value += (byte & 0x7F) * mult
            if not (byte & 0x80):
                return value
            mult *= 128
            if mult > 128 ** 3:
                raise MqttError("malformed remaining length")

    def connect(self, client_id, username=None, password=None, keepalive=60):
        flags = 0x02  # clean session
        payload = _enc_str(client_id)
        if username:
            flags |= 0x80
            payload += _enc_str(username)
            if password is not None:
                flags |= 0x40
                pw = password.encode("utf-8")
                payload += struct.pack("!H", len(pw)) + pw
        var_header = _enc_str("MQTT") + struct.pack("!BBH", 0x04, flags, keepalive)
        body = var_header + payload
        self._send(b"\x10" + _enc_remaining_length(len(body)) + body)

        (b1,) = struct.unpack("!B", self._recv_exact(1))
        if b1 != 0x20:
            raise MqttError(f"expected CONNACK, got control byte 0x{b1:02x}")
        rem = self._recv_remaining_length()
        ack = self._recv_exact(rem)
        if len(ack) < 2 or ack[1] != 0x00:
            code = ack[1] if len(ack) >= 2 else -1
            raise MqttError(f"broker refused connection (CONNACK code {code})")

    def subscribe(self, topic_filter, qos=0):
        pkt_id = 1
        body = struct.pack("!H", pkt_id) + _enc_str(topic_filter) + bytes([qos])
        self._send(b"\x82" + _enc_remaining_length(len(body)) + body)
        (b1,) = struct.unpack("!B", self._recv_exact(1))
        if b1 != 0x90:
            raise MqttError(f"expected SUBACK, got control byte 0x{b1:02x}")
        rem = self._recv_remaining_length()
        ack = self._recv_exact(rem)
        if ack and ack[-1] == 0x80:
            raise MqttError("broker rejected the subscription")

    def collect_retained(self, idle_timeout):
        """Return {topic: payload_bytes} for every retained message the broker
        delivers. Stops after idle_timeout seconds without a new packet."""
        retained = {}
        self._sock.settimeout(idle_timeout)
        try:
            while True:
                try:
                    (b1,) = struct.unpack("!B", self._recv_exact(1))
                except (socket.timeout, TimeoutError):
                    break
                rem = self._recv_remaining_length()
                body = self._recv_exact(rem) if rem else b""
                if (b1 >> 4) != 3:  # only care about PUBLISH
                    continue
                retain = b1 & 0x01
                qos = (b1 >> 1) & 0x03
                (tlen,) = struct.unpack("!H", body[:2])
                topic = body[2 : 2 + tlen].decode("utf-8", "replace")
                rest = body[2 + tlen :]
                if qos > 0:
                    pkt_id = rest[:2]
                    rest = rest[2:]
                    self._send(b"\x40\x02" + pkt_id)  # PUBACK (QoS 1)
                if retain:
                    retained[topic] = rest
        finally:
            self._sock.settimeout(self._timeout)
        return retained

    def publish_clear_retained(self, topic):
        """Publish a zero-length retained message (QoS 0) - this deletes the
        retained value for the topic on the broker."""
        body = _enc_str(topic)  # no packet id (QoS 0), empty payload
        self._send(b"\x31" + _enc_remaining_length(len(body)) + body)

    def disconnect(self):
        try:
            self._send(b"\xe0\x00")
        except OSError:
            pass
        try:
            self._sock.close()
        except OSError:
            pass


# --------------------------------------------------------------------------
# Topic matching
# --------------------------------------------------------------------------
def is_device_config_topic(topic, ha_prefix, unique_id):
    segs = topic.split("/")
    return (
        len(segs) >= 3
        and segs[0] == ha_prefix
        and segs[-1] == "config"
        and unique_id in segs
    )


# --------------------------------------------------------------------------
# CLI
# --------------------------------------------------------------------------
def build_parser():
    p = argparse.ArgumentParser(
        prog="wipe_ha_discovery",
        description="Wipe retained Home Assistant MQTT discovery (config) "
        "topics for one OTGW device.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    p.add_argument(
        "--host",
        default=DEFAULT_HOST,
        help="OTGW hostname/IP for REST auto-discovery",
    )
    p.add_argument("--http-user", default="admin", help="OTGW HTTP auth user")
    p.add_argument(
        "--http-pass",
        default=None,
        help="OTGW HTTP auth password (only if one is set on the OTGW)",
    )
    p.add_argument("--broker", default=None, help="MQTT broker host (overrides REST)")
    p.add_argument("--port", type=int, default=None, help="MQTT broker port")
    p.add_argument("--mqtt-user", default=None, help="MQTT username")
    p.add_argument(
        "--mqtt-pass",
        default=None,
        help="MQTT password (prompted if a user is set and this is omitted)",
    )
    p.add_argument(
        "--unique-id",
        default=None,
        help="Device unique_id / node id (overrides REST discovery)",
    )
    p.add_argument(
        "--ha-prefix",
        default=None,
        help="Home Assistant discovery prefix (overrides REST discovery)",
    )
    p.add_argument(
        "--rest-timeout", type=float, default=5.0, help="REST request timeout (s)"
    )
    p.add_argument(
        "--mqtt-timeout",
        type=float,
        default=10.0,
        help="MQTT connect/network timeout (s)",
    )
    p.add_argument(
        "--collect-timeout",
        type=float,
        default=3.0,
        help="Idle seconds to wait for retained messages after subscribe",
    )
    p.add_argument(
        "--dry-run",
        action="store_true",
        help="List matching topics and exit without wiping",
    )
    p.add_argument(
        "-y",
        "--yes",
        action="store_true",
        help="Skip the interactive confirmation dialog (non-interactive use)",
    )
    return p


def confirm(matched, ctx):
    print()
    print("About to WIPE retained Home Assistant discovery topics:")
    print(f"  OTGW host    : {ctx['host']}")
    print(f"  MQTT broker  : {ctx['broker']}:{ctx['port']}")
    print(f"  HA prefix    : {ctx['ha_prefix']}")
    print(f"  Device id    : {ctx['unique_id']}{ctx['id_note']}")
    print(f"  Topics found : {len(matched)}")
    print()
    for t in matched:
        print(f"    - {t}")
    print()
    print(
        "This permanently removes these retained config topics from the "
        "broker.\nHome Assistant will drop the matching entities."
    )
    try:
        answer = input('Type "WIPE" to confirm, anything else to abort: ')
    except (EOFError, KeyboardInterrupt):
        print()
        return False
    return answer.strip() == "WIPE"


def main(argv=None):
    args = build_parser().parse_args(argv)

    print(f"Discovering OTGW configuration via REST at http://{args.host} ...")
    rest = {}
    try:
        rest = discover_from_rest(
            args.host, args.http_user, args.http_pass, args.rest_timeout
        )
    except socket.gaierror:
        print(
            f"  Could not resolve '{args.host}'. If mDNS (.local) is "
            "unavailable, pass --host <ip-address>.",
            file=sys.stderr,
        )

    unique_id = args.unique_id or rest.get("unique_id")
    ha_prefix = args.ha_prefix or rest.get("ha_prefix") or DEFAULT_HA_PREFIX
    broker = args.broker or rest.get("broker")
    port = args.port or rest.get("port") or DEFAULT_MQTT_PORT
    mqtt_user = args.mqtt_user or rest.get("mqtt_user")

    id_note = ""
    if not args.unique_id and rest.get("unique_id_is_default"):
        id_note = "  (derived default from MAC; pass --unique-id if customised)"

    if rest.get("hostname"):
        print(f"  OTGW hostname : {rest['hostname']}")
    if not unique_id:
        print(
            "ERROR: could not determine the device unique_id from REST. "
            "Provide it explicitly with --unique-id.",
            file=sys.stderr,
        )
        return 2
    if not broker:
        print(
            "ERROR: could not determine the MQTT broker from REST "
            "(the debug/settings endpoint may be HTTP-auth protected). "
            "Provide --broker (and --http-pass to read it via REST).",
            file=sys.stderr,
        )
        return 2

    print(f"  unique_id     : {unique_id}{id_note}")
    print(f"  ha_prefix     : {ha_prefix}")
    print(f"  broker        : {broker}:{port}")

    mqtt_pass = args.mqtt_pass
    if mqtt_user and mqtt_pass is None and not args.yes:
        try:
            mqtt_pass = getpass.getpass(f"MQTT password for '{mqtt_user}': ")
        except (EOFError, KeyboardInterrupt):
            print()
            return 1

    print(f"Connecting to MQTT broker {broker}:{port} ...")
    try:
        client = MiniMQTT(broker, port, args.mqtt_timeout)
        client.connect(
            client_id="otgwwipe-" + os.urandom(3).hex(),
            username=mqtt_user,
            password=mqtt_pass,
        )
    except (OSError, MqttError) as exc:
        print(f"ERROR: MQTT connection failed: {exc}", file=sys.stderr)
        return 3

    try:
        sub = f"{ha_prefix}/#"
        print(f"Subscribing to '{sub}' and collecting retained topics ...")
        client.subscribe(sub)
        retained = client.collect_retained(args.collect_timeout)
        matched = sorted(
            t for t in retained if is_device_config_topic(t, ha_prefix, unique_id)
        )

        if not matched:
            print(
                f"No retained discovery topics found for '{unique_id}' "
                f"under '{ha_prefix}/'. Nothing to wipe."
            )
            return 0

        ctx = {
            "host": args.host,
            "broker": broker,
            "port": port,
            "ha_prefix": ha_prefix,
            "unique_id": unique_id,
            "id_note": id_note,
        }

        if args.dry_run:
            print(f"\n[dry-run] {len(matched)} topic(s) would be wiped:")
            for t in matched:
                print(f"    - {t}")
            return 0

        if not args.yes:
            if not confirm(matched, ctx):
                print("Aborted - nothing was changed.")
                return 1

        wiped = 0
        for t in matched:
            client.publish_clear_retained(t)
            wiped += 1
        print(f"Wiped {wiped} retained discovery topic(s) for '{unique_id}'.")
        return 0
    except (OSError, MqttError) as exc:
        print(f"ERROR: MQTT operation failed: {exc}", file=sys.stderr)
        return 3
    finally:
        client.disconnect()


if __name__ == "__main__":
    sys.exit(main())
