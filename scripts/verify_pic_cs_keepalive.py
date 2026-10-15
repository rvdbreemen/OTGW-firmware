#!/usr/bin/env python3
"""
verify_pic_cs_keepalive.py - bench verification for TASK-1236 on a PIC board

Runs against a Classic (PIC) board with port 25238 enabled and no boiler or
thermostat on the bus. With nothing on the bus the PIC runs stand-alone and
sends a MsgID 0 status request every second. Its CH-enable bit (R80000300 vs
R80000200) shows whether a user CS= is active, as long as no CH= command
overrides that bit. SAT does send CH=, so with SAT on the evidence is the
cadence of the PIC's "CS: x" acknowledgements instead.

Tests (each prints PASS/FAIL with the evidence lines):
  T1  mechanism, SAT off: CS=45 re-sent every 30 s keeps the CH bit set; after
      the last CS= the PIC drops it 63-130 s later.
  T2  SAT on: a "CS:" ack at least every 35 s for --hold seconds.
  T3  SAT on: an external CS=45 is refused (no "CS: 45" ack within 15 s).
  T4  SAT off again: SAT's release CS=0 reaches the PIC, then no more CS acks.
  T5  SAT off: an external CS=45 is accepted again (ack within 10 s), then CS=0.

The script switches satenabled itself and restores it to off at the end.

  python scripts/verify_pic_cs_keepalive.py --host 192.168.88.63 --hold 660 --log out.txt
"""

import argparse
import json
import re
import socket
import sys
import threading
import time
import urllib.request

ACK_RE = re.compile(r"^CS:\s*(-?\d+(?:\.\d+)?)")


class Stream:
    def __init__(self, host, port, log):
        self.lines = []          # (t, line)
        self.lock = threading.Lock()
        self.t0 = time.monotonic()
        self.log_f = log
        self.sock = socket.create_connection((host, port), timeout=5)
        self.sock.settimeout(1.0)
        self.alive = True
        threading.Thread(target=self._run, daemon=True).start()

    def now(self):
        return time.monotonic() - self.t0

    def note(self, msg):
        line = "%7.1f s  %s" % (self.now(), msg)
        print(line, flush=True)
        if self.log_f:
            self.log_f.write(line + "\n")
            self.log_f.flush()

    def _run(self):
        buf = b""
        while self.alive:
            try:
                chunk = self.sock.recv(4096)
            except socket.timeout:
                continue
            except OSError:
                break
            if not chunk:
                break
            buf += chunk
            while True:
                idx = min((i for i in (buf.find(b"\r"), buf.find(b"\n")) if i >= 0), default=-1)
                if idx < 0:
                    break
                raw, buf = buf[:idx], buf[idx + 1:]
                line = raw.decode("ascii", "replace").strip()
                if line:
                    with self.lock:
                        self.lines.append((self.now(), line))
                    if not (len(line) == 9 and line[0] in "TBRA" and line[3:5] == "00"):
                        self.note("stream: " + line)

    def since(self, t):
        with self.lock:
            return [(ts, l) for ts, l in self.lines if ts >= t]

    def id0(self, t_from, t_to):
        with self.lock:
            return [(ts, l) for ts, l in self.lines
                    if t_from <= ts <= t_to and len(l) == 9 and l[0] == "R" and l[3:5] == "00"]

    def acks(self, t_from, t_to=1e9):
        with self.lock:
            return [(ts, float(m.group(1))) for ts, l in self.lines
                    if t_from <= ts <= t_to for m in [ACK_RE.match(l)] if m]


def post(host, path, obj):
    req = urllib.request.Request("http://%s%s" % (host, path), data=json.dumps(obj).encode(),
                                 method="POST", headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=5) as r:
        return r.status


def get(host, path):
    with urllib.request.urlopen("http://%s%s" % (host, path), timeout=5) as r:
        return json.loads(r.read().decode())


def cmd(host, c):
    return post(host, "/api/v2/otgw/commands", {"command": c})


def setting(host, name, value):
    return post(host, "/api/v2/settings", {"name": name, "value": value})


def ch_bit(line):
    # Frame "R80000300": type, byte0 (parity/msgtype), byte1 (DataID), HB, LB.
    # The master status flags of MsgID 0 are in the high byte; bit 0 = CH enable.
    return int(line[5:7], 16) & 1


results = []


def verdict(st, name, ok, why):
    results.append((name, ok))
    st.note("%s %s: %s" % ("PASS" if ok else "FAIL", name, why))


def t1_mechanism(st, host):
    st.note("T1: SAT off, CS=45 refreshed every 30 s for 150 s, then left to expire")
    setting(host, "satenabled", False)
    time.sleep(5)
    # SAT sends CH=0 at its cold setpoint, and the PIC keeps that: CHModeOff then
    # masks the CH bit even while a CS is active (gateway.asm readstatus). CH=1 is
    # the PIC default and lets the bit follow the CS state again.
    cmd(host, "CH=1")
    time.sleep(3)
    start = st.now()
    for _ in range(6):
        cmd(host, "CS=45")
        time.sleep(30)
    last_send = st.now() - 30
    held = st.id0(start + 5, last_send + 25)
    held_ok = bool(held) and all(ch_bit(l) for _, l in held)
    time.sleep(140)
    after = st.id0(last_send, st.now())
    drop = next((ts for ts, l in after if not ch_bit(l)), None)
    cmd(host, "CS=0")
    acks = st.acks(start)
    last_ack = acks[-1][0] if acks else last_send
    lag = (drop - last_ack) if drop is not None else None
    verdict(st, "T1a CH bit held while refreshed", held_ok,
            "%d status frames, all with CH bit set: %s" % (len(held), held_ok))
    verdict(st, "T1b expiry after the last CS", lag is not None and 60 <= lag <= 130,
            "CH bit dropped %s s after the last ack" % ("%.1f" % lag if lag is not None else "never"))


def t2_keepalive(st, host, hold):
    st.note("T2: SAT on, watching CS acks for %d s" % hold)
    setting(host, "satenabled", True)
    time.sleep(40)                       # SAT activates on its first control tick
    start = st.now()
    time.sleep(hold)
    acks = st.acks(start, start + hold)
    times = [start] + [t for t, _ in acks] + [start + hold]
    max_gap = max(b - a for a, b in zip(times, times[1:]))
    values = sorted({v for _, v in acks})
    verdict(st, "T2 SAT CS refreshed within the PIC window", len(acks) >= hold // 35 and max_gap <= 35,
            "%d acks in %d s, longest gap %.1f s, values %s" % (len(acks), hold, max_gap, values))


def t3_refuse(st, host):
    st.note("T3: SAT on, external CS=45 must be refused")
    t = st.now()
    status = cmd(host, "CS=45")
    time.sleep(15)
    bad = [v for _, v in st.acks(t) if abs(v - 45.0) < 0.01]
    verdict(st, "T3 external CS refused while SAT owns MsgID 1", not bad,
            "HTTP %s, CS: 45 acks seen: %d" % (status, len(bad)))


def t4_release(st, host):
    st.note("T4: SAT off, release CS=0 must reach the PIC, keepalive must stop")
    t = st.now()
    setting(host, "satenabled", False)
    time.sleep(20)
    zero = [v for _, v in st.acks(t) if abs(v) < 0.01]
    t_after = st.now()
    time.sleep(70)
    later = st.acks(t_after)
    verdict(st, "T4a SAT release CS=0 acknowledged", bool(zero), "CS: 0 acks: %d" % len(zero))
    verdict(st, "T4b keepalive stopped after disable", not later,
            "CS acks in the 70 s after disable: %d" % len(later))


def t5_accept(st, host):
    st.note("T5: SAT off, external CS=45 must be accepted again")
    t = st.now()
    cmd(host, "CS=45")
    time.sleep(10)
    ok = [v for _, v in st.acks(t) if abs(v - 45.0) < 0.01]
    cmd(host, "CS=0")
    verdict(st, "T5 external CS accepted with SAT off", bool(ok), "CS: 45 acks: %d" % len(ok))


def main():
    ap = argparse.ArgumentParser(description="TASK-1236 bench verification on a PIC board")
    ap.add_argument("--host", required=True)
    ap.add_argument("--port", type=int, default=25238)
    ap.add_argument("--hold", type=int, default=660, help="T2 watch time in seconds")
    ap.add_argument("--only", help="comma-separated subset, e.g. t1,t3")
    ap.add_argument("--log")
    args = ap.parse_args()
    log = open(args.log, "w", encoding="utf-8") if args.log else None
    st = Stream(args.host, args.port, log)
    raw = json.dumps(get(args.host, "/api/v2/device/info"))
    m = re.search(r'"fwversion": "([^"]+)"', raw)
    st.note("device %s, fw %s" % (args.host, m.group(1) if m else "?"))
    try:
        only = set(args.only.lower().split(",")) if args.only else None
        steps = [("t1", lambda: t1_mechanism(st, args.host)),
                 ("t2", lambda: t2_keepalive(st, args.host, args.hold)),
                 ("t3", lambda: t3_refuse(st, args.host)),
                 ("t4", lambda: t4_release(st, args.host)),
                 ("t5", lambda: t5_accept(st, args.host))]
        for key, fn in steps:
            if only is None or key in only:
                fn()
    finally:
        setting(args.host, "satenabled", False)
        st.alive = False
        st.note("summary: %d/%d passed" % (sum(ok for _, ok in results), len(results)))
        for name, ok in results:
            st.note("  %s  %s" % ("PASS" if ok else "FAIL", name))
        if log:
            log.close()
    return 0 if results and all(ok for _, ok in results) else 1


if __name__ == "__main__":
    sys.exit(main())
