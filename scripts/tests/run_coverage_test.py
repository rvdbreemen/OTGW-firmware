#!/usr/bin/env python3
"""Run the OT coverage fixture against a device and check it against the baseline.

One command instead of the five-step manual sequence (upload, start, capture,
stop, compare). Exits 0 when the run matches the committed baseline and non-zero
on drift or on any device error, so it works as a gate after a firmware change.

    python run_coverage_test.py --host otgw1.local

The simulation is stopped in a finally block: a failed capture or a drifting
comparison must never leave a bench publishing synthetic OpenTherm data to a real
MQTT broker.

The two halves of the fingerprint come from two different transports over one
window, because they cannot survive the same one. Decoded frames are read from
the debug telnet with MQTT debug OFF: with it on, a MsgID that fans out to ~9
topics emits its publish lines in ~10 ms, overruns the telnet, and drops whatever
follows, which on the bench S3 swallowed AC0630000's decode line while the frame
itself was processed normally. Topics are read from an MQTT subscription instead
of from those publish log lines, because a broker sees every publish while the
log is lossy: two identical runs differed by 9 topics, the smaller set a strict
subset of the larger. MQTTDebugTf gates the log line only and never the publish,
so both can be true at once, and the two collectors share a single window.

Capture length defaults to TWO full fixture loops plus a margin, and the runner
refuses to run shorter. One loop is not enough for two independent reasons: a
capture cut mid-loop reports topics as MISSING purely because they had not come
round yet, and a frame that appears once per loop gets no second chance when the
telnet stream interleaves (2.0.0 splices SAT/BLE lines into decode output) and
mangles its only occurrence. Both failure modes look exactly like a regression.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import socket
import sys
import threading
import time
import urllib.error
import urllib.request

import coverage_baseline
import mqtt_topic_capture

HERE = os.path.dirname(os.path.abspath(__file__))
FIXTURE = os.path.join(HERE, "otgw_simulation_coverage.log")
BASELINE = os.path.join(HERE, "baseline_coverage.json")
FRAME_INTERVAL_S = 0.75          # device default, /api/v2/simulate reports it
TELNET_PORT = 23


def http(url: str, method: str = "GET", timeout: int = 15) -> str:
    req = urllib.request.Request(url, method=method)
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        return resp.read().decode("utf-8", "replace")


def device_mqtt_config(host: str, user: str, password: str) -> dict:
    """Read the broker the DEVICE publishes to, plus the id its topics carry.

    Taken from the device rather than from the local secrets store on purpose:
    the stored BrokerHost is a bench convenience that goes stale (it pointed at
    an unreachable 192.168.1.11 while the device was publishing happily to
    homeassistant.local), and a gate that watches the wrong broker sees nothing
    and calls it a regression.
    """
    mgr = urllib.request.HTTPPasswordMgrWithDefaultRealm()
    mgr.add_password(None, f"http://{host}/", user, password)
    opener = urllib.request.build_opener(urllib.request.HTTPBasicAuthHandler(mgr),
                                         urllib.request.HTTPDigestAuthHandler(mgr))
    with opener.open(f"http://{host}/api/v2/settings", timeout=15) as resp:
        doc = json.loads(resp.read().decode("utf-8", "replace"))
    with urllib.request.urlopen(f"http://{host}/api/v2/device/info", timeout=15) as resp:
        info = json.loads(resp.read().decode("utf-8", "replace"))

    # Both endpoints wrap their payload in a single-key envelope
    # ({"settings": {...}}, {"device": {...}}), and each setting is
    # {"value": ..., "type": ...} rather than a bare value.
    settings = doc.get("settings", doc)
    device = info.get("device", info)

    def val(key):
        entry = settings.get(key)
        return entry.get("value") if isinstance(entry, dict) else None

    broker = val("mqttbroker")
    mac = device.get("macaddress", "")
    if not broker or not mac:
        # Falling back to the local secrets store here would silently point the
        # gate at a different broker (its stored host is stale), and an empty
        # uniqueid would collect every OTGW on the broker as if it were this one.
        raise RuntimeError(f"device did not report a usable MQTT config "
                           f"(broker={broker!r}, mac={mac!r})")
    return {
        "broker": broker,
        "port": int(val("mqttbrokerport") or 1883),
        "user": val("mqttuser") or "",
        "root": val("mqtttoptopic") or "OTGW",
        "uniqueid": "otgw-" + mac.replace(":", "").upper(),
    }


def upload_fixture(host: str, path: str) -> None:
    """Multipart upload to /upload, keeping the device-side filename."""
    boundary = "----otgwcoverage"
    body = (
        f"--{boundary}\r\n"
        f'Content-Disposition: form-data; name="file"; filename="otgw_simulation.log"\r\n'
        f"Content-Type: application/octet-stream\r\n\r\n"
    ).encode() + open(path, "rb").read() + f"\r\n--{boundary}--\r\n".encode()
    req = urllib.request.Request(
        f"http://{host}/upload", data=body, method="POST",
        headers={"Content-Type": f"multipart/form-data; boundary={boundary}"},
    )
    try:
        urllib.request.urlopen(req, timeout=60).read()
    except urllib.error.HTTPError as exc:
        if exc.code not in (200, 302, 303):     # the handler answers 303 on success
            raise


# Each toggle echoes exactly one short confirmation line naming the flag and its
# NEW value. Identical text on otgw-1.x.x and 2.0.0, so matching on the text is
# branch-agnostic where matching on the keypress is not. 'Debug MQTT:' cannot
# also match 'Debug MQTT Gating:' because the colon must follow MQTT directly.
ECHO_RX = {
    "mqtt":      re.compile(r"Debug MQTT:\s*(true|false)"),
    "mqtt_gate": re.compile(r"Debug MQTT Gating:\s*(true|false)"),
}

# Candidate toggle keys per flag, tried in order. The mapping is NOT stable
# across branches (otgw-1.x.x gives '4' to MQTTGate, 2.0.0 gives '4' to Sensors
# and 'g' to MQTTGate), so every press is verified against the 'D' dump below
# and rolled back when it moved the wrong flag.
TOGGLE_KEYS = {"mqtt": ("3",), "mqtt_gate": ("g", "4")}


def _drain(sock: socket.socket, seconds: float) -> str:
    text = ""
    end = time.time() + seconds
    while time.time() < end:
        try:
            text += sock.recv(4096).decode("utf-8", "replace")
        except socket.timeout:
            pass
    return text


def press(sock: socket.socket, key: str, name: str):
    """Press one toggle key; return the flag's NEW value, or None if `name` did
    not echo (wrong key for this branch, or the echo was lost in the stream)."""
    sock.sendall(key.encode())
    m = ECHO_RX[name].search(_drain(sock, 1.5))
    return None if m is None else (m.group(1) == "true")


def set_debug_flag(sock: socket.socket, name: str, want: bool) -> str:
    """Drive one debug flag to `want`, reading back the result rather than
    assuming it.

    The keys are toggles, not setters: pressing one that is already on turns it
    OFF. That, plus a key mapping that is not stable across branches, used to
    fail silently and yield a capture with no publish lines, which then reads as
    a regression instead of as a broken capture.

    The device's own '[state.debug]' dump ('D') would give the state by name,
    but it is ~290 lines and the busy telnet truncates it (measured on the bench
    S3: 8 KB in 12 s and the section never arrived). The per-toggle echo is one
    short line, so it survives where the dump does not.
    """
    for key in TOGGLE_KEYS[name]:
        state = press(sock, key, name)
        if state is None:
            # Either this key drives a different flag, or the echo was dropped.
            # Press again: that restores the state in the first case and gives
            # the echo a second chance in the second. Either way, net zero
            # toggles before moving on to the next candidate key.
            state = press(sock, key, name)
            if state is None:
                continue
        if state == want:
            return f"{name} -> {want} (key '{key}')"
        state = press(sock, key, name)
        if state == want:
            return f"{name} -> {want} (key '{key}')"
        raise RuntimeError(f"{name}: pressed '{key}' twice and it reads {state}, "
                           f"wanted {want}")
    raise RuntimeError(f"could not drive {name}={want}: none of "
                       f"{TOGGLE_KEYS[name]} echoed a confirmation")


def preflight_replay(host: str, seconds: int = 25, min_distinct: int = 12) -> int:
    """Confirm the replay is actually advancing before committing to a full window.

    Observed once on the bench: the device replayed the fixture's first five
    lines over and over for the whole 694 s window. Those five happen to all be
    MsgID 0, so the capture looked superficially alive (703 decode lines) while
    carrying 30 of 376 distinct frames. Not reproducible afterwards, upload and
    fixture on device were both correct, so treat it as a transient the gate has
    to survive rather than a bug to fix blind.

    Unchecked it costs a 12-minute run and, with --record, silently writes a
    baseline reflecting almost no coverage. 25 seconds is ~33 frames at the
    default pacing, so a healthy replay clears min_distinct comfortably while a
    stuck one cannot.
    """
    fixture = {line.strip() for line in open(FIXTURE, encoding="ascii") if line.strip()}
    sock = socket.create_connection((host, TELNET_PORT), timeout=10)
    sock.settimeout(1.0)
    text = _drain(sock, seconds)
    sock.close()
    distinct = {f for f in fixture if f in text}
    if len(distinct) < min_distinct:
        raise RuntimeError(
            f"replay is not advancing: only {len(distinct)} distinct fixture "
            f"frames in {seconds}s (expected at least {min_distinct}). The "
            f"device is looping the start of the file rather than reading "
            f"through it. Stop and restart the simulation, then retry.")
    return len(distinct)


def capture(host: str, seconds: int, out_path: str, want_mqtt: bool) -> int:
    """Log telnet:23 for `seconds`, with the MQTT debug flags driven to want_mqtt.

    want_mqtt selects which half of the fingerprint this capture is for. The two
    halves cannot share one capture: on 2.0.0 a MsgID that fans out to ~9 topics
    emits its publish lines in ~10 ms, which overruns the debug telnet and drops
    the following decode line outright (observed: AC0630000's processOT line
    vanished inside a 740 ms hole, in BOTH fixture loops, while the frame itself
    was processed normally). The MQTT output is thus simultaneously the source of
    the topic fingerprint and the reason the OT fingerprint loses frames.
    """
    sock = socket.create_connection((host, TELNET_PORT), timeout=10)
    sock.settimeout(1.0)
    _drain(sock, 2.0)                            # let the banner land and settle
    for flag in ("mqtt", "mqtt_gate"):
        print(f"toggles  : {set_debug_flag(sock, flag, want_mqtt)}")

    lines = 0
    with open(out_path, "w", encoding="utf-8", errors="replace") as fh:
        stop = time.time() + seconds
        while time.time() < stop:
            try:
                data = sock.recv(4096)
                if not data:
                    break
                text = data.decode("utf-8", "replace")
                fh.write(text)
                fh.flush()
                lines += text.count("\n")
            except socket.timeout:
                continue
    sock.close()
    return lines


def main() -> int:
    fixture_frames = sum(1 for _ in open(FIXTURE, encoding="ascii"))
    loop_s = int(fixture_frames * FRAME_INTERVAL_S)
    default_s = loop_s * 2 + 60

    ap = argparse.ArgumentParser()
    ap.add_argument("--host", required=True)
    ap.add_argument("--seconds", type=int, default=default_s,
                    help=f"capture length (default {default_s}s = two {loop_s}s loops + margin)")
    ap.add_argument("--baseline", default=BASELINE)
    ap.add_argument("--out", default=os.path.join(HERE, "last-coverage-run.log"))
    ap.add_argument("--topics", choices=("broker", "telnet"), default="broker",
                    help="where the MQTT topic half comes from. 'broker' "
                         "subscribes and sees every publish, so topic presence "
                         "is gated. 'telnet' reads the debug log, which drops "
                         "lines under load, so presence is only reported.")
    ap.add_argument("--http-user", default="robert")
    ap.add_argument("--http-password", default=os.environ.get("OTGW_HTTP_PASSWORD", ""),
                    help="device HTTP password (default: $OTGW_HTTP_PASSWORD)")
    ap.add_argument("--record", action="store_true",
                    help="write this run as the new baseline instead of "
                         "comparing against it. Only after reading a diff and "
                         "concluding the new behaviour is intended.")
    ap.add_argument("--keep-running", action="store_true",
                    help="leave the simulation enabled after the run (default: stop it)")
    args = ap.parse_args()

    if args.seconds < loop_s * 2:
        print(f"refusing to run: {args.seconds}s is under two {loop_s}s fixture loops.\n"
              "A partial loop reports frames as MISSING that had not come round yet, and a\n"
              "single loop gives a once-per-loop frame no second chance when the telnet\n"
              "stream interleaves and mangles its only occurrence. Both read as regressions.",
              file=sys.stderr)
        return 2

    print(f"device   : {args.host}")
    print(f"fixture  : {fixture_frames} frames, loop {loop_s}s")
    print(f"window   : {args.seconds}s -> {args.out}")

    reader = None
    pairs: list = []
    scope = None
    mqtt_source = "telnet"
    if args.topics == "broker":
        try:
            cfg = device_mqtt_config(args.host, args.http_user, args.http_password)
            scope = cfg["uniqueid"]
            reader, bhost, bport, drained = mqtt_topic_capture.open_reader(
                mqtt_topic_capture.topic_filter_for(cfg["root"]),
                cfg["broker"], cfg["port"], cfg["user"], cfg["uniqueid"])
            mqtt_source = "broker"
            print(f"broker   : {bhost}:{bport}  scope {cfg['uniqueid']}  "
                  f"({drained} retained message(s) discarded)")
        except (OSError, RuntimeError, ValueError, KeyError,
                urllib.error.URLError) as exc:
            print(f"broker error: {exc}", file=sys.stderr)
            print("Pass --topics telnet to fall back to the lossy debug-log "
                  "source (topic presence is then not gated).", file=sys.stderr)
            return 2

    started = False
    try:
        upload_fixture(args.host, FIXTURE)
        print("uploaded : fixture -> /otgw_simulation.log")
        http(f"http://{args.host}/api/v2/simulate/start", method="POST")
        started = True
        print("started  : simulation running")
        print(f"preflight: {preflight_replay(args.host)} distinct frames seen, "
              f"replay is advancing")

        # The broker subscription and the telnet capture cover the SAME window.
        # They are independent transports, and MQTTDebugTf gates only the log
        # line, never the publish, so the device can keep MQTT debug off (which
        # is what keeps the decode lines intact) while the broker still sees
        # every publish. One window instead of two.
        collector = None
        if reader is not None:
            # Drop the backlog buffered since subscribe: upload, simulation
            # start and the preflight all happen after the subscription goes
            # live, and everything published in that stretch belongs to the idle
            # device, not to this window.
            stale = reader.flush()
            # `scope` is not optional: without it the collector takes in every
            # OTGW on the broker, and normalise_topic() strips the uniqueid, so
            # another unit's topics land in this device's fingerprint.
            collector = threading.Thread(
                target=reader.collect,
                args=(args.seconds + 5, pairs, scope), daemon=True)
            collector.start()
            print(f"flushed  : {stale} pre-window publish(es) discarded")

        want_mqtt_log = (args.topics == "telnet")
        print(f"telnet   : MQTT debug {'on' if want_mqtt_log else 'off'}")
        lines = capture(args.host, args.seconds, args.out, want_mqtt_log)
        print(f"captured : {lines} telnet lines")
        if collector is not None:
            collector.join(timeout=30)
            print(f"observed : {len(pairs)} publishes from the broker")
    except (OSError, urllib.error.URLError, RuntimeError) as exc:
        print(f"device error: {exc}", file=sys.stderr)
        return 2
    finally:
        if started and not args.keep_running:
            try:
                http(f"http://{args.host}/api/v2/simulate/stop", method="POST")
                print("stopped  : simulation disabled")
            except Exception as exc:                      # noqa: BLE001 - best effort
                print(f"WARNING: could not stop simulation: {exc}", file=sys.stderr)
        if reader is not None:
            reader.close()

    current = coverage_baseline.fingerprint(
        coverage_baseline.read(args.out),
        mqtt_pairs=pairs if mqtt_source == "broker" else None,
        mqtt_source=mqtt_source)
    counts = current["counts"]
    print(f"decoded  : {counts['ot_keys']} keys, {counts['msgids']} MsgIDs, "
          f"{counts['mqtt_topics']} topics ({mqtt_source})")

    if args.record:
        with open(args.baseline, "w", encoding="utf-8", newline="\n") as fh:
            json.dump(current, fh, indent=1, sort_keys=True, ensure_ascii=False)
            fh.write("\n")
        print(f"\nbaseline written: {args.baseline}")
        return 0

    with open(args.baseline, encoding="utf-8") as fh:
        base = json.load(fh)
    failures, notes = coverage_baseline.diff(base, current)

    if notes:
        print(f"\n{len(notes)} informational difference(s), not gated "
              f"(see mqtt_presence_gated() in coverage_baseline.py):\n")
        for n in notes:
            print(n)

    if not failures:
        print("\nPASS: run matches the baseline")
        return 0

    print(f"\nFAIL: {len(failures)} difference(s) vs baseline\n")
    for p in failures:
        print(p)
    print(f"\nCapture kept at {args.out} for investigation.")
    print("If the change is intended, review the diff and re-run with --record.")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
