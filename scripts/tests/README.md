# OTGW 1.x heap-fragmentation test rig

Host-side tools to reproduce and measure the heap-fragmentation instability
(TASK-901: the 1.6.x/1.7.x random-reboot regression, bisected to the
`delay(1) -> yield()` change in `doBackgroundTasks()`, TASK-651 / commit
05e777bf).

The failure is **fragmentation, not out-of-memory**: under sustained load the
largest contiguous free block (`maxfreeblock`) collapses while total free heap
still looks fine, an allocation fails, and the device reboots. It is
**load-dependent** and only manifests with real decode + MQTT + WS + HTTP churn.

## Tools

### `heap_sampler.py`
Samples the device's heap over a window and writes a JSON verdict.
```
python heap_sampler.py --host <ip> --secs 600 --out result.json
```
Reads the telnet:23 `( free | maxBlock )` stream AND polls
`/api/v2/device/info`. Reports `maxblock_p05/median/min`, `free_min`,
exceptions, `bootcount_delta` (reboot detection). The device's own tier
counters in `/api/v2/device/info` are the cleanest discriminator:
`hd_enter_low`, `hd_enter_critical` (crossings of the protective heap tiers),
`hd_ws_drops` / `hd_mqtt_drops`.

### `overload.py`
Generates sustained load alongside the sampler.
```
python overload.py --host <ip> --minutes 20 --http-workers 8 --ws-subs 3 [--broker <mqtt-host>]
```
- WS subscribers HOLD a live-log stream on `ws://<host>:81/` (1.x caps real
  clients at 3). A recv-timeout is treated as "keep holding", so they sustain
  the firehose (`ws_reconnects` stays ~0) instead of churn-reconnecting.
- HTTP flood loops the valid 1.x `/api/v2` endpoints (no `sat`/`otdirect`).

### `otgw_simulation.log` (replay fixture)
2445 real boiler+thermostat OT frames (B/T monitor lines) extracted from a
field transcript. Upload it to the device LittleFS, then enable the firmware's
built-in replay to generate the **decode + publish workload of an attached
boiler without any boiler**:
```
# upload (no auth needed when sHTTPpasswd is empty):
curl -F "upload=@otgw_simulation.log;filename=otgw_simulation.log" http://<ip>/upload
# enable / disable replay (paced at iOTGWSimulationIntervalMs, default 750 ms):
curl -X POST http://<ip>/api/v2/simulate/start
curl -X POST http://<ip>/api/v2/simulate/stop
curl       http://<ip>/api/v2/simulate          # status
```
The replay feeds frames through `processOT()` exactly as PIC frames arrive, so
the full decode -> state -> MQTT -> WS path runs. Combine with `overload.py`
for the complete field-shaped load. (The replay flag is runtime state and
resets on reboot; re-enable after each flash. LittleFS survives app-only OTA.)

### `otgw_simulation_coverage.log` + `make_simulation_coverage.py` (decode-coverage fixture)
A second, complementary fixture. Where `otgw_simulation.log` optimises for
**sustained realistic load** (2445 frames, 21 MsgIDs), this one optimises for
**breadth**: 423 frames covering all 134 ids in `OTmap[]` plus 9 out-of-map ids,
all six OpenTherm message types, and all five source prefixes including the
`E` parity-error path. Use it to check that a firmware change did not break any
decode or publish path; use the load fixture to check stability.

Upload and run it exactly like the load fixture (same `/upload` +
`/api/v2/simulate/start` flow, same `/otgw_simulation.log` filename on device).
One loop takes about 5.3 minutes at the default 750 ms pacing.

Regenerate after `OTmap[]` gains ids:
```
python make_simulation_coverage.py            # reads OTGW-Core.h + ../../../OTGW-logs
```
The generator harvests every real frame it can find in the capture corpus (67
OTmap ids appear in real captures) and synthesises the remaining 67 that no real
boiler implements, choosing values per `ot_*` type so the f8.8, s16, u16, u8u8
and flag8 decoders all do real work. Status(0) is interleaved in two alternating
variants so the bit and byte fan-outs fire on *change* each loop, not only
first-seen.

Two things to know before hand-editing any fixture:
- `readOTGWSimulationLine()` returns **every** non-empty line to the parser. It
  does not skip comments, so a `#` header would be decoded as a frame.
- Parity is not verified ESP-side; the PIC signals it with an `E` prefix
  (`OTGW-Core.ino`). Synthetic frames therefore do not need valid parity, and an
  `E` line is the only way to reach that branch.

Measured on otgw1.local (ESP8266, 1.7.3-beta.3): one loop decoded 143 distinct
MsgIDs, published 394 distinct MQTT topics, with zero exceptions, zero discarded
lines and flat heap.

### `coverage_baseline.py` + `baseline_coverage.json` (regression gate)
The coverage fixture proves breadth, but a run on its own still has to be
eyeballed. This turns it into a pass/fail gate: a capture is reduced to a
normalized fingerprint, and that fingerprint is compared against a committed
baseline taken from a known-good firmware.

```
# 1. run the fixture (upload + start as above) and capture telnet:23 to a file
# 2. check it against the baseline
python coverage_baseline.py compare mycapture.log        # exit 1 on any drift
```

What the fingerprint keeps, because it characterises firmware behaviour:
- for each `(prefix, msgtype, msgid)`: the decoded label and rendered value
- for each MQTT topic: the **set** of payloads published
- which message types and source prefixes were exercised at all

What it strips, because it varies per run or per bench and would be pure noise:
timestamps, heap and max-block columns, task ids, uptime, device uniqueid/MAC,
broker host, topic root. Non-ASCII is dropped from values too: captures carry
cp1252 degree signs, so the same firmware would otherwise fingerprint
differently depending on how the log was decoded.

Two properties worth knowing:
- **Stability is proven, not assumed.** `python coverage_baseline.py selftest
  <capture>` fingerprints two halves of one capture and diffs them. On the
  v1.7.3-beta.3 reference capture that reports 0 unstable keys across 370 keys.
- **Compare needs at least one complete loop** (about 5.3 minutes, and prefer 2+).
  A capture cut mid-loop reports topics as MISSING simply because they had not
  come round yet. The selftest halves show this: 383 topics versus 379.

The committed baseline is from **v1.7.3-beta.3+5f852a0**, validated on
otgw1.local over 20 minutes / 3.7 loops: 370 keys, 143 distinct MsgIDs, 383 MQTT
topics, all 6 message types, all 5 source prefixes, zero exceptions, flat heap.

The raw reference capture is deliberately not committed (about 1 MB, against
27 KB for the largest other asset here). It lives outside the firmware repo at
`OTGW-logs/validation-beta3-coverage.log`. Nothing depends on it: `selftest`
works on whatever capture you produce.

Refresh the baseline **only deliberately**, when a diff has been reviewed and the
new behaviour is the intended behaviour:
```
python coverage_baseline.py record newcapture.log
```
Committing a refreshed baseline without reading the diff first defeats the point
of having one.

## Notes
- Flash arms over OTA, not USB: `curl -F "firmware=@x.ino.bin" "http://<ip>/update?cmd=0"`.
  USB serial flashing fails on a PIC-connected unit (PIC stream corrupts the bootloader).
- A boiler-less bench needs the replay (decode) + a reachable MQTT broker
  (publish churn) to reproduce the field collapse; WS+HTTP alone is not enough.
