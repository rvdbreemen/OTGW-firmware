#!/usr/bin/env python3
"""Sustained network-overload generator for OTGW-firmware 1.x (ESP8266).

The LOAD side of the heap-fragmentation A/B test. Pair it with heap_sampler.py
(run them side by side, same host) to drive maxBlock down while you measure it.

Three load sources, each independently switchable:

  (a) Persistent WebSocket live-log subscribers.
      1.x runs a SEPARATE Links2004 `WebSocketsServer` on PORT 81
      (webSocketStuff.ino:35 `WebSocketsServer webSocket = WebSocketsServer(81)`),
      NOT a `ws://host/ws` route folded into the HTTP server like the 2.0.0
      async line. So the URL here is  ws://<host>:81/  -- a top-level WS, no
      path. The firmware broadcasts OT log lines to every connected client;
      that broadcast churns heap (~700 B/client + buffers), which is the WS
      contribution to fragmentation. Hard cap MAX_WEBSOCKET_CLIENTS = 3
      (webSocketStuff.ino:59) PLUS a low-heap reject (line 164): asking for
      >3 subscribers is fine -- the extra ones get actively rejected, which
      exercises the burst-window/reject path. Reconnect failures under heap
      pressure are SIGNAL, not harness bugs: they are counted, never fatal.

  (b) HTTP REST/asset flood (concurrent workers).
      On ESP8266 the dominant fragmenter is CONNECTION CONCURRENCY (overlapping
      lwIP/WiFiClient buffers), not which single JSON is biggest. So we just
      rotate ALL valid 1.x GETs across genuinely concurrent workers. The valid
      v2 routes were read from the dispatch table kV2Routes[] in restAPI.ino
      (lines 827-843): health, settings, sensors, device, otgw, discovery, ...
      There is NO /api/v2/sat/* and NO /api/v2/otdirect/* on 1.x -- those would
      404 -- so they are deliberately absent from FLOOD_ENDPOINTS.
      GET-only: every POST/PUT on v2 hits checkHttpAuth() (restAPI.ino:894) and
      would just 401, so there is no inbound-mutation worker here.

  (c) OPTIONAL MQTT churn (default OFF).
      With --broker <host>, connect/publish/disconnect in a loop to add the
      MQTT reconnect + (re)subscribe heap pressure the device sees in the
      field. Requires paho-mqtt; only imported when --broker is given.

Crash awareness (ESP8266): a hard panic dumps to UART0/Serial, not to any
socket. This tool does NOT itself decode crashes -- it generates load and
reports a coarse liveness read (device/info bootcount before/after). For the
authoritative reboot/heap timeline, run heap_sampler.py in parallel, and for a
panic DECODE run _serialcap.py on the COM port.

Safe to Ctrl-C at any time: a single KeyboardInterrupt stops every thread
cleanly and still prints the summary.

Importable AND runnable (no side effects on import):
  >>> from overload import run_overload
  >>> run_overload("192.168.1.50", minutes=2, http_workers=6, ws_subs=3)
Standalone:
  python scripts/tests/overload.py --host 192.168.1.50 --minutes 5 \
      --http-workers 8 --ws-subs 3 [--broker 192.168.1.234]

Uses stdlib for HTTP; `websocket-client` for WS (optional, only when ws-subs>0)
and `paho-mqtt` for the optional broker churn (only when --broker is given).
"""
import argparse
import json
import sys
import threading
import time
import urllib.error
import urllib.request

# --- Valid 1.x GET targets ---------------------------------------------------
# Static assets served from LittleFS by FSexplorer, plus the v2 REST routes
# confirmed present in kV2Routes[] (restAPI.ino:827-843). device/info and
# device/time are sub-routes of handleDevice (restAPI.ino:354-356);
# otgw/otmonitor is a sub-route of handleOtgw (restAPI.ino:441).
STATIC_ASSETS = ["/", "/index.html"]
REST_ENDPOINTS = [
    "/api/v2/health",
    "/api/v2/device/info",
    "/api/v2/device/time",
    "/api/v2/sensors",
    "/api/v2/settings",
    "/api/v2/otgw/otmonitor",
]
FLOOD_ENDPOINTS = STATIC_ASSETS + REST_ENDPOINTS

DEVICE_INFO = "/api/v2/device/info"
WS_PORT = 81  # 1.x WebSocketsServer port -- ws://host:81/


# --- shared run state --------------------------------------------------------
class _RunState:
    def __init__(self):
        self.stop = threading.Event()
        self.lock = threading.Lock()
        self.http_ok = 0
        self.http_fail = 0
        self.http_status = {}      # status_code -> count
        self.ws_open = 0
        self.ws_frames = 0
        self.ws_bytes = 0
        self.ws_reconnects = 0
        self.ws_rejected = 0       # connect attempts that the device refused
        self.ws_errors = 0
        self.mqtt_cycles = 0
        self.mqtt_errors = 0

    def bump(self, attr, n=1):
        with self.lock:
            setattr(self, attr, getattr(self, attr) + n)

    def note_status(self, code):
        with self.lock:
            self.http_status[code] = self.http_status.get(code, 0) + 1


def _snap(host, timeout=5):
    """device/info liveness snapshot. None = no read (degraded/busy/down)."""
    try:
        with urllib.request.urlopen("http://%s%s" % (host, DEVICE_INFO),
                                    timeout=timeout) as r:
            if r.getcode() != 200:
                return None
            d = json.loads(r.read())["device"]
            return {"boot": d.get("bootcount"), "heap": d.get("freeheap"),
                    "maxblock": d.get("maxfreeblock"),
                    "frag": d.get("hd_fragmentation_pct"),
                    "uptime": d.get("uptime")}
    except (urllib.error.URLError, urllib.error.HTTPError, OSError,
            ValueError, KeyError, TypeError):
        return None


# --- (b) HTTP flood ----------------------------------------------------------
def _http_worker(host, idx, st):
    i = idx
    n = len(FLOOD_ENDPOINTS)
    while not st.stop.is_set():
        path = FLOOD_ENDPOINTS[i % n]
        i += 1
        url = "http://%s%s" % (host, path)
        try:
            with urllib.request.urlopen(url, timeout=10) as r:
                r.read()
                st.note_status(r.getcode())
                st.bump("http_ok")
        except urllib.error.HTTPError as e:
            st.note_status(e.code)
            # a real HTTP status (e.g. 500 low-heap, restAPI.ino:866) means the
            # device answered -- it stayed alive. Count as "handled", not fail.
            st.bump("http_ok")
        except (urllib.error.URLError, OSError):
            st.bump("http_fail")   # timeout / connection error = true miss


# --- (a) persistent WebSocket subscribers (port 81) --------------------------
def _ws_subscriber(host, idx, st):
    try:
        import websocket  # websocket-client
        import socket as _socket
    except ImportError:
        st.bump("ws_errors")
        return  # logged once by the caller's preflight; thread just exits
    url = "ws://%s:%d/" % (host, WS_PORT)
    while not st.stop.is_set():
        try:
            ws = websocket.create_connection(url, timeout=8)
            ws.settimeout(1.0)
            st.bump("ws_open")
        except Exception:
            # Could be a low-heap reject, the >3-clients reject, or the device
            # being busy. All are SIGNAL under load, never fatal to the run.
            st.bump("ws_rejected")
            st.stop.wait(2.0)
            continue
        while not st.stop.is_set():
            try:
                msg = ws.recv()
            except (websocket.WebSocketTimeoutException, _socket.timeout):
                continue  # quiet second; connection still ALIVE -> hold steady (no false reconnect)
            except Exception:
                break     # real disconnect/error -> reconnect
            if msg is None or msg == "":
                break
            st.bump("ws_frames")
            st.bump("ws_bytes", len(msg) if msg else 0)
        try:
            ws.close()
        except Exception:
            pass
        if not st.stop.is_set():
            st.bump("ws_reconnects")
            st.stop.wait(0.5)


# --- (c) optional MQTT churn -------------------------------------------------
def _mqtt_churn(broker, port, st):
    try:
        import paho.mqtt.client as mqtt
    except ImportError:
        st.bump("mqtt_errors")
        return
    topic = "OTGW/overload/ping"
    i = 0
    while not st.stop.is_set():
        try:
            c = mqtt.Client()
            c.connect(broker, port, keepalive=15)
            c.loop_start()
            for _ in range(5):
                if st.stop.is_set():
                    break
                c.publish(topic, "%d" % i, qos=0)
                i += 1
                st.stop.wait(0.2)
            c.loop_stop()
            c.disconnect()
            st.bump("mqtt_cycles")
        except Exception:
            st.bump("mqtt_errors")
            st.stop.wait(1.0)


def run_overload(host, minutes=5.0, http_workers=8, ws_subs=3,
                 broker=None, broker_port=1883, verbose=True):
    """Run the load for `minutes`, return a summary dict. Importable / no exit.

    Ctrl-C is honoured: KeyboardInterrupt stops every thread and still returns
    the summary collected so far.
    """
    st = _RunState()
    seconds = max(minutes, 0.1) * 60.0

    # Preflight: warn loudly (once) if an optional dep for a REQUESTED feature
    # is missing, rather than letting threads silently no-op.
    if ws_subs > 0:
        try:
            import websocket  # noqa: F401
        except ImportError:
            print("ERROR: --ws-subs %d requested but 'websocket-client' is not "
                  "installed (pip install websocket-client). WS load disabled."
                  % ws_subs, file=sys.stderr)
            ws_subs = 0
    if broker:
        try:
            import paho.mqtt.client  # noqa: F401
        except ImportError:
            print("ERROR: --broker requested but 'paho-mqtt' is not installed "
                  "(pip install paho-mqtt). MQTT churn disabled.",
                  file=sys.stderr)
            broker = None

    base = _snap(host)
    if verbose:
        print("== overload (1.x ESP8266) ==", flush=True)
        print("host=%s minutes=%.1f http_workers=%d ws_subs=%d(@:%d) broker=%s"
              % (host, minutes, http_workers, ws_subs, WS_PORT,
                 broker or "off"), flush=True)
        print("baseline: %s" % base, flush=True)
        if ws_subs > 3:
            print("note: 1.x caps WS clients at 3 (webSocketStuff.ino:59); "
                  "the extra %d subscriber(s) will be actively rejected -- "
                  "that exercises the reject path, it is not an error."
                  % (ws_subs - 3), flush=True)

    threads = []
    for k in range(ws_subs):
        threads.append(threading.Thread(target=_ws_subscriber,
                                        args=(host, k, st), daemon=True))
    for k in range(http_workers):
        threads.append(threading.Thread(target=_http_worker,
                                        args=(host, k, st), daemon=True))
    if broker:
        threads.append(threading.Thread(target=_mqtt_churn,
                                        args=(broker, broker_port, st),
                                        daemon=True))
    for t in threads:
        t.start()

    t0 = time.monotonic()
    floor = base
    interrupted = False
    try:
        while time.monotonic() - t0 < seconds:
            time.sleep(5)
            s = _snap(host)
            if s and isinstance(s.get("maxblock"), int):
                if (floor is None or not isinstance(floor.get("maxblock"), int)
                        or s["maxblock"] < floor["maxblock"]):
                    floor = s
            if verbose:
                with st.lock:
                    ho, hf = st.http_ok, st.http_fail
                    wf, wr, wj = st.ws_frames, st.ws_reconnects, st.ws_rejected
                if s:
                    print("  +%4ds boot=%s heap=%s maxblk=%s frag=%s%% | "
                          "http_ok=%d http_fail=%d ws_frames=%d reconn=%d "
                          "ws_rej=%d"
                          % (int(time.monotonic() - t0), s["boot"], s["heap"],
                             s["maxblock"], s["frag"], ho, hf, wf, wr, wj),
                          flush=True)
                else:
                    print("  +%4ds device/info UNREACHABLE (degraded/busy "
                          "under load -- not a crash by itself) | http_ok=%d "
                          "http_fail=%d ws_frames=%d reconn=%d"
                          % (int(time.monotonic() - t0), ho, hf, wf, wr),
                          flush=True)
    except KeyboardInterrupt:
        interrupted = True
        if verbose:
            print("\n^C -- stopping load...", flush=True)
    finally:
        st.stop.set()
        for t in threads:
            t.join(timeout=8)

    time.sleep(2)
    final = _snap(host)
    boot_delta = None
    if base and final and isinstance(base.get("boot"), int) \
            and isinstance(final.get("boot"), int):
        boot_delta = final["boot"] - base["boot"]

    with st.lock:
        summary = {
            "host": host,
            "interrupted": interrupted,
            "duration_s": round(time.monotonic() - t0, 1),
            "http_ok": st.http_ok,
            "http_fail": st.http_fail,
            "http_status": dict(st.http_status),
            "ws_open": st.ws_open,
            "ws_frames": st.ws_frames,
            "ws_bytes": st.ws_bytes,
            "ws_reconnects": st.ws_reconnects,
            "ws_rejected": st.ws_rejected,
            "ws_errors": st.ws_errors,
            "mqtt_cycles": st.mqtt_cycles,
            "mqtt_errors": st.mqtt_errors,
            "baseline": base,
            "maxblock_floor": floor,
            "final": final,
            "bootcount_delta": boot_delta,
            "reboot_detected": (bool(boot_delta)
                                if boot_delta is not None else None),
        }

    if verbose:
        print("\n== RESULTS ==", flush=True)
        print("HTTP: ok/handled=%d fail=%d  statuses=%s"
              % (summary["http_ok"], summary["http_fail"],
                 summary["http_status"]), flush=True)
        print("WS:   opens=%d frames=%d bytes=%d reconnects=%d rejected=%d"
              % (summary["ws_open"], summary["ws_frames"], summary["ws_bytes"],
                 summary["ws_reconnects"], summary["ws_rejected"]), flush=True)
        if broker:
            print("MQTT: cycles=%d errors=%d"
                  % (summary["mqtt_cycles"], summary["mqtt_errors"]),
                  flush=True)
        print("baseline    : %s" % base, flush=True)
        print("maxblock floor: %s" % floor, flush=True)
        print("final       : %s" % final, flush=True)
        print("bootcount delta: %s" % boot_delta, flush=True)
        if boot_delta == 0:
            print("liveness: no reboot detected during load.", flush=True)
        elif boot_delta is None:
            print("liveness: INCONCLUSIVE -- could not read bootcount "
                  "(device may be hung/busy; check serial).", flush=True)
        else:
            print("liveness: REBOOT -- bootcount moved %s (decode serial "
                  "panic with _serialcap.py)." % boot_delta, flush=True)

    return summary


def main(argv=None):
    ap = argparse.ArgumentParser(
        description="Sustained WS+HTTP(+MQTT) overload for OTGW-firmware 1.x "
                    "(ESP8266). Run alongside heap_sampler.py.")
    ap.add_argument("--host", required=True, help="device IP or hostname")
    ap.add_argument("--minutes", type=float, default=5.0,
                    help="load window in minutes (default 5)")
    ap.add_argument("--http-workers", type=int, default=8,
                    help="concurrent HTTP GET flood workers (default 8; 0=off)")
    ap.add_argument("--ws-subs", type=int, default=3,
                    help="persistent WS subscribers on ws://host:81/ "
                         "(default 3; 1.x caps real clients at 3)")
    ap.add_argument("--broker", default=None,
                    help="optional MQTT broker host for connect/publish churn "
                         "(default off; needs paho-mqtt)")
    ap.add_argument("--broker-port", type=int, default=1883,
                    help="MQTT broker port (default 1883)")
    ap.add_argument("--out", default=None,
                    help="also write the JSON summary here")
    args = ap.parse_args(argv)

    summary = run_overload(args.host, minutes=args.minutes,
                           http_workers=args.http_workers,
                           ws_subs=args.ws_subs, broker=args.broker,
                           broker_port=args.broker_port, verbose=True)

    if args.out:
        with open(args.out, "w", encoding="utf-8") as f:
            f.write(json.dumps(summary, indent=2) + "\n")
        print("wrote %s" % args.out, file=sys.stderr)

    # Exit 1 only on a definitively detected reboot; otherwise 0. This is a load
    # generator, not a pass/fail gate on absolute heap numbers.
    return 1 if summary.get("reboot_detected") else 0


if __name__ == "__main__":
    sys.exit(main())
