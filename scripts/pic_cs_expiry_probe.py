#!/usr/bin/env python3
"""
pic_cs_expiry_probe.py - measure how long the PIC holds a control setpoint (TASK-1236)

The PIC gateway firmware (other-projects/otgw-6.6/gateway.asm, CommandExpiry)
drops a CS= control setpoint of 8 C or more when no new CS= command arrived
within one expiry period. The period is Timer1 at Fosc/4 = 1 MHz with a 1:8
prescaler (0.524 s per overflow) times a 120-tick counter, so the override ends
between about 63 s and 126 s after the last CS= command.

The script reads the PIC byte stream on the TCP serial bridge (port 25238) and
follows MsgID 1 (TSet):
  T1 frame  - the thermostat's request
  R1 frame  - the gateway replaced the request towards the boiler (override on)
  "CS: x"   - the PIC acknowledged a CS= command (from SAT, the user or us)

For each CS acknowledgement it reports how long R frames for MsgID 1 kept
coming. A held override shows R frames up to the next acknowledgement; a lapse
shows T frames passing through unchanged before the next one.

Usage (Classic-S3 or any PIC board):
  python scripts/pic_cs_expiry_probe.py --host 192.168.88.63 --duration 600
  python scripts/pic_cs_expiry_probe.py --host 192.168.88.63 --send-cs 45 --duration 240

--send-cs sends one CS=<value> through POST /api/v2/otgw/commands at the start
and CS=0 at the end, so the boiler setpoint is released again. Without it the
script only watches, for example while SAT is enabled.

Needs a thermostat that sends MsgID 1, otherwise there are no T1 frames to
override. Stdlib only.
"""

import argparse
import json
import re
import socket
import sys
import time
import urllib.request

FRAME_RE = re.compile(r"^([TBRA])([0-9A-Fa-f]{8})$")
CS_ACK_RE = re.compile(r"^CS:\s*(-?\d+(?:\.\d+)?)")


def f88(hi, lo):
    v = (hi << 8) | lo
    if v & 0x8000:
        v -= 0x10000
    return v / 256.0


def send_command(host, api_port, cmd):
    url = "http://%s:%d/api/v2/otgw/commands" % (host, api_port)
    body = json.dumps({"command": cmd}).encode()
    req = urllib.request.Request(url, data=body, method="POST",
                                 headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=5) as resp:
            return resp.status
    except Exception as exc:  # report and carry on; the stream still tells the truth
        print("command %s failed: %s" % (cmd, exc), flush=True)
        return None


class Tracker:
    def __init__(self, out):
        self.out = out
        self.t0 = time.monotonic()
        self.override = None       # None = unknown, True = R1 seen, False = T1 passed
        self.last_ack = None       # monotonic time of the last CS acknowledgement
        self.last_ack_value = None
        self.last_r1 = None        # monotonic time of the last R1 frame
        self.pending_t1 = None     # (time, value) of a T1 not yet followed by R1
        self.t1_frames = 0
        self.r1_frames = 0
        self.held = []             # seconds the override lasted after each ack

    def log(self, msg):
        line = "%8.1f s  %s" % (time.monotonic() - self.t0, msg)
        print(line, flush=True)
        if self.out:
            self.out.write(line + "\n")
            self.out.flush()

    def since_ack(self, now):
        if self.last_ack is None:
            return "no CS ack seen yet"
        return "%.1f s after CS ack %.2f" % (now - self.last_ack, self.last_ack_value)

    def close_hold(self, now):
        # The hold of the previous ack ends at its last R1 frame.
        if self.last_ack is not None and self.last_r1 is not None and self.last_r1 >= self.last_ack:
            self.held.append(self.last_r1 - self.last_ack)

    def on_line(self, line):
        now = time.monotonic()
        m = CS_ACK_RE.match(line)
        if m:
            self.close_hold(now)
            self.last_ack = now
            self.last_ack_value = float(m.group(1))
            self.log("CS ack: %s" % line)
            return
        m = FRAME_RE.match(line)
        if not m:
            return
        kind, hexs = m.group(1), m.group(2)
        b = bytes.fromhex(hexs)
        if b[1] != 1:
            return
        msgtype = (b[0] >> 4) & 0x7
        if msgtype != 1:           # only WRITE-DATA requests carry TSet
            return
        value = f88(b[2], b[3])
        if kind == "T":
            self.t1_frames += 1
            # A T1 that is not replaced shows up without an R1 before the next T1.
            if self.pending_t1 is not None and self.override is not False:
                self.override = False
                self.log("override OFF: thermostat TSet %.2f passes through (%s)"
                         % (self.pending_t1[1], self.since_ack(now)))
            self.pending_t1 = (now, value)
        elif kind == "R":
            self.r1_frames += 1
            self.pending_t1 = None
            self.last_r1 = now
            if self.override is not True:
                self.override = True
                self.log("override ON: boiler gets TSet %.2f (%s)" % (value, self.since_ack(now)))

    def summary(self):
        self.close_hold(time.monotonic())
        self.log("summary: %d T1 frames, %d R1 frames (%.0f%% overridden)"
                 % (self.t1_frames, self.r1_frames,
                    100.0 * self.r1_frames / self.t1_frames if self.t1_frames else 0.0))
        for i, s in enumerate(self.held, 1):
            self.log("  hold %d: last R1 at %.1f s after its CS ack" % (i, s))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[1])
    ap.add_argument("--host", required=True)
    ap.add_argument("--port", type=int, default=25238, help="PIC serial bridge port")
    ap.add_argument("--api-port", type=int, default=80)
    ap.add_argument("--duration", type=int, default=600, help="seconds to watch")
    ap.add_argument("--send-cs", type=float, help="send CS=<value> once at start, CS=0 at the end")
    ap.add_argument("--log", help="also write the report to this file")
    args = ap.parse_args()

    out = open(args.log, "w", encoding="utf-8") if args.log else None
    tr = Tracker(out)
    sock = socket.create_connection((args.host, args.port), timeout=5)
    sock.settimeout(1.0)
    tr.log("connected to %s:%d, watching %d s" % (args.host, args.port, args.duration))
    if args.send_cs is not None:
        status = send_command(args.host, args.api_port, "CS=%.2f" % args.send_cs)
        tr.log("sent CS=%.2f (HTTP %s)" % (args.send_cs, status))

    buf = b""
    deadline = time.monotonic() + args.duration
    try:
        while time.monotonic() < deadline:
            try:
                chunk = sock.recv(4096)
            except socket.timeout:
                continue
            if not chunk:
                tr.log("bridge closed the connection")
                break
            buf += chunk
            while True:
                idx = min((i for i in (buf.find(b"\r"), buf.find(b"\n")) if i >= 0), default=-1)
                if idx < 0:
                    break
                raw, buf = buf[:idx], buf[idx + 1:]
                line = raw.decode("ascii", "replace").strip()
                if line:
                    tr.on_line(line)
    except KeyboardInterrupt:
        tr.log("stopped by user")
    finally:
        if args.send_cs is not None:
            status = send_command(args.host, args.api_port, "CS=0")
            tr.log("sent CS=0 to release the setpoint (HTTP %s)" % status)
        tr.summary()
        sock.close()
        if out:
            out.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
