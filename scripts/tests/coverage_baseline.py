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

# Device- and site-specific fragments stripped so a baseline is portable between
# benches: topic root, uniqueid/MAC, broker host.
TOPIC_STRIP_RX = re.compile(r"^[^/]+/(value|set)/otgw-[0-9A-Fa-f]+/")


def normalise_value(text: str) -> str:
    """Make a decoded value byte-encoding independent.

    Captures are written by a telnet stream that carries cp1252 bytes (the degree
    sign in unit suffixes). Decoding as UTF-8 turns those into U+FFFD, so the same
    firmware could fingerprint differently depending on how the log was read.
    Units carry no regression signal that the label does not already carry, so
    drop non-ASCII entirely and collapse the whitespace it leaves behind.
    """
    ascii_only = text.encode("ascii", "ignore").decode("ascii")
    return re.sub(r"\s+", " ", ascii_only).strip()


def normalise_topic(topic: str) -> str:
    stripped = TOPIC_STRIP_RX.sub("", topic)
    return stripped if stripped != topic else re.sub(r"otgw-[0-9A-Fa-f]{12}", "<id>", topic)


def fingerprint(lines) -> dict:
    ot: dict[str, dict] = {}
    mqtt: dict[str, set] = {}
    msgtypes: set[str] = set()
    sources: set[str] = set()

    for line in lines:
        m = OT_RX.search(line)
        if m:
            key = f"{m.group('frame')[0]}|{m.group('type')}|{int(m.group('id'))}"
            rest = normalise_value(m.group("rest"))
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
            topic = normalise_topic(m.group("topic"))
            mqtt.setdefault(topic, set())
            if not VOLATILE_TOPIC_RX.search(topic):
                mqtt[topic].add(m.group("payload"))

    return {
        "ot": {k: {"src": v["src"], "rendered": sorted(v["rendered"])} for k, v in sorted(ot.items())},
        "mqtt": {k: sorted(v) for k, v in sorted(mqtt.items())},
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


def diff(base: dict, cur: dict) -> list[str]:
    out: list[str] = []

    for section in ("ot", "mqtt"):
        b, c = base[section], cur[section]
        for key in sorted(set(b) - set(c)):
            out.append(f"MISSING  {section}: {key}   (baseline had: {b[key]})")
        for key in sorted(set(c) - set(b)):
            out.append(f"NEW      {section}: {key}   (now: {c[key]})")
        for key in sorted(set(b) & set(c)):
            if b[key] != c[key]:
                out.append(f"CHANGED  {section}: {key}\n           baseline: {b[key]}\n           current : {c[key]}")

    for section in ("msgtypes", "sources"):
        b, c = set(base[section]), set(cur[section])
        for v in sorted(b - c):
            out.append(f"MISSING  {section}: {v}")
        for v in sorted(c - b):
            out.append(f"NEW      {section}: {v}")

    return out


def main() -> int:
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser()
    ap.add_argument("mode", choices=["record", "compare", "selftest"])
    ap.add_argument("capture")
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
    problems = diff(base, cur)
    if not problems:
        c = cur["counts"]
        print(f"PASS: no drift vs baseline "
              f"({c['ot_keys']} ot keys, {c['msgids']} MsgIDs, {c['mqtt_topics']} topics)")
        return 0
    print(f"DRIFT: {len(problems)} difference(s) vs {args.baseline}\n")
    for p in problems:
        print(p)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
