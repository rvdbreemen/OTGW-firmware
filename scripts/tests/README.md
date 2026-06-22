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

## Notes
- Flash arms over OTA, not USB: `curl -F "firmware=@x.ino.bin" "http://<ip>/update?cmd=0"`.
  USB serial flashing fails on a PIC-connected unit (PIC stream corrupts the bootloader).
- A boiler-less bench needs the replay (decode) + a reachable MQTT broker
  (publish churn) to reproduce the field collapse; WS+HTTP alone is not enough.
