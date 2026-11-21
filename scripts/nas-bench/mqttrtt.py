#!/usr/bin/env python3
"""mqttrtt.py <broker> <uid> <outfile> [n] [gap_s]: command round trip set -> state for one OTGW unit (stdlib only).

One MQTT 3.1.1 connection does both sides, so both timestamps come from the same time.monotonic() clock:
publish OTGW/set/<uid>/sat/target (alternating 20.5 / 20.0), then wait for OTGW/value/<uid>/sat/climate_target
to carry that value. Also measures PINGREQ -> PINGRESP to the broker (the broker's own share of the time).
Writes one JSON line per sample to <outfile> and prints p50/p95/max. Needs SAT enabled on the unit.
"""
import json
import socket
import struct
import sys
import time

BROKER, UID, OUT = sys.argv[1], sys.argv[2], sys.argv[3]
N = int(sys.argv[4]) if len(sys.argv) > 4 else 20
GAP = float(sys.argv[5]) if len(sys.argv) > 5 else 8.0
SET_T = "OTGW/set/%s/sat/target" % UID
STATE_T = "OTGW/value/%s/sat/climate_target" % UID


def remlen(n):
    out = b""
    while True:
        b = n % 128
        n //= 128
        out += bytes([b | (0x80 if n else 0)])
        if not n:
            return out


def mstr(s):
    b = s.encode()
    return struct.pack(">H", len(b)) + b


def recv_packet(sock):
    """One MQTT packet: (type, payload bytes)."""
    h = sock.recv(1)
    if not h:
        raise EOFError
    mult, n = 1, 0
    while True:
        b = sock.recv(1)[0]
        n += (b & 0x7F) * mult
        mult *= 128
        if not b & 0x80:
            break
    data = b""
    while len(data) < n:
        chunk = sock.recv(n - len(data))
        if not chunk:
            raise EOFError
        data += chunk
    return h[0] >> 4, data


def publish_value(data):
    tl = struct.unpack(">H", data[:2])[0]
    return data[2:2 + tl].decode(errors="replace"), data[2 + tl:].decode(errors="replace")


s = socket.create_connection((BROKER, 1883), timeout=30)
s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)       # the probe itself must not add Nagle delay
cid = "otgwbench-rtt-%d" % int(time.time())
var = mstr("MQTT") + bytes([4, 2]) + struct.pack(">H", 60)
s.sendall(bytes([0x10]) + remlen(len(var) + len(mstr(cid)))+ var + mstr(cid))
assert recv_packet(s)[0] == 2, "no CONNACK"
sub = struct.pack(">H", 1) + mstr(STATE_T) + bytes([0])
s.sendall(bytes([0x82]) + remlen(len(sub)) + sub)

current = None
deadline = time.monotonic() + 10
while time.monotonic() < deadline:                            # SUBACK, then the retained state
    t, d = recv_packet(s)
    if t == 3:
        current = publish_value(d)[1]
        break
f = open(OUT, "a")
rtts, pings = [], []
for i in range(N):
    target = "20.50" if (current or "20.00").startswith("20.0") else "20.00"
    pl = mstr(SET_T) + target[:4].encode()
    t0 = time.monotonic()
    s.sendall(bytes([0x30]) + remlen(len(pl)) + pl)
    got = None
    while time.monotonic() - t0 < 10:
        t, d = recv_packet(s)
        if t == 3:
            topic, val = publish_value(d)
            if topic == STATE_T and abs(float(val) - float(target)) < 0.001:
                got = time.monotonic() - t0
                current = val
                break
    p0 = time.monotonic()
    s.sendall(bytes([0xC0, 0]))
    while True:
        t, d = recv_packet(s)
        if t == 13:
            ping = time.monotonic() - p0
            break
    rec = {"t": time.strftime("%F %T"), "i": i, "target": target, "rtt_ms": None if got is None else round(got * 1000, 1),
           "ping_ms": round(ping * 1000, 1)}
    f.write(json.dumps(rec) + "\n")
    f.flush()
    if got is not None:
        rtts.append(got * 1000)
    pings.append(ping * 1000)
    time.sleep(GAP)


def pct(a, p):
    a = sorted(a)
    return round(a[min(len(a) - 1, int(round(p / 100.0 * (len(a) - 1))))], 1) if a else None


print(json.dumps({"n": N, "answered": len(rtts), "rtt_p50": pct(rtts, 50), "rtt_p95": pct(rtts, 95), "rtt_max": pct(rtts, 100),
                  "ping_p50": pct(pings, 50), "ping_p95": pct(pings, 95)}))
