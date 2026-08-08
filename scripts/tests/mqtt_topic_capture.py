#!/usr/bin/env python3
"""Collect published (topic, payload) pairs straight from the MQTT broker.

The coverage gate's OT half is read from the debug telnet, which is fine: with
MQTT debug off that stream is quiet enough to carry every decode line. The topic
half is not fine to read there. The telnet drops output under a publish burst, so
a topic's presence in a capture is a sample rather than an observation: two
identical runs differed by 9 topics, with the smaller set a strict subset of the
larger. A broker subscription sees every publish the firmware makes, so presence
becomes observable and the gate can hold it to exact equality.

MQTT 3.1.1 over plain TCP, hand-rolled against stdlib sockets. The rest of this
test rig is stdlib-only and a subscriber needs four packet types, so pulling in
paho for CONNECT/SUBSCRIBE/PUBLISH/DISCONNECT is not worth a dependency.

    python mqtt_topic_capture.py --seconds 60          # prints the topics it saw

Credentials come from scripts/_secrets.py, i.e. the out-of-repo
capture-settings.json or the OTGW_* environment variables. Nothing is read from,
or written to, the repository.
"""
from __future__ import annotations

import argparse
import os
import socket
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
try:
    import _secrets
except Exception:                                     # noqa: BLE001 - optional
    _secrets = None

CONNECT, CONNACK = 0x10, 0x20
PUBLISH, SUBSCRIBE, SUBACK = 0x30, 0x82, 0x90
PINGREQ, PINGRESP, DISCONNECT = 0xC0, 0xD0, 0xE0

CONNACK_ERRORS = {
    1: "unacceptable protocol version",
    2: "identifier rejected",
    3: "server unavailable",
    4: "bad username or password",
    5: "not authorised",
}


def topic_filter_for(root: str | None) -> str:
    """Build a subscribe filter from a top topic, tolerating a wildcard root.

    '#' is only legal as the final character of a filter, so naively formatting
    '<root>/#' produces '#/#' when the stored root is itself a wildcard. Brokers
    answer an illegal filter by closing the connection with no error packet,
    which surfaces as a bare 'broker closed the connection' several frames later.
    The stored bench root really is '#', so this is not hypothetical.
    """
    root = (root or "").strip().rstrip("/")
    if not root or "#" in root or "+" in root:
        return "#"
    return f"{root}/#"


def _varint(n: int) -> bytes:
    """MQTT remaining-length encoding: 7 bits per byte, high bit = continuation."""
    out = bytearray()
    while True:
        digit = n % 128
        n //= 128
        if n:
            digit |= 0x80
        out.append(digit)
        if not n:
            return bytes(out)


def _str(s: str) -> bytes:
    raw = s.encode("utf-8")
    return len(raw).to_bytes(2, "big") + raw


class BrokerReader:
    """Minimal subscribe-only MQTT client.

    Keepalive is set to 0 (disabled). The capture windows here are minutes, not
    hours, and a disabled keepalive removes the PINGREQ timer that would
    otherwise be the only thing this client has to do on a schedule. PINGRESP is
    still tolerated on the read path in case a broker pings first.
    """

    def __init__(self, host: str, port: int, user: str, password: str,
                 client_id: str | None = None):
        # Unique per run. A broker kicks the existing session when a second
        # client connects with the same identifier, so a fixed id makes two gate
        # runs (or a leftover session from a crashed one) silently disconnect
        # each other mid-capture.
        client_id = client_id or f"otgw-gate-{os.getpid()}-{int(time.time())}"
        self.sock = socket.create_connection((host, port), timeout=10)
        self.sock.settimeout(1.0)
        self.buf = bytearray()
        self._connect(user, password, client_id)

    def _connect(self, user: str, password: str, client_id: str) -> None:
        flags = 0x02                                   # clean session
        payload = _str(client_id)
        if user:
            flags |= 0x80
            payload += _str(user)
            if password:
                flags |= 0x40
                payload += _str(password)
        variable = _str("MQTT") + bytes([0x04, flags]) + (0).to_bytes(2, "big")
        body = variable + payload
        self.sock.sendall(bytes([CONNECT]) + _varint(len(body)) + body)

        kind, data = self._packet(timeout=10)
        if kind != CONNACK:
            raise RuntimeError(f"expected CONNACK, got packet type 0x{kind:02X}")
        code = data[1] if len(data) > 1 else 0xFF
        if code != 0:
            raise RuntimeError(f"broker refused the connection: "
                               f"{CONNACK_ERRORS.get(code, f'code {code}')}")

    def subscribe(self, topic_filter: str) -> None:
        body = (1).to_bytes(2, "big") + _str(topic_filter) + bytes([0])   # QoS 0
        self.sock.sendall(bytes([SUBSCRIBE]) + _varint(len(body)) + body)
        kind, _ = self._packet(timeout=10)
        if kind != SUBACK:
            raise RuntimeError(f"expected SUBACK, got packet type 0x{kind:02X}")

    def _fill(self, want: int, deadline: float) -> bool:
        while len(self.buf) < want:
            if time.time() > deadline:
                return False
            try:
                chunk = self.sock.recv(4096)
            except socket.timeout:
                continue
            if not chunk:
                raise RuntimeError("broker closed the connection")
            self.buf += chunk
        return True

    def _packet(self, timeout: float):
        """Read one control packet. Returns (type_byte, body) or (None, None)."""
        deadline = time.time() + timeout
        if not self._fill(1, deadline):
            return None, None
        header = self.buf[0]
        length, shift, offset = 0, 0, 1
        while True:
            if not self._fill(offset + 1, deadline):
                return None, None
            digit = self.buf[offset]
            length |= (digit & 0x7F) << shift
            offset += 1
            if not digit & 0x80:
                break
            shift += 7
            if shift > 21:
                raise RuntimeError("malformed remaining length")
        if not self._fill(offset + length, deadline):
            return None, None
        body = bytes(self.buf[offset:offset + length])
        del self.buf[:offset + length]
        return header & 0xF0, body

    def collect(self, seconds: float, into: list, only: str | None = None) -> int:
        """Append (topic, payload) for every PUBLISH seen within the window.

        `only` is a required substring, normally the device's uniqueid. A broker
        usually carries more than one OTGW: this bench sees otgw-AC276ECE45D8
        (under test) alongside otgw-2CF43257D77C (production). normalise_topic()
        strips the uniqueid to keep baselines portable between benches, so
        without this filter both devices collapse onto the same topic names and
        the fingerprint records another unit's behaviour as if it were the
        device under test.
        """
        seen = 0
        stop = time.time() + seconds
        while time.time() < stop:
            kind, body = self._packet(timeout=max(0.1, stop - time.time()))
            if kind is None:
                continue
            if kind == PINGREQ:
                self.sock.sendall(bytes([PINGRESP, 0x00]))
                continue
            if kind != PUBLISH:
                continue
            tlen = int.from_bytes(body[:2], "big")
            topic = body[2:2 + tlen].decode("utf-8", "replace")
            if only and only not in topic:
                continue
            # QoS 0 only (we subscribed at QoS 0), so no packet identifier.
            payload = body[2 + tlen:].decode("utf-8", "replace")
            into.append((topic, payload))
            seen += 1
        return seen

    def flush(self, seconds: float = 2.0) -> int:
        """Discard everything published so far, so the next collect() starts clean.

        The subscription goes live at open_reader(), but the caller still has to
        upload the fixture, start the simulation and wait out a preflight before
        the window it actually wants to measure begins. The broker keeps sending
        throughout, and TCP buffers it, so without this the first reads of
        collect() return a backlog from before the window. That is how an 'OFF'
        published while the device sat idle between runs ended up in a capture
        whose simulation had been running the whole time.
        """
        junk: list = []
        return self.collect(seconds, junk)

    def close(self) -> None:
        try:
            self.sock.sendall(bytes([DISCONNECT, 0x00]))
        except OSError:
            pass
        self.sock.close()


def open_reader(topic_filter: str, host: str | None = None, port: int | None = None,
                user: str | None = None, only: str | None = None,
                retained_drain_s: float = 5.0):
    """Connect, subscribe, and swallow the retained flush.

    Subscribing delivers every retained message immediately, which for this
    broker means discovery configs and any retained state left by earlier runs or
    other firmware. Those are not observations of THIS run. MQTT 3.1.1 has no way
    to decline them (retain handling arrived in MQTT 5), so drain and discard for
    a few seconds before the caller starts the simulation.

    Pass host/port/user to watch a specific broker. The gate passes the values it
    reads from the device under test, which is the only way to be sure it is
    watching the broker the device actually publishes to: _secrets' stored
    BrokerHost is a bench convenience and goes stale when the bench moves subnet
    (it pointed at an unreachable 192.168.1.11 while the device was happily
    publishing to homeassistant.local on 192.168.88.x).
    """
    if host is None or port is None:
        if _secrets is None:
            raise RuntimeError("no broker given and scripts/_secrets.py is not "
                               "importable")
        s_host, s_port, _ = _secrets.resolve_broker()
        host, port = host or s_host, port or s_port
    if user is None:
        user = (_secrets.get("mqtt_user", "") if _secrets else "") or ""
    password = _secrets.mqtt_password() if _secrets else ""
    if not password:
        raise RuntimeError(
            "no MQTT password available. Put MqttPassword in the out-of-repo "
            "capture-settings.json (capture-mqtt-debug.bat -SaveSecrets writes "
            "it) or set OTGW_MQTT_PASSWORD.")
    reader = BrokerReader(host, int(port), user, password)
    reader.subscribe(topic_filter)
    drained: list = []
    reader.collect(retained_drain_s, drained, only)
    return reader, host, int(port), len(drained)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--seconds", type=int, default=60)
    ap.add_argument("--filter", default=None,
                    help="topic filter (default: <Topic from secrets>/#)")
    ap.add_argument("--broker", default=None, help="broker host (overrides secrets)")
    ap.add_argument("--port", type=int, default=None)
    ap.add_argument("--user", default=None)
    ap.add_argument("--only", default=None,
                    help="required substring, normally the device uniqueid "
                         "(e.g. otgw-AC276ECE45D8); other devices on the same "
                         "broker are ignored")
    args = ap.parse_args()

    topic_filter = args.filter or topic_filter_for(
        _secrets.get("topic") if _secrets else None)
    reader, host, port, drained = open_reader(topic_filter, args.broker,
                                              args.port, args.user, args.only)
    print(f"broker   : {host}:{port}")
    print(f"filter   : {topic_filter}")
    print(f"retained : {drained} message(s) drained and discarded")
    pairs: list = []
    try:
        n = reader.collect(args.seconds, pairs, args.only)
    finally:
        reader.close()
    topics = sorted({t for t, _ in pairs})
    print(f"observed : {n} publishes, {len(topics)} distinct topics")
    for t in topics:
        print(f"  {t}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
