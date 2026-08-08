#!/usr/bin/env python3
"""Reduce a coverage-simulation capture to a normalized fingerprint, and diff it
against a committed baseline.

Why a fingerprint and not the raw log: every run differs in timestamps, heap and
max-block columns, task ids, uptime, device MAC and broker host. Diffing raw logs
is all noise. What actually characterises firmware behaviour is narrower:

  * for each (prefix, msgtype, msgid): the decoded label and the rendered value
  * for each MQTT topic: the SET of payloads it published
  * which message types and source prefixes were exercised at all

Those are stable across runs of the same firmware against the same fixture, and
they move the moment decode or publish behaviour changes. That makes them a
usable regression gate.

Usage:
    # record a baseline from a known-good capture
    python coverage_baseline.py record CAPTURE.log --out baseline_coverage.json

    # check a new capture against it (exit 1 on drift)
    python coverage_baseline.py compare CAPTURE.log --baseline baseline_coverage.json

    # prove stability: fingerprint two halves of one capture and diff them
    python coverage_baseline.py selftest CAPTURE.log
"""
from __future__ import annotations

import argparse
import json
import os
import re
import sys

# 12:03:59.781864 (  19760| 18312) processOT   (4419): Thermostat  T10010A00   1 Write-Data  > TSet = 10.00 °C
OT_RX = re.compile(
    r"processOT\s+\(\s*\d+\):\s+(?P<src>Request Boiler|Answer Thermostat|Thermostat|Boiler|Parity Error)\s+"
    r"(?P<frame>[TBRAE][0-9A-F]{8})\s+(?P<id>\d+)\s+(?P<type>[A-Za-z-]+)\s*>?\s*(?P<rest>.*)$"
)
MQTT_RX = re.compile(
    r"sendMQTTData\(\d+\):\s+Sending MQTT:.*?TopicId \[(?P<topic>[^\]]+)\] --> Message \[(?P<payload>[^\]]*)\]"
)

# Topics whose payload is unbounded/wall-clock driven. Their PRESENCE is still
# recorded (a topic vanishing is a regression); only the payload set is dropped.
VOLATILE_TOPIC_RX = re.compile(
    r"(otgw-firmware/uptime$|otgw-firmware/stats/|/epoch|_epoch$|/timestamp$)"
)

# Topics not driven by the OT fixture at all: PIC polling on its own timer
# (ADR-037 PR=M) and firmware/device metadata. Whether they land inside a given
# capture window is a matter of timing, not of decode behaviour, so including
# them makes the gate flap. Dropped entirely rather than payload-stripped.
NON_FIXTURE_TOPIC_RX = re.compile(r"^(otgw-pic/|otgw-firmware/)")

# Device- and site-specific fragments stripped so a baseline is portable between
# benches: topic root, uniqueid/MAC, broker host.
TOPIC_STRIP_RX = re.compile(r"^[^/]+/(value|set)/otgw-[0-9A-Fa-f]+/")


# A telnet stream carries several concurrent log producers, so one line can be cut
# mid-way and another task's line appended to it. 2.0.0 does this far more than
# 1.x (the SAT BLE scanner, the async web server and the OT decoder all write).
# Left unhandled the intruding text lands inside a decoded value and the
# fingerprint drifts between runs for no firmware reason. Cut at the start of any
# embedded log line: "HH:MM:SS.uuuuuu (" is the unmistakable prefix.
SPLICE_RX = re.compile(r"\d{2}:\d{2}:\d{2}\.\d+\s*\(")


def normalise_value(text: str) -> str:
    """Make a decoded value byte-encoding independent.

    Captures are written by a telnet stream that carries cp1252 bytes (the degree
    sign in unit suffixes). Decoding as UTF-8 turns those into U+FFFD, so the same
    firmware could fingerprint differently depending on how the log was read.
    Units carry no regression signal that the label does not already carry, so
    drop non-ASCII entirely and collapse the whitespace it leaves behind.
    """
    cut = SPLICE_RX.search(text)
    if cut:
        text = text[:cut.start()]
    ascii_only = text.encode("ascii", "ignore").decode("ascii")
    return re.sub(r"\s+", " ", ascii_only).strip()


def normalise_topic(topic: str) -> str:
    stripped = TOPIC_STRIP_RX.sub("", topic)
    return stripped if stripped != topic else re.sub(r"otgw-[0-9A-Fa-f]{12}", "<id>", topic)


def mqtt_section(pairs) -> dict:
    """Build the 'mqtt' half of a fingerprint from (topic, payload) observations.

    Shared by both sources so a broker-sourced and a telnet-sourced half are
    normalised identically and stay comparable.
    """
    mqtt: dict[str, set] = {}
    for topic, payload in pairs:
        t = normalise_topic(topic)
        # A real topic never contains whitespace. One that does came from a
        # telnet line with another task's output spliced into the middle, e.g.
        # 'otgw-piSending MQTT: server ...'. Drop the observation rather than
        # invent a topic that will never appear again. Harmless for the broker
        # source, which cannot produce one.
        if re.search(r"\s", t):
            continue
        if NON_FIXTURE_TOPIC_RX.search(t):
            continue
        mqtt.setdefault(t, set())
        if not VOLATILE_TOPIC_RX.search(t):
            mqtt[t].add(payload)
    return {k: sorted(v) for k, v in sorted(mqtt.items())}


def fingerprint(lines, mqtt_pairs=None, mqtt_source: str = "telnet") -> dict:
    """Reduce a capture to its normalized fingerprint.

    `mqtt_pairs`, when given, supplies the topic half from a broker subscription
    instead of from the capture's publish log lines. The source is recorded in
    the fingerprint because the two are not interchangeable: telnet presence is
    a lossy sample, broker presence is an observation, and only the latter can
    be gated on.
    """
    ot: dict[str, dict] = {}
    telnet_pairs: list = []
    msgtypes: set[str] = set()
    sources: set[str] = set()

    for line in lines:
        m = OT_RX.search(line)
        if m:
            key = f"{m.group('frame')[0]}|{m.group('type')}|{int(m.group('id'))}"
            rest = normalise_value(m.group("rest"))
            if not rest:
                # The line was truncated by interleaved output and carried no
                # decoded value of its own. Recording an empty rendering would
                # make the key differ between runs purely on stream timing, so
                # drop this observation. The same key from an intact line
                # elsewhere in the capture still counts.
                continue
            prev = ot.get(key)
            if prev is None:
                ot[key] = {"src": m.group("src"), "rendered": [rest]}
            elif rest not in prev["rendered"]:
                prev["rendered"].append(rest)
            msgtypes.add(m.group("type"))
            sources.add(m.group("src"))
            continue

        m = MQTT_RX.search(line)
        if m:
            telnet_pairs.append((m.group("topic"), m.group("payload")))

    mqtt = mqtt_section(telnet_pairs if mqtt_pairs is None else mqtt_pairs)
    return {
        "ot": {k: {"src": v["src"], "rendered": sorted(v["rendered"])} for k, v in sorted(ot.items())},
        "mqtt": mqtt,
        "mqtt_source": "telnet" if mqtt_pairs is None else mqtt_source,
        "msgtypes": sorted(msgtypes),
        "sources": sorted(sources),
        "counts": {
            "ot_keys": len(ot),
            "msgids": len({int(k.split("|")[2]) for k in ot}),
            "mqtt_topics": len(mqtt),
        },
    }


def read(path: str) -> list[str]:
    with open(path, encoding="utf-8", errors="replace") as fh:
        return fh.readlines()


# Sections whose PRESENCE is always reproducible enough to gate on. 'mqtt' is
# conditional and handled by mqtt_presence_gated(): whether its presence can be
# gated depends on where the topics were read from, not on the section itself.
PRESENCE_GATED = ("ot", "msgtypes", "sources")


def mqtt_presence_gated(base: dict, cur: dict) -> bool:
    """True when both sides' topics came from a broker subscription.

    Telnet-sourced presence is a lossy sample: across two independent two-loop
    runs of identical firmware and fixture the topic sets differed by 9, with the
    smaller a strict SUBSET of the larger and zero spurious topics, while payload
    sets differed on 0 of the 215 shared topics. So loss is subtractive sampling,
    not behaviour, and gating telnet presence produces a gate that cries wolf.
    (Intersecting several runs was considered and rejected: each run drops a
    different random subset, so the intersection erodes rather than converging.)

    A broker subscription sees every publish the firmware makes, so presence
    there is an observation and a vanished topic is a real regression. Mixing the
    two is not allowed: a broker baseline compared against a telnet run would
    report every dropped line as a regression.
    """
    return base.get("mqtt_source") == "broker" and cur.get("mqtt_source") == "broker"


def diff(base: dict, cur: dict, sections: tuple = ()) -> tuple:
    """Compare two fingerprints.

    Returns (failures, notes). `failures` set the exit code; `notes` are printed
    for the reader but do not fail the gate. `sections` limits the comparison to
    one pass's half, so a single-pass run is not reported as having lost the
    other half.
    """
    failures: list[str] = []
    notes: list[str] = []
    allowed = set(sections) if sections else None

    gate_mqtt = mqtt_presence_gated(base, cur)
    for section in ("ot", "mqtt"):
        if allowed is not None and section not in allowed:
            continue
        # Presence diffs fail only where presence is actually observable.
        gated = section in PRESENCE_GATED or (section == "mqtt" and gate_mqtt)
        presence = failures if gated else notes
        b, c = base[section], cur[section]
        for key in sorted(set(b) - set(c)):
            presence.append(f"MISSING  {section}: {key}   (baseline had: {b[key]})")
        for key in sorted(set(c) - set(b)):
            presence.append(f"NEW      {section}: {key}   (now: {c[key]})")
        for key in sorted(set(b) & set(c)):
            if b[key] != c[key]:
                failures.append(f"CHANGED  {section}: {key}\n           baseline: {b[key]}\n           current : {c[key]}")

    for section in ("msgtypes", "sources"):
        if allowed is not None and section not in allowed:
            continue
        presence = failures if section in PRESENCE_GATED else notes
        b, c = set(base[section]), set(cur[section])
        for v in sorted(b - c):
            presence.append(f"MISSING  {section}: {v}")
        for v in sorted(c - b):
            presence.append(f"NEW      {section}: {v}")

    return failures, notes


def main() -> int:
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser()
    ap.add_argument("mode", choices=["record", "compare", "selftest"])
    ap.add_argument("capture", help="capture log to fingerprint")
    ap.add_argument("--out", default=os.path.join(here, "baseline_coverage.json"))
    ap.add_argument("--baseline", default=os.path.join(here, "baseline_coverage.json"))
    args = ap.parse_args()

    lines = read(args.capture)

    if args.mode == "record":
        fp = fingerprint(lines)
        with open(args.out, "w", encoding="utf-8", newline="\n") as fh:
            json.dump(fp, fh, indent=1, sort_keys=True, ensure_ascii=False)
            fh.write("\n")
        c = fp["counts"]
        print(f"baseline written : {args.out}")
        print(f"  ot keys        : {c['ot_keys']}  ({c['msgids']} distinct MsgIDs)")
        print(f"  mqtt topics    : {c['mqtt_topics']}")
        print(f"  message types  : {', '.join(fp['msgtypes'])}")
        print(f"  sources        : {', '.join(fp['sources'])}")
        return 0

    if args.mode == "selftest":
        # Split the capture in half and fingerprint each. Same firmware, same
        # fixture, different wall-clock: any diff here is a volatile field that
        # normalisation failed to strip.
        half = len(lines) // 2
        a, b = fingerprint(lines[:half]), fingerprint(lines[half:])
        shared_ot = set(a["ot"]) & set(b["ot"])
        drift = [k for k in shared_ot if a["ot"][k] != b["ot"][k]]
        print(f"half A: {a['counts']}")
        print(f"half B: {b['counts']}")
        print(f"ot keys in both halves : {len(shared_ot)}")
        print(f"unstable ot keys       : {len(drift)}")
        for k in drift[:10]:
            print(f"  {k}\n    A: {a['ot'][k]['rendered']}\n    B: {b['ot'][k]['rendered']}")
        return 1 if drift else 0

    with open(args.baseline, encoding="utf-8") as fh:
        base = json.load(fh)
    cur = fingerprint(lines)
    sections: tuple = ()
    if not cur["mqtt"]:
        # A pass-A capture (MQTT debug off) has no topics by construction.
        # Comparing the mqtt section would report every baseline topic as
        # MISSING, which says nothing about the firmware.
        sections = PRESENCE_GATED
        print("note: capture has no MQTT lines; comparing the OT half only")
    failures, notes = diff(base, cur, sections)
    if notes:
        print(f"\n{len(notes)} informational difference(s) (not gated):\n")
        for n in notes:
            print(n)
    if not failures:
        c = cur["counts"]
        print(f"\nPASS: no drift vs baseline "
              f"({c['ot_keys']} ot keys, {c['msgids']} MsgIDs, {c['mqtt_topics']} topics)")
        return 0
    print(f"\nDRIFT: {len(failures)} difference(s) vs {args.baseline}\n")
    for p in failures:
        print(p)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
