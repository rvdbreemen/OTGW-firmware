#!/usr/bin/env python3
"""ADR-185 / TASK-1199 bench check: internal heap with the BLE scan running, and does the scan still see advertisers?

    python scripts/tests/ble_heap_bench.py --host <ip> --label <name> --out <file.jsonl>

Run it once on the old build and once on the new build: same board (the OTGW32
bench unit has no PSRAM), same place, nothing else using the device. It changes
two settings and reboots the device, then puts the settings back. Every HTTP
request waits for the previous one (the REST gate allows 2 in flight).

Steps:
1. Saves GET /api/v2/settings to <out>.settings.json; the restore step uses it.
2. Turns BLE on with POST /api/v2/settings {"name":"satbleenable","value":true}
   and, on a board without PSRAM, {"name":"satbleriskack","value":true}. BLE only
   runs when satbleenable is set and the board has PSRAM or satbleriskack is set
   (ADR-169 consent gate, bleActive() in SATble.ino). Booleans go as unquoted JSON;
   both values are read back from GET /api/v2/settings.
3. Reboots through GET /ReBoot and waits until GET /api/v2/device/info answers
   with a higher bootcount than before.
4. NimBLE init: GET /api/v2/sat/ble/discovery must report "active": true. That
   field is bleActive(). While it is true, satBLEInit() runs at boot (SAT init in
   SATcontrol.ino) and from satBLELoop() on the first loop pass; it calls
   NimBLEDevice::init("") and starts the passive scan. No REST field reports the
   result of NimBLEDevice::init() itself (satBLEInit ignores its return value), so
   step 6 is the proof that the stack runs.
5. Heap: anchors the device clock with runtime.uptime_sec from GET /api/v2/debug,
   then reads GET /api/v2/device/info at uptime 60, 70, ... 180 s. It records
   internal_free and internal_maxblk (heap_caps_get_free_size and
   heap_caps_get_largest_free_block for MALLOC_CAP_INTERNAL) plus freeheap and
   maxfreeblock. The summary gives the median and the minimum of each.
6. Advertisers, after the heap window so the reads do not disturb it:
   - Any advertiser (the acceptance observable). On the telnet debug port (23),
     key '7' toggles state.debug.bSATBLE. satBLELoop() then prints once per
     satbleinterval seconds (default 30): "SAT BLE: <n>s window: <ads> ads, ...".
     <ads> is _bleAdCount: onResult() adds 1 for every advertisement report the
     passive scan delivers, before any parsing. ads > 0 in a window therefore
     proves that the scan reports advertisers, of ANY type: a phone or a TV
     counts. The script reads two windows, then sets the flag back.
   - Parsed thermometers only (supporting). GET /api/v2/sat/ble/discovery lists
     the roster. "valid": true means an advertisement of that sensor was decoded
     during this boot and in the last 5 minutes (runtime data starts empty at
     boot). It needs an ATC/pvvx (0x181A), BTHome v2 (0xFCD2) or Xiaomi MiBeacon
     (0xFE95) sensor in range; an empty or stale roster says nothing about the scan.
   --no-telnet skips the telnet part.
   The summary field ble_running_proven is true only when a window showed ads > 0
   or the roster has a valid sensor. Without that proof the heap numbers answer
   nothing: a stack that failed to start would look like a large saving. Compare
   an old and a new run only when both have ble_running_proven true.
7. Restores satbleenable and satbleriskack from the saved settings. When BLE was
   not running before the test, it reboots once more: turning BLE off does not
   deinit NimBLE, only a reboot frees it.

Output: one JSON line per event and per sample, then a summary line. Written to
--out and printed. Exit 0 when the run completed and BLE was proven to run
(whatever the heap numbers), 1 when a step failed or ble_running_proven is false.
The restore step runs in either case. The device must not have an HTTP admin
password, or pass it with --password (HTTP Basic, user admin).
"""
import argparse
import base64
import json
import re
import socket
import statistics
import time
import urllib.error
import urllib.request

MARKS = list(range(60, 181, 10))          # device uptime (s) at which the heap is read
WINDOW_RE = re.compile(r"SAT BLE: (\d+)s window: (\d+) ads, (\d+) accepted, (\d+) filter-rej, "
                       r"(\d+) unknown, (\d+) no-slot")
TOGGLE_RE = re.compile(r"Debug SAT BLE: (true|false)")


class Bench:
    def __init__(self, host, password, out):
        self.host = host
        self.auth = None
        if password:
            self.auth = "Basic " + base64.b64encode(f"admin:{password}".encode()).decode()
        self.out = out
        self.log = open(out, "w", encoding="utf-8")
        self.t0 = time.monotonic()
        self.errors = []

    def emit(self, rec):
        rec = {"t": round(time.monotonic() - self.t0, 1), **rec}
        self.log.write(json.dumps(rec) + "\n")
        self.log.flush()
        print(json.dumps(rec))

    def http(self, method, path, body=None, timeout=10, retries=20):
        """One request at a time; retries on 503 (gate busy, low heap) and on network errors.

        20 retries ride out about 4 minutes of silence: with BLE on, a no-PSRAM board on the
        framework NimBLE config was seen deaf to REST for about 3 minutes (TASK-1199 notes)."""
        headers = {}
        data = None
        if body is not None:
            data = json.dumps(body).encode()
            headers["Content-Type"] = "application/json"
        if self.auth:
            headers["Authorization"] = self.auth
        last = None
        for attempt in range(retries):
            req = urllib.request.Request(f"http://{self.host}{path}", data=data, method=method, headers=headers)
            try:
                with urllib.request.urlopen(req, timeout=timeout) as r:
                    return r.read().decode("utf-8", "replace")
            except urllib.error.HTTPError as e:
                last = e
                if e.code != 503:
                    raise
            except (urllib.error.URLError, OSError) as e:
                last = e
            if attempt < retries - 1:
                time.sleep(2)
        raise last

    def get_json(self, path, **kw):
        return json.loads(self.http("GET", path, **kw))

    def settings(self):
        return self.get_json("/api/v2/settings")["settings"]

    def set_bool(self, name, value):
        self.http("POST", "/api/v2/settings", {"name": name, "value": bool(value)})

    def bootcount(self, timeout=10, retries=20):
        return self.get_json("/api/v2/device/info", timeout=timeout, retries=retries)["device"]["bootcount"]

    def reboot_and_wait(self, limit_s=180):
        before = self.bootcount()
        try:
            self.http("GET", "/ReBoot", retries=1)   # no retry: a second request could reboot twice
        except Exception as e:  # noqa: BLE001 - the restart may cut the response; the bootcount decides
            self.emit({"event": "reboot_response_lost", "error": str(e)})
        self.emit({"event": "reboot", "bootcount_before": before})
        time.sleep(5)
        deadline = time.monotonic() + limit_s
        while time.monotonic() < deadline:
            try:
                now = self.bootcount(timeout=4, retries=1)
                if now > before:
                    self.emit({"event": "back", "bootcount": now})
                    return
            except Exception:  # noqa: BLE001 - the device is rebooting
                pass
            time.sleep(2)
        raise RuntimeError(f"device not back within {limit_s} s after /ReBoot")


def telnet_ad_windows(host, interval_s, want=2):
    """Toggle bSATBLE on via key '7', collect `want` scan-stat windows, toggle it back."""
    s = socket.create_connection((host, 23), timeout=5)
    s.settimeout(1.0)
    buf = ""

    def read_for(seconds, pattern):
        nonlocal buf
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            m = pattern.search(buf)
            if m:
                buf = buf[m.end():]
                return m
            try:
                chunk = s.recv(4096)
            except socket.timeout:
                continue
            if not chunk:
                raise ConnectionError("telnet closed")
            buf = (buf + chunk.decode("ascii", "replace"))[-20000:]
        return None

    try:
        end = time.monotonic() + 2              # let the connect banner arrive, then drop it
        while time.monotonic() < end:
            try:
                if not s.recv(4096):
                    raise ConnectionError("telnet closed")
            except socket.timeout:
                pass
        s.sendall(b"7")
        m = read_for(5, TOGGLE_RE)
        if not m:
            raise RuntimeError("no 'Debug SAT BLE' reply to key 7")
        was_on = m.group(1) == "false"         # the first press turned it off: it was on
        if was_on:
            s.sendall(b"7")
            if not read_for(5, TOGGLE_RE):
                raise RuntimeError("no reply to the second key 7")
        windows = []
        deadline = time.monotonic() + interval_s * (want + 1) + 15
        while len(windows) < want and time.monotonic() < deadline:
            m = read_for(max(1.0, deadline - time.monotonic()), WINDOW_RE)
            if m:
                w, ads, acc, rej, unk, noslot = (int(x) for x in m.groups())
                windows.append({"window_s": w, "ads": ads, "accepted": acc, "filter_rej": rej,
                                "unknown": unk, "no_slot": noslot})
        if not was_on:
            s.sendall(b"7")                    # back to off, as found
            read_for(5, TOGGLE_RE)
        return windows
    finally:
        s.close()


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--host", required=True, help="device IP; never a default host")
    ap.add_argument("--label", default="run", help="label stored in the summary, e.g. old or new")
    ap.add_argument("--out", required=True, help="JSON-lines output file")
    ap.add_argument("--password", default="", help="HTTP admin password, if one is set")
    ap.add_argument("--no-telnet", action="store_true", help="skip the any-advertiser count on telnet")
    args = ap.parse_args()

    b = Bench(args.host, args.password, args.out)
    summary = {"label": args.label, "host": args.host}
    original = None
    ble_was_active = None
    samples = []
    try:
        info = b.get_json("/api/v2/device/info")["device"]
        psram = int(info.get("psram_found", 0))
        summary.update({"fwversion": info.get("fwversion"), "psram_found": psram})
        original = b.settings()
        with open(args.out + ".settings.json", "w", encoding="utf-8") as f:
            json.dump(original, f, indent=1)
        orig_en = bool(original["satbleenable"]["value"])
        orig_ack = bool(original["satbleriskack"]["value"])
        ble_was_active = orig_en and (psram == 1 or orig_ack)
        interval = int(original.get("satbleinterval", {}).get("value", 30))
        b.emit({"event": "start", "fwversion": info.get("fwversion"), "psram_found": psram,
                "satbleenable": orig_en, "satbleriskack": orig_ack, "ble_was_active": ble_was_active})

        b.set_bool("satbleenable", True)
        if psram == 0:
            b.set_bool("satbleriskack", True)
        now = b.settings()
        if not now["satbleenable"]["value"] or (psram == 0 and not now["satbleriskack"]["value"]):
            raise RuntimeError("BLE settings did not take (read back false)")
        time.sleep(3)                          # 2 s settings-flush debounce (doRestart flushes too)

        b.reboot_and_wait()
        t_req = time.monotonic()
        dbg = b.get_json("/api/v2/debug")["debug"]
        t_anchor = (t_req + time.monotonic()) / 2
        up0 = int(dbg["runtime.uptime_sec"])
        disc = b.get_json("/api/v2/sat/ble/discovery")
        summary["ble_active"] = bool(disc.get("active"))
        b.emit({"event": "ble_state", "uptime_s": up0, "active": disc.get("active"), "psram": disc.get("psram"),
                "ble_enable": disc.get("ble_enable"), "risk_ack": disc.get("risk_ack")})
        if not disc.get("active"):
            raise RuntimeError("BLE is not active after the reboot (bleActive() false)")

        for mark in MARKS:
            target = t_anchor + (mark - up0)
            if target < time.monotonic():
                b.emit({"event": "mark_missed", "mark_s": mark})
                continue
            time.sleep(target - time.monotonic())
            t_s = time.monotonic()
            try:
                d = b.get_json("/api/v2/device/info")["device"]
            except Exception as e:  # noqa: BLE001 - record and keep the schedule
                b.emit({"event": "sample_error", "mark_s": mark, "error": str(e)})
                continue
            rec = {"mark_s": mark, "uptime_s": round(up0 + (t_s - t_anchor), 1),
                   "internal_free": d.get("internal_free"), "internal_maxblk": d.get("internal_maxblk"),
                   "freeheap": d.get("freeheap"), "maxfreeblock": d.get("maxfreeblock")}
            samples.append(rec)
            b.emit({"event": "sample", **rec})

        disc = b.get_json("/api/v2/sat/ble/discovery")
        valid = [s for s in disc.get("sensors", []) if s.get("valid")]
        summary["roster_valid"] = len(valid)
        b.emit({"event": "roster", "populated": disc.get("populated_slots"), "valid": len(valid),
                "sensors": [{k: s.get(k) for k in ("mac", "name", "rssi", "age_ms", "valid")}
                            for s in disc.get("sensors", [])]})
        if not args.no_telnet:
            try:
                windows = telnet_ad_windows(args.host, interval)
                summary["ad_windows"] = windows
                summary["ads_seen"] = any(w["ads"] > 0 for w in windows)
                b.emit({"event": "ad_windows", "windows": windows})
            except Exception as e:  # noqa: BLE001
                b.errors.append(f"telnet: {e}")
                b.emit({"event": "telnet_error", "error": str(e)})
        summary["ble_running_proven"] = bool(summary.get("ads_seen")) or summary["roster_valid"] > 0
        if not summary["ble_running_proven"]:
            b.errors.append("BLE running not proven: no window with ads > 0 and no valid roster sensor; "
                            "the heap numbers are not comparable")
            b.emit({"event": "unproven", "ads_seen": summary.get("ads_seen"), "roster_valid": len(valid)})
        dbg = b.get_json("/api/v2/debug")["debug"]
        summary["heap_min_free_at_end"] = dbg.get("runtime.heap_min_free")
        summary["uptime_at_end_s"] = dbg.get("runtime.uptime_sec")
    except Exception as e:  # noqa: BLE001
        b.errors.append(str(e))
        b.emit({"event": "error", "error": str(e)})
    finally:
        if original is not None:
            try:
                b.set_bool("satbleenable", original["satbleenable"]["value"])
                b.set_bool("satbleriskack", original["satbleriskack"]["value"])
                time.sleep(3)
                if ble_was_active is False:
                    b.reboot_and_wait()
                now = b.settings()
                summary["restored"] = (now["satbleenable"]["value"] == original["satbleenable"]["value"]
                                       and now["satbleriskack"]["value"] == original["satbleriskack"]["value"])
            except Exception as e:  # noqa: BLE001
                b.errors.append(f"restore: {e}")
                summary["restored"] = False
            b.emit({"event": "restore", "restored": summary.get("restored")})

    for key in ("internal_free", "internal_maxblk", "freeheap", "maxfreeblock"):
        vals = [s[key] for s in samples if isinstance(s.get(key), int)]
        if vals:
            summary[f"{key}_median"] = statistics.median(vals)
            summary[f"{key}_min"] = min(vals)
    summary["samples"] = len(samples)
    summary["errors"] = b.errors
    b.emit({"summary": summary})
    b.log.close()
    return 1 if b.errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
