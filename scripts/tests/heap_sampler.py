#!/usr/bin/env python3
"""Heap-fragmentation sampler for the OTGW-firmware 1.x (ESP8266) line.

This is the MEASUREMENT side of an A/B firmware test. It quantifies the
heap-fragmentation collapse where the largest CONTIGUOUS free block
(``maxBlock``) floor drops while total free heap still looks healthy, under
network load. That collapse is the failure mode that bricks the async/REST
paths long before "free heap" hits zero, because a 2 KB allocation can fail
while 14 KB free heap is scattered across tiny fragments.

  Two independent signals are captured and cross-checked:

  1. TELNET port 23 (PRIMARY).  Every debug line on the 1.x firmware is
     prefixed by Debug.h::_debugBOL() with the format
         "HH:MM:SS.uuuuuu (%7u|%6u) <fn>(<line>): ..."
     i.e. "( <freeheap>| <maxBlock>) ".  We parse EVERY such pair out of the
     stream.  The 2nd number is the largest contiguous block -- the floor we
     care about.  This stream keeps flowing right down to the edge, which is
     why it is the primary source.  (Verified: networkStuff.ino:24
     `SimpleTelnet<1> debugTelnet(23)`, prefix at Debug.h:121.)

  2. /api/v2/device/info JSON (CROSS-CHECK, ~every 5 s).  Carries
     `freeheap`, `maxfreeblock`, `hd_fragmentation_pct`, `bootcount`,
     `uptime` under a top-level "device" wrapper (restAPI.ino:1182-1184,
     1163, 1161; sendEndJsonMap(F("device")) at 1215).  This endpoint
     DEGRADES BY DESIGN: processAPI() returns 500 below 4096 free heap
     (restAPI.ino:866) and a min-block guard at 1123.  So it goes dark
     exactly when things get interesting -- that is *why* telnet is primary.
     A non-200 / unreachable snapshot is "no sample this tick", NEVER a
     crash on its own.

  CRASH DETECTION on ESP8266 -- read this before trusting the string-watch.
  The ESP8266 panic decoder writes the `Exception (N)` / `epc1=...` dump to
  UART0 (Serial), NOT to the telnet socket.  So watching the telnet text for
  'Exception'/'epc1' will usually catch NOTHING on a hard crash.  What you
  WILL observe instead is the telnet socket dropping (recv returns empty /
  connection reset) and, after the device recovers, `bootcount` incrementing
  in device/info.  Therefore:
     - the string-watch is kept (it cheaply catches a soft-WDT `DebugTln`
       warning now and then) but is a WEAK secondary signal;
     - the DEFINITIVE reboot signal is `bootcount` delta from the device/info
       poll (reported as `reboot_detected` / `bootcount_*`);
     - to actually DECODE an ESP8266 panic you must run a serial capture on
       the USB COM port in parallel:
           python scripts/tests/_serialcap.py COM4 <secs+30> heap-serial.log

Output (JSON, to --out or stdout): the specified keys
  {samples, maxblock_min, maxblock_p05, maxblock_median, free_min,
   exceptions_seen, duration_s}
plus a few extras that are cheap and load-bearing for the A/B verdict
  (maxblock_median_telnet vs _httpinfo source split, bootcount_start/end,
   reboot_detected, frag_max_pct, telnet_pairs, http_samples).

Importable AND runnable:
  >>> from heap_sampler import sample
  >>> r = sample("192.168.1.50", secs=60)      # no side effects on import
Standalone:
  python scripts/tests/heap_sampler.py --host 192.168.1.50 --secs 60 --out s.json

1.x specifics baked in: telnet on port 23 (not the async telnet), device/info
under a "device" wrapper, ~40 KB heap so NO ESP32-tuned thresholds are applied
here -- this tool only MEASURES and reports; it does not PASS/FAIL on absolute
heap numbers.  The one platform-independent hard signal is the bootcount delta.

Stdlib only.
"""
import argparse
import json
import re
import socket
import sys
import threading
import time
import urllib.error
import urllib.request

# Matches the maxBlock prefix on every 1.x debug line: "( <free>| <maxBlock>) ".
# The HH:MM:SS.uuuuuu timestamp in front is irrelevant -- we anchor on the
# parenthesised "(free|block)" pair, so this is robust to the timestamp and to
# the %7u/%6u field widths (leading spaces). 2nd group = maxBlock.
_HEAP_RE = re.compile(rb"\(\s*(\d+)\s*\|\s*(\d+)\s*\)")

# Soft signals occasionally emitted to the telnet stream itself. The hard
# ESP8266 panic (epc1/Exception) goes to UART0, not here -- see module docstring.
_EXC_RE = re.compile(rb"exception|watchdog|epc1|panic|abort|rst cause|soft wdt",
                     re.IGNORECASE)

DEVICE_INFO = "/api/v2/device/info"


def _percentile(sorted_vals, pct):
    """Nearest-rank percentile (pct in [0,100]) over an already-sorted list."""
    if not sorted_vals:
        return None
    if len(sorted_vals) == 1:
        return sorted_vals[0]
    k = (pct / 100.0) * (len(sorted_vals) - 1)
    lo = int(k)
    hi = min(lo + 1, len(sorted_vals) - 1)
    frac = k - lo
    return sorted_vals[lo] + (sorted_vals[hi] - sorted_vals[lo]) * frac


def _median(vals):
    if not vals:
        return None
    s = sorted(vals)
    n = len(s)
    mid = n // 2
    if n % 2:
        return s[mid]
    return (s[mid - 1] + s[mid]) / 2.0


def poll_device_info(host, timeout=5):
    """One device/info snapshot. Returns dict of heap keys, or None.

    None means "no sample" (degraded/busy/unreachable), NOT a crash. Only a
    bootcount delta proves a reboot. Mirrors test_load.get_heap's contract.
    """
    url = "http://%s%s" % (host, DEVICE_INFO)
    try:
        with urllib.request.urlopen(url, timeout=timeout) as r:
            if r.getcode() != 200:
                return None
            body = r.read()
    except (urllib.error.URLError, urllib.error.HTTPError, OSError):
        return None
    try:
        d = json.loads(body)["device"]
    except (ValueError, KeyError, TypeError):
        return None
    keys = ["freeheap", "maxfreeblock", "hd_fragmentation_pct",
            "hd_enter_low", "hd_enter_warning", "hd_enter_critical",
            "bootcount", "uptime"]
    return {k: d.get(k) for k in keys}


class _TelnetReader(threading.Thread):
    """Background reader of telnet:host:23. Extracts every (free|maxBlock) pair
    and notes any soft-exception keyword. Auto-reconnects if the socket drops
    (a drop is itself evidence under load, so we count reconnects)."""

    def __init__(self, host, port, stop_evt):
        super().__init__(daemon=True)
        self.host = host
        self.port = port
        self._stop = stop_evt
        self.lock = threading.Lock()
        self.free_vals = []      # list[int]
        self.block_vals = []     # list[int]  (maxBlock floor lives here)
        self.exceptions = []     # list[str]  soft signals seen on telnet
        self.connects = 0
        self.drops = 0
        self.connect_errors = 0

    def run(self):
        buf = b""
        while not self._stop.is_set():
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.settimeout(6.0)
            try:
                s.connect((self.host, self.port))
            except OSError:
                with self.lock:
                    self.connect_errors += 1
                s.close()
                self._stop.wait(2.0)
                continue
            with self.lock:
                self.connects += 1
            s.settimeout(1.0)
            buf = b""
            while not self._stop.is_set():
                try:
                    chunk = s.recv(4096)
                except socket.timeout:
                    continue
                except OSError:
                    break
                if chunk == b"":
                    # FIN/RST: server dropped us. Under heap pressure this is a
                    # meaningful event (possible reboot precursor), so count it.
                    with self.lock:
                        self.drops += 1
                    break
                buf += chunk
                # keep the buffer bounded; parse on the tail
                if len(buf) > 65536:
                    buf = buf[-8192:]
                self._consume(buf)
                # drop everything up to the last newline so we never re-match
                nl = buf.rfind(b"\n")
                if nl >= 0:
                    buf = buf[nl + 1:]
            try:
                s.close()
            except OSError:
                pass
        # final flush of any tail
        self._consume(buf)

    def _consume(self, data):
        with self.lock:
            for m in _HEAP_RE.finditer(data):
                self.free_vals.append(int(m.group(1)))
                self.block_vals.append(int(m.group(2)))
            for m in _EXC_RE.finditer(data):
                # store a small surrounding slice for context
                start = max(0, m.start() - 8)
                end = min(len(data), m.end() + 32)
                snippet = data[start:end].decode("ascii", "replace").strip()
                if snippet not in self.exceptions:
                    self.exceptions.append(snippet)

    def snapshot(self):
        with self.lock:
            return (list(self.free_vals), list(self.block_vals),
                    list(self.exceptions), self.connects, self.drops,
                    self.connect_errors)


def sample(host, secs=60, telnet_port=23, info_interval=5.0, verbose=False):
    """Capture heap signals for `secs` seconds. Returns the result dict.

    No process exit, no argv parsing -- safe to call from other code or a test.
    """
    stop = threading.Event()
    reader = _TelnetReader(host, telnet_port, stop)
    reader.start()

    http_blocks = []
    http_free = []
    http_frag = []
    boot_start = None
    boot_end = None
    info_samples = 0

    t0 = time.monotonic()
    next_info = 0.0
    try:
        while time.monotonic() - t0 < secs:
            now = time.monotonic() - t0
            if now >= next_info:
                next_info = now + info_interval
                info = poll_device_info(host)
                if info is not None:
                    info_samples += 1
                    if isinstance(info.get("maxfreeblock"), int):
                        http_blocks.append(info["maxfreeblock"])
                    if isinstance(info.get("freeheap"), int):
                        http_free.append(info["freeheap"])
                    if isinstance(info.get("hd_fragmentation_pct"), int):
                        http_frag.append(info["hd_fragmentation_pct"])
                    bc = info.get("bootcount")
                    if isinstance(bc, int):
                        if boot_start is None:
                            boot_start = bc
                        boot_end = bc
                    if verbose:
                        tf, tb, _, _, dr, _ = reader.snapshot()
                        print("  +%4ds free=%s maxblk=%s frag=%s%% boot=%s "
                              "telnet_pairs=%d drops=%d"
                              % (int(now), info.get("freeheap"),
                                 info.get("maxfreeblock"),
                                 info.get("hd_fragmentation_pct"), bc,
                                 len(tb), dr), flush=True)
                elif verbose:
                    print("  +%4ds device/info UNREACHABLE (degraded/busy "
                          "-- not a crash by itself)" % int(now), flush=True)
            stop.wait(0.5)
    finally:
        stop.set()
        reader.join(timeout=8)

    t_free, t_block, t_exc, connects, drops, conn_err = reader.snapshot()
    duration = round(time.monotonic() - t0, 1)

    # Combine telnet + http maxBlock samples for the headline floor/percentiles.
    all_blocks = t_block + http_blocks
    all_free = t_free + http_free
    sorted_blocks = sorted(all_blocks)

    boot_delta = (boot_end - boot_start
                  if isinstance(boot_start, int) and isinstance(boot_end, int)
                  else None)
    reboot_detected = bool(boot_delta) if boot_delta is not None else None

    result = {
        # --- the keys the task specifies ---
        "samples": len(all_blocks),
        "maxblock_min": min(all_blocks) if all_blocks else None,
        "maxblock_p05": (round(_percentile(sorted_blocks, 5), 1)
                         if sorted_blocks else None),
        "maxblock_median": _median(all_blocks),
        "free_min": min(all_free) if all_free else None,
        "exceptions_seen": t_exc,
        "duration_s": duration,
        # --- extras: cheap, and load-bearing for the A/B verdict ---
        "host": host,
        "telnet_pairs": len(t_block),
        "http_samples": info_samples,
        "maxblock_min_telnet": min(t_block) if t_block else None,
        "maxblock_min_httpinfo": min(http_blocks) if http_blocks else None,
        "frag_max_pct": max(http_frag) if http_frag else None,
        "telnet_connects": connects,
        "telnet_drops": drops,
        "telnet_connect_errors": conn_err,
        "bootcount_start": boot_start,
        "bootcount_end": boot_end,
        "bootcount_delta": boot_delta,
        # PRIMARY crash signal on ESP8266 (string-watch is only secondary).
        "reboot_detected": reboot_detected,
    }
    return result


def main(argv=None):
    ap = argparse.ArgumentParser(
        description="Sample ESP8266 heap/maxBlock under load (telnet:23 + "
                    "device/info), for OTGW-firmware 1.x A/B testing.")
    ap.add_argument("--host", required=True, help="device IP or hostname")
    ap.add_argument("--secs", type=float, default=60.0,
                    help="capture window in seconds (default 60)")
    ap.add_argument("--out", default=None,
                    help="write the JSON result here (default: stdout)")
    ap.add_argument("--telnet-port", type=int, default=23,
                    help="debug telnet port (1.x default 23)")
    ap.add_argument("--quiet", action="store_true",
                    help="suppress per-tick progress on stderr")
    args = ap.parse_args(argv)

    if not args.quiet:
        print("== heap_sampler (1.x ESP8266) ==", file=sys.stderr)
        print("host=%s secs=%.0f telnet=:%d  (primary=telnet maxBlock, "
              "cross-check=device/info)" % (args.host, args.secs,
                                            args.telnet_port), file=sys.stderr)
        print("NB: ESP8266 panics go to UART0, not telnet -- run _serialcap.py "
              "on COM for a real panic decode.", file=sys.stderr)

    result = sample(args.host, secs=args.secs, telnet_port=args.telnet_port,
                    verbose=not args.quiet)

    blob = json.dumps(result, indent=2)
    if args.out:
        with open(args.out, "w", encoding="utf-8") as f:
            f.write(blob + "\n")
        if not args.quiet:
            print("wrote %s" % args.out, file=sys.stderr)
    else:
        print(blob)

    # Exit code: 0 normally; 1 only if a reboot was definitively detected
    # (bootcount moved). Absolute heap numbers never gate this MEASUREMENT tool.
    if result.get("reboot_detected"):
        if not args.quiet:
            print("WARNING: bootcount moved %s -> %s during capture "
                  "(device REBOOTED -- decode the serial panic!)"
                  % (result["bootcount_start"], result["bootcount_end"]),
                  file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
