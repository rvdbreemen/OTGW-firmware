#!/usr/bin/env python3
"""Active MQTT-reliability test for TASK-871 (single HA device) + TASK-874 (birth
re-assert). Runs against whichever broker the live device is actually publishing
to (auto-detected), so it works against both the real HA broker and the laptop
test-rig (192.168.1.234).

What it asserts
---------------
TASK-871 AC#7  (topology):
  Retained homeassistant/.../config discovery shows exactly ONE primary HA device
  (identifiers == nodeId) for every non-BLE entity, and any via_device present
  points back to that same nodeId (the ADR-148-sanctioned BLE child-device path).
  Reports the uniq_id source-prefix split (otd_/pic_/esp_/sat_).

TASK-874 AC#2  (self-heal of a stranded birth):
  The availability topic <topTopic>/value/<nodeId> is the HA avty_t. We inject a
  retained "offline" on it (simulating a dropped birth / stranded retained LWT)
  and assert the firmware re-asserts "online" within the 300s re-assert window
  (publishBirthOnline()'s DECLARE_TIMER_SEC(timerMQTTbirthreassert, 300)).

TASK-874 AC#1  (rebirth on reconnect) -- only with --bounce against the LOCAL
  docker test-rig: restart the mosquitto container, reconnect, and assert the
  device republishes "online" promptly on its MQTT reconnect (onMqttConnect ->
  publishBirthOnline).

NOT covered here: TASK-873 (boot-time DNS-miss recovery). That path only fires
when the broker is a NAME that fails to resolve at boot; the rig uses a bare IP,
so it is not exercisable without a toggleable DNS responder. Maintainer waived it.

Usage (typically via the otgw-test.py orchestrator, or standalone):
  python scripts/tests/test_mqtt_reliability.py
  python scripts/tests/test_mqtt_reliability.py --broker 192.168.1.234 --top-topic OTGW
  python scripts/tests/test_mqtt_reliability.py --bounce        # rig-only rebirth test
  python scripts/tests/test_mqtt_reliability.py --self-heal-timeout 320
"""
import argparse
import json
import os
import socket
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
SCRIPTS = os.path.dirname(HERE)
sys.path.insert(0, SCRIPTS)
import _secrets  # noqa: E402

try:
    import paho.mqtt.client as mqtt
except ImportError:
    sys.exit("paho-mqtt not installed: pip install paho-mqtt")


def _new_client(client_id):
    """paho 2.x needs CallbackAPIVersion; fall back to the 1.x signature."""
    try:
        return mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=client_id)
    except (AttributeError, TypeError):
        return mqtt.Client(client_id=client_id)


def _resolvable(host):
    try:
        socket.gethostbyname(host)
        return True
    except OSError:
        return False


class Collector:
    """Connect to a broker, harvest retained topics under the given filters."""

    def __init__(self, host, port, user, password, filters):
        self.host = host
        self.port = port
        self.filters = filters
        self.retained = {}          # topic -> payload (str)
        self._connected = False
        self.client = _new_client(f"otgw-reltest-{os.getpid()}")
        if user:
            self.client.username_pw_set(user, password)
        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message

    def _on_connect(self, client, userdata, flags, reason_code, properties=None):
        self._connected = True
        for f in self.filters:
            client.subscribe(f, qos=0)

    def _on_message(self, client, userdata, msg):
        # Retained-only harvest: paho delivers retained msgs with .retain True on
        # the initial subscribe burst. Non-retained live traffic also lands here;
        # we keep the latest payload per topic either way (the avty topic's live
        # value is exactly what we want).
        try:
            self.retained[msg.topic] = msg.payload.decode("utf-8", "replace")
        except Exception:
            self.retained[msg.topic] = repr(msg.payload)

    def harvest(self, seconds):
        self.client.connect(self.host, self.port, keepalive=30)
        self.client.loop_start()
        deadline = time.time() + seconds
        while time.time() < deadline:
            time.sleep(0.1)
        return self.retained

    def publish_retained(self, topic, payload):
        self.client.publish(topic, payload, qos=1, retain=True)
        time.sleep(0.5)

    def watch_topic(self, topic, want_value, timeout):
        """Block until `topic` carries want_value, or timeout. Returns elapsed or None."""
        self.retained.pop(topic, None)
        self.client.subscribe(topic, qos=0)
        start = time.time()
        while time.time() - start < timeout:
            if self.retained.get(topic) == want_value:
                return time.time() - start
            time.sleep(0.5)
        return None

    def stop(self):
        try:
            self.client.loop_stop()
            self.client.disconnect()
        except Exception:
            pass


def find_avty_topic(retained, node=None):
    """The avty topic is <topTopic>/value/<nodeId> with a retained online/offline.
    Detect it by payload, so we never depend on the namespace shape. When `node`
    is given (the device's uniqueid, e.g. otgw-1020BA21B4F8), scope to that
    device so a shared/production broker with many devices targets the right one."""
    hits = [t for t, p in retained.items()
            if p in ("online", "offline") and "/value/" in t
            and (node is None or t.endswith("/" + node))]
    return hits[0] if hits else None


import re

_NODE_RE = re.compile(r"^(otgw-[0-9A-Fa-f]+)")


def _node_of(uniq_id):
    """Extract the per-hardware nodeId baked into every uniq_id (otgw-<mac>-...)."""
    m = _NODE_RE.match(uniq_id or "")
    return m.group(1) if m else None


def assert_topology(retained, node=None):
    """TASK-871 AC#7 / ADR-140: ONE HA device PER HARDWARE. Multiple physical
    OTGW units on the same broker legitimately produce multiple devices; the
    invariant is per-node: every entity of a given nodeId must bind to exactly
    that one device identifier (== nodeId), with no cross-device leakage. The old
    seven-device bug would show as a SINGLE physical unit whose entities scatter
    across several device ids. ADR-148 BLE children may add a via_device that
    points back to their own node's root."""
    configs = {t: p for t, p in retained.items()
               if t.startswith("homeassistant/") and t.endswith("/config")}
    if node:
        # On a shared/production broker the discovery tree carries historical
        # retained configs from many devices + old firmware. Scope to our node so
        # the per-hardware assert reflects the device under test, not the cruft.
        scoped = {}
        for t, p in configs.items():
            try:
                doc = json.loads(p)
            except Exception:
                continue
            uid = doc.get("uniq_id") or doc.get("unique_id") or ""
            if uid.startswith(node):
                scoped[t] = p
        configs = scoped
    if not configs:
        return False, "no retained homeassistant/.../config topics found", {}

    # nodeId -> {declared device ids}, plus via targets per node, and prefix tally.
    node_dev_ids = {}
    node_via = {}
    prefix_counts = {}
    parse_fail = 0
    unscoped = 0
    for topic, payload in configs.items():
        try:
            doc = json.loads(payload)
        except Exception:
            parse_fail += 1
            continue
        uid = doc.get("uniq_id") or doc.get("unique_id") or ""
        node = _node_of(uid)
        if not node:
            unscoped += 1
            continue
        dev = doc.get("dev") or doc.get("device") or {}
        ids = dev.get("ids") or dev.get("identifiers")
        if isinstance(ids, str):
            ids = [ids]
        node_dev_ids.setdefault(node, set())
        for i in (ids or []):
            node_dev_ids[node].add(i)
        via = dev.get("via_device") or doc.get("via_device")
        if via:
            node_via.setdefault(node, set()).add(via)
        # source-prefix split, scoped per node: <node>-<src>_...
        tail = uid[len(node) + 1:] if uid.startswith(node) else uid
        src = tail.split("_", 1)[0] + "_" if "_" in tail else "(none)"
        key = f"{node}-{src}"
        prefix_counts[key] = prefix_counts.get(key, 0) + 1

    info = {
        "config_topics": len(configs),
        "parse_fail": parse_fail,
        "unscoped": unscoped,
        "nodes": sorted(node_dev_ids),
        "uniq_prefixes": prefix_counts,
    }

    # Per-node invariant: each node binds to exactly its own one device id.
    offenders = []
    for node, ids in node_dev_ids.items():
        extra = ids - {node}
        via = node_via.get(node, set())
        # BLE child-device (ADR-148): extra id allowed iff it via_device->node.
        leaked = extra - via
        if node not in ids:
            offenders.append(f"{node}: entities bind to {sorted(ids)}, not own nodeId")
        elif leaked:
            offenders.append(f"{node}: stray device id(s) {sorted(leaked)} (no via_device->{node})")
        if via and via != {node}:
            offenders.append(f"{node}: via_device {sorted(via)} != own node")

    if offenders:
        return False, "per-hardware topology violated: " + "; ".join(offenders), info
    n = len(node_dev_ids)
    return True, (f"{n} hardware unit(s), each ONE HA device, all entities bound "
                  f"to own nodeId: {sorted(node_dev_ids)}"), info


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--broker", help="force broker host (else auto-detect)")
    ap.add_argument("--node", default=_secrets.get("uniqueid"),
                    help="scope to one device's nodeId (e.g. otgw-1020BA21B4F8); "
                         "required on a shared/production broker with many devices")
    ap.add_argument("--port", type=int, default=int(_secrets.get("broker_port") or 1883))
    ap.add_argument("--top-topic", default=None, help="base topic (default: auto)")
    ap.add_argument("--harvest", type=float, default=6.0, help="retained-harvest seconds")
    ap.add_argument("--self-heal-timeout", type=float, default=320.0)
    ap.add_argument("--bounce", action="store_true",
                    help="rig-only: docker restart mosquitto to test rebirth (AC#1)")
    ap.add_argument("--no-self-heal", action="store_true",
                    help="skip the 5-min self-heal wait (topology only)")
    args = ap.parse_args()

    user = _secrets.get("mqtt_user") or ""
    password = _secrets.mqtt_password()

    # Candidate brokers: forced > configured (if resolvable) > test-rig fallback.
    candidates = []
    if args.broker:
        candidates.append(args.broker)
    else:
        cfg = _secrets.get("broker_host")
        if cfg and _resolvable(cfg):
            candidates.append(cfg)
        if _secrets.TEST_RIG_BROKER not in candidates:
            candidates.append(_secrets.TEST_RIG_BROKER)

    # Subscribe broadly enough to catch any topTopic's avty + all discovery.
    filters = ["+/value/+", "homeassistant/#"] if not args.top_topic \
        else [f"{args.top_topic}/value/+", "homeassistant/#"]

    chosen = None
    retained = {}
    for host in candidates:
        if not _resolvable(host):
            print(f"[broker] {host}: unresolvable, skip")
            continue
        print(f"[broker] {host}:{args.port}: harvesting {args.harvest}s ...")
        try:
            c = Collector(host, args.port, user, password, filters)
            retained = c.harvest(args.harvest)
        except Exception as e:
            print(f"[broker] {host}: connect/harvest failed: {e}")
            continue
        avty = find_avty_topic(retained, args.node)
        if avty:
            print(f"[broker] {host}: live device avty found -> {avty}")
            chosen = c
            break
        print(f"[broker] {host}: no live OTGW avty topic; {len(retained)} retained topics")
        c.stop()

    if not chosen:
        sys.exit("FAIL: no broker has a live OTGW availability topic "
                 "(device offline, wrong broker, or wrong credentials)")

    results = {}

    # --- TASK-871 topology ---
    ok, msg, info = assert_topology(retained, args.node)
    results["TASK-871 topology"] = ok
    print(f"\n[871] {'PASS' if ok else 'FAIL'}: {msg}")
    print(f"      config_topics={info.get('config_topics')} "
          f"prefixes={info.get('uniq_prefixes')} via={info.get('via_targets')}")

    # --- TASK-874 self-heal ---
    avty = find_avty_topic(retained, args.node)
    if args.no_self_heal:
        print("\n[874] self-heal SKIPPED (--no-self-heal)")
    else:
        cur = retained.get(avty)
        print(f"\n[874] avty '{avty}' currently '{cur}'")
        print(f"[874] injecting retained 'offline' (simulating stranded birth) ...")
        chosen.publish_retained(avty, "offline")
        print(f"[874] waiting up to {args.self_heal_timeout:.0f}s for firmware re-assert ...")
        elapsed = chosen.watch_topic(avty, "online", args.self_heal_timeout)
        ok = elapsed is not None
        results["TASK-874 self-heal"] = ok
        if ok:
            print(f"[874] PASS: avty self-healed to 'online' after {elapsed:.1f}s "
                  f"(<= 300s re-assert window)")
        else:
            print(f"[874] FAIL: avty still not 'online' after {args.self_heal_timeout:.0f}s")
            # Courtesy: do not leave the rig device stranded 'offline'.
            chosen.publish_retained(avty, "online")
            print("[874] (republished 'online' as cleanup)")

    # --- TASK-874 rebirth on reconnect (rig docker only) ---
    if args.bounce:
        print("\n[874] rebirth: docker restart mosquitto ...")
        try:
            subprocess.run(["docker", "restart", "mosquitto"], check=True,
                           capture_output=True, timeout=60)
        except Exception as e:
            print(f"[874] rebirth SKIPPED: docker restart failed ({e})")
        else:
            chosen.stop()
            time.sleep(5)
            c2 = Collector(chosen.host, args.port, user, password, [avty])
            c2.harvest(2)
            elapsed = c2.watch_topic(avty, "online", 90)
            ok = elapsed is not None
            results["TASK-874 rebirth"] = ok
            print(f"[874] rebirth {'PASS' if ok else 'FAIL'}: "
                  f"{'online '+format(elapsed,'.1f')+'s after broker bounce' if ok else 'no online within 90s'}")
            c2.stop()
            chosen = None

    if chosen:
        chosen.stop()

    print("\n=== SUMMARY ===")
    for k, v in results.items():
        print(f"  {'PASS' if v else 'FAIL'}  {k}")
    sys.exit(0 if all(results.values()) and results else 1)


if __name__ == "__main__":
    main()
