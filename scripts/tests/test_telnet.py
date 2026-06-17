#!/usr/bin/env python3
"""ACTIVE telnet/ser2net probe for the OTGW32 (separate from the passive monitors).

Turns "the telnet just does not work" into measured facts, one TCP layer at a time:

  L1 CONNECT      can we open a TCP socket to the port at all? (accept works?)
  L2 BANNER       does the server deliver bytes after connect, and how many / how
                  fast? (onConnect banner emission + TX path)
  L3 STABILITY    does the session stay open for the hold window, or does the
                  server drop it (RST/FIN) on its own?
  L4 ECHO/INPUT   (port 23 only, optional) send a keystroke, see if the input path
                  reacts (onInputReceived) without dropping the session.
  L5 REPEAT       N rapid connect/close cycles — exposes the per-disconnect leak /
                  slot-exhaustion (AsyncSimpleTelnet evicts at MAX_CLIENTS).

Port 23 = debug console (AsyncSimpleTelnet<1>). Port 25238 = ser2net/OTmonitor raw
bridge (AsyncSimpleTelnet<2>, NEG_OFF). Host comes from _secrets.py.

Usage (standalone or via otgw-test.py):
  python scripts/tests/test_telnet.py --minutes 1
  python scripts/tests/test_telnet.py --host 192.168.88.39 --port 23 --cycles 10
"""
import argparse
import os
import socket
import sys
import time
from datetime import datetime

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
try:
    import _secrets
except Exception:
    _secrets = None


def ts():
    return datetime.now().strftime("%H:%M:%S.%f")[:-3]


def one_session(host, port, hold, send_key, connect_timeout=6.0):
    """Open one TCP session; return a dict of measured facts."""
    r = {"connect_ok": False, "connect_s": None, "first_byte_s": None,
         "bytes": 0, "closed_by_server_s": None, "echo_bytes": 0, "error": ""}
    t0 = time.monotonic()
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(connect_timeout)
    try:
        s.connect((host, port))
        r["connect_ok"] = True
        r["connect_s"] = time.monotonic() - t0
    except Exception as e:
        r["error"] = f"connect:{type(e).__name__}"
        s.close()
        return r

    s.settimeout(0.5)
    deadline = time.monotonic() + hold
    first = None
    sent = False
    while time.monotonic() < deadline:
        # Send a keystroke once, partway through, to probe the input path.
        if send_key and not sent and (time.monotonic() - t0) > min(1.0, hold / 3):
            try:
                s.sendall(b"h")  # 'h' = help menu on the debug console
                sent = True
            except Exception as e:
                r["error"] = r["error"] or f"send:{type(e).__name__}"
                break
        try:
            chunk = s.recv(2048)
        except socket.timeout:
            continue
        except Exception as e:
            r["error"] = r["error"] or f"recv:{type(e).__name__}"
            break
        if chunk == b"":
            # server closed the connection (FIN)
            r["closed_by_server_s"] = time.monotonic() - t0
            break
        if first is None:
            first = time.monotonic()
            r["first_byte_s"] = first - t0
        r["bytes"] += len(chunk)
        if sent:
            r["echo_bytes"] += len(chunk)
    try:
        s.close()
    except Exception:
        pass
    return r


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--minutes", type=float, default=1.0)
    ap.add_argument("--host", default=None)
    ap.add_argument("--port", type=int, default=23)
    ap.add_argument("--hold", type=float, default=8.0, help="seconds to hold each session")
    ap.add_argument("--cycles", type=int, default=6, help="rapid connect/close cycles for L5")
    ap.add_argument("--no-input", action="store_true", help="skip the L4 keystroke probe")
    args = ap.parse_args()

    host = args.host or (_secrets.get("device_host", "OTGW.local") if _secrets else "OTGW.local")
    # prefer the literal IP if the host is a .local that may resolve slowly
    print(f"# telnet test  host={host} port={args.port}  hold={args.hold}s  cycles={args.cycles}")

    # --- L1-L4: one held session with input probe ---
    send_key = (args.port == 23) and (not args.no_input)
    print(f"{ts()} [L1-4] held session (send_key={send_key})...")
    r = one_session(host, args.port, args.hold, send_key)
    print(f"{ts()}   connect_ok={r['connect_ok']} connect_s={r['connect_s']}")
    print(f"{ts()}   first_byte_s={r['first_byte_s']} banner_bytes={r['bytes']}")
    print(f"{ts()}   closed_by_server_s={r['closed_by_server_s']} echo_bytes={r['echo_bytes']} err={r['error']}")

    # --- L5: rapid connect/close cycles (leak / slot exhaustion) ---
    print(f"{ts()} [L5] {args.cycles} rapid connect/close cycles...")
    ok = 0
    banners = 0
    for i in range(args.cycles):
        c = one_session(host, args.port, 2.0, False)
        if c["connect_ok"]:
            ok += 1
        if c["bytes"] > 0:
            banners += 1
        print(f"{ts()}   cycle {i+1:2}: connect={c['connect_ok']} bytes={c['bytes']} "
              f"closed_by_server_s={c['closed_by_server_s']} err={c['error']}")
        time.sleep(0.3)

    # --- verdict ---
    print(f"\n# SUMMARY held: connect={r['connect_ok']} banner_bytes={r['bytes']} "
          f"server_closed={r['closed_by_server_s'] is not None}")
    print(f"# SUMMARY cycles: {ok}/{args.cycles} connected, {banners}/{args.cycles} got a banner")
    healthy = (r["connect_ok"] and r["bytes"] > 100
               and r["closed_by_server_s"] is None
               and ok == args.cycles and banners == args.cycles)
    if not r["connect_ok"]:
        print("# VERDICT: BROKEN - cannot even connect (accept path dead)")
        return 2
    if r["bytes"] == 0:
        print("# VERDICT: BROKEN - connects but NO banner (TX/onConnect path)")
        return 1
    if r["closed_by_server_s"] is not None:
        print(f"# VERDICT: UNSTABLE - server dropped the held session at {r['closed_by_server_s']:.1f}s")
        return 1
    if ok < args.cycles or banners < args.cycles:
        print("# VERDICT: FLAKY - some connect/close cycles failed (leak/slot exhaustion?)")
        return 1
    print("# VERDICT: healthy" if healthy else "# VERDICT: degraded")
    return 0 if healthy else 1


if __name__ == "__main__":
    sys.exit(main())
