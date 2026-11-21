#!/usr/bin/env python3
"""mqttcfg.py <unit-ip> <run-dir>: write the MQTT broker settings of an OTGW unit to <run-dir>/.mqtt.env (mode 600, removed by bench.sh once docker has read it).

The bench subscribes to the broker the unit itself publishes to (Robert, 2026-11-21: the Home Assistant broker,
host and credentials as in the unit's settings.ini). Reads http://<unit-ip>/settings.ini (JSON, keys MQTTbroker,
MQTTbrokerPort, MQTTuser, MQTTpasswd). A .local host name is resolved with one multicast DNS query, because the NAS
resolver has no mDNS (nsswitch: files dns). Prints host, IP, port and user; never the password.
"""
import json
import os
import socket
import struct
import sys
import urllib.request


def mdns_a(name, timeout=3.0):
    """IPv4 address of a .local name via one mDNS query (RFC 6762), or None."""
    q = struct.pack(">HHHHHH", 0, 0, 1, 0, 0, 0)
    for label in name.rstrip(".").split("."):
        q += bytes([len(label)]) + label.encode()
    q += b"\x00" + struct.pack(">HH", 1, 0x8001)          # type A, class IN with the unicast-response bit
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    s.settimeout(timeout)
    s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 255)
    try:
        s.sendto(q, ("224.0.0.251", 5353))
        while True:
            data, _ = s.recvfrom(4096)
            ans = struct.unpack(">H", data[6:8])[0]
            i = 12
            for _ in range(struct.unpack(">H", data[4:6])[0]):   # skip questions
                while data[i] and data[i] < 0xC0:
                    i += data[i] + 1
                i += (2 if data[i] >= 0xC0 else 1) + 4
            for _ in range(ans):
                while data[i] and data[i] < 0xC0:
                    i += data[i] + 1
                i += 2 if data[i] >= 0xC0 else 1
                rtype, _cls, _ttl, rdlen = struct.unpack(">HHIH", data[i:i + 10])
                i += 10
                if rtype == 1 and rdlen == 4:
                    return socket.inet_ntoa(data[i:i + 4])
                i += rdlen
    except (socket.timeout, OSError, IndexError, struct.error):
        return None
    finally:
        s.close()


def resolve(host):
    try:
        return socket.gethostbyname(host)
    except OSError:
        return mdns_a(host) if host.lower().endswith(".local") else None


def main():
    ip, run = sys.argv[1], sys.argv[2]
    with urllib.request.urlopen(f"http://{ip}/settings.ini", timeout=10) as r:
        cfg = json.loads(r.read().decode("utf-8"))
    host = str(cfg.get("MQTTbroker", "")).strip()
    port = int(cfg.get("MQTTbrokerPort") or 1883)
    user = str(cfg.get("MQTTuser", ""))
    pw = str(cfg.get("MQTTpasswd", ""))
    if not host:
        sys.exit("unit has no MQTTbroker in settings.ini")
    addr = resolve(host)
    if not addr:
        sys.exit(f"cannot resolve broker host {host}")
    path = os.path.join(run, ".mqtt.env")   # bench.sh removes it right after docker run
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    with os.fdopen(fd, "w") as f:
        f.write(f"MQTT_HOST={addr}\nMQTT_PORT={port}\nMQTT_USER={user}\nMQTT_PASS={pw}\n")
    print(f"broker {host} -> {addr}:{port} user={user or '-'} password={'set' if pw else 'none'}")


if __name__ == "__main__":
    main()
