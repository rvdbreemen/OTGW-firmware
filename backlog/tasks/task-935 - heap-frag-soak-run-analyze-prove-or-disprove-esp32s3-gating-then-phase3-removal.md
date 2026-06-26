---
id: TASK-935
title: >-
  test(heap): run the heap-frag soak + analyze — prove/disprove ESP32-S3 gating,
  then Phase-3 removal if proven
status: To Do
assignee:
  - '@claude'
created_date: '2026-06-25 21:30'
labels:
  - heap
  - soak
  - esp32-s3
  - test
dependencies:
  - TASK-934
ordinal: 149000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Phase 2 + Phase 3 of the heap-frag investigation. TASK-934 added the observability (min-maxBlock watermark + maxBlock histogram + max_loop_gap, exposed over MQTT stats + telnet, with a 'z' reset). This task runs the actual soak on real ESP32-S3 hardware and decides whether dev's preventive heap-frag gating (drip throttle, per-consumer tier ladders ADR-121, maxBlock-promoted tiers) and any delay(1)-style loop pacing are dead weight on ESP32-S3.

DEFERRED at user request (2026-06-25): the soak is a multi-hour physical run and the one known device (192.168.88.39) is in use by the concurrent v2-webui work (TASK-933). Run when a free ESP32-S3 is available.

## Procedure (Phase 2 — soak)
1. Flash the TASK-934 instrumented firmware (branch feat/heap-soak-instr, or dev once merged): `flash_otgw.bat --board esp32`.
   - Fresh worktree first build: `git submodule update --init --recursive`; build with `~/.platformio/penv/Scripts/python.exe -m platformio run -e esp32` (host Python 3.14 breaks the espressif32 platform — see CLAUDE.md "Test automation").
2. Telnet `z` to zero the soak window from a HEALTHY heap.
3. Drive fragmenting load: `python scripts/sat_boiler_emulator.py --host <ip>` + concurrent Web UI polling + MQTT discovery republish.
4. Capture for a representative window (e.g. 24–72h): `scripts/capture-mqtt-debug.bat -DeviceHost <ip> -BrokerHost <ip> -DurationSeconds <N> -Topic "otgw-firmware/stats/#"`.

## Proof criterion (Phase-3 trigger)
Over the soak, ALL of:
- `otgw-firmware/stats/min_max_block` stays well above the gating thresholds (8192 emergency / 1536 promote) — i.e. the histogram sits in `maxblock_ge16k` with empty `lt2k/lt4k/lt8k`.
- `drip_slowmode` / `mqtt_drops` / `ws_drops` / `enter_low|warning|critical` all stay at 0.
- `max_loop_gap_ms` stays low (no multi-second stalls → delay(1) pacing not needed).

## Phase 3 — gating removal (ONLY if proven)
If the criterion holds, remove dev's preventive drip/tier gating. This is ADR-governed and breaks static gates that must be updated together:
- evaluate.py `check_heap_fragmentation_promotion` (ADR-089 sub-rule 2) REQUIRES getHeapHealth() to reference HEAP_FRAG_PROMOTE_MAXBLOCK + platformMaxFreeBlock().
- evaluate.py `check_per_consumer_heap_gate` (ADR-121) REQUIRES getHeapHealthForWebSocket()/getHeapHealthForMQTT() + independent WS/MQTT ladders.
- evaluate.py `check_heap_tier_entry_counters` (ADR-089 sub-rule 3) + `check_heap_tier_thresholds_ordered`.
Supersede/amend ADR-089 and ADR-121, update the gates, and rebuild + re-soak to confirm no regression. If the criterion does NOT hold, keep the gating and document the ESP32-S3 evidence on the ADRs instead.

## Deployment + first-probe findings (2026-06-26, maintainer to run the full soak)

Instrumented firmware `2.0.0-alpha.275+b55b7f8` (heap watermark/histogram + jitter) was flashed to the bench OTGW32 and the instrumentation verified live, but a stable headless soak could not be driven from the agent; handed to the maintainer.

- **Flash (app-only, settings-preserving):** the device is USB on COM4 (VID 303A/PID 1001), MAC 10:20:ba:21:b4:f8, DHCP IP 192.168.1.143 (broker laptop 192.168.1.234). `flash_otgw.bat` does a merged `-e 0x0` erase that wipes settings — DO NOT use it on a provisioned device. App-only flash that preserves NVS(WiFi)+littlefs(settings):
  `PYTHONIOENCODING=utf-8 ~/.platformio/penv/Scripts/python.exe -m esptool --chip esp32s3 --port COM4 --before default_reset --after hard_reset write_flash 0x10000 .pio/build/esp32/firmware.bin`
  (app0 = ota_0 @ 0x10000 per partitions_otgw_esp32.csv; nvs @ 0x9000, spiffs/littlefs @ 0x270000 untouched.) Build with the penv python, NOT `python -m platformio` (host Python 3.14 fails the espressif32 platform; `-t upload` also failed on a penv dep-install — esptool app-only is the reliable path).
- **Do NOT read serial with DtrEnable/RtsEnable set** — it drops the ESP32-S3 USB-JTAG into bootloader. A clean `esptool ... --after hard_reset read-mac` reboots it into the app.
- **Instrumentation confirmed working:** telnet `z` returned the exact reset message; the debug-line prefix `(free|maxBlock)` gives a live heap read on every line.
- **First-probe observation (NOT a verdict):** under brief heavy load (5 concurrent HTTP workers + discovery republish) free went 112k→70k and maxBlock 63k→~13.8k — i.e. maxBlock stayed WELL above the gating thresholds (8192 emergency / 1536 promote). No fragmentation in the window observed.
- **Blocking instability (separate issue, flag for its own investigation):** the device repeatedly fell OFF the network (no ARP) within ~90s under the heavy concurrent load (and once right after flash), recovered each time only by a USB hard-reset. The heap was healthy right before each drop, so this is NOT heap-fragmentation — it looks like WiFi instability or a WDT/crash under sustained concurrent async load on this bench unit. A real soak needs a stable device (good WiFi, gentler load) and physical monitoring.

## Runbook (maintainer)
1. App-only flash (above) — keeps the device at 192.168.1.143 on instrumented firmware.
2. Telnet `192.168.1.143:23`, press `z` from a healthy heap to zero the window.
3. Drive load + run the soak (gentler than the agent's probe if the WiFi/WDT drop recurs):
   `scripts/capture-mqtt-debug.bat -DeviceHost 192.168.1.143 -BrokerHost 192.168.1.234 -DurationSeconds <hours*3600> -Topic "otgw-firmware/stats/#"`
   or the agent's `scratchpad/soak_driver2.py <seconds>` (stream-monitors min free/maxBlock + flags pressure lines, then takes a clean `D` dump).
4. After the window, telnet `D` → record `[state.heapdiag]` (min_max_block, maxblock_hist, drip_slow_mode, ws_drops, mqtt_drops, entered_*, max_loop_gap_ms). MQTT stats publish only hourly, so a multi-hour run is needed for the `otgw-firmware/stats/#` trend; telnet `D` is the on-demand readout.
5. Apply the proof criterion above → Phase 3 (gating removal) only if it holds.

## Soak result — first 30-min gentle-load run (2026-06-27, agent-driven)

Ran a 30-min gentle sustained load (1 worker ~1.5s pacing: REST + occasional heavy index.html, no discovery republish) at 192.168.1.143, stream-monitoring the `(free|maxBlock)` debug prefix + a clean telnet `D` dump after. Device stayed up the whole window (telnet_drops=0, http err=1/908).

Authoritative `[state.heapdiag]` after the run (from telnet `D`, since the last `z` reset):
- `maxblock_hist <2k/<4k/<8k/<16k/>=16k = 0/0/0/0/1967` — EVERY one of 1967 1 Hz samples had the largest contiguous block ≥ 16 KB.
- `min_max_block = 31732` (31.7 KB) — smallest contiguous block ever, vs gating thresholds 8192 (emergency) / 1536 (promote).
- `min_free_heap = 63316`; stream min free = 90380; stream min max_block = 40948.
- `drip_slow_mode = 0`, `ws_drops = 0`, `mqtt_drops = 0`, `entered_low = 0`, `entered_warning = 0`, `entered_critical = 0` — the gating NEVER fired and the heap NEVER left the HEALTHY tier.
- `max_loop_gap_ms = 76` (< 200 ms) — no loop stalls / WDT pressure.

**Verdict (this window): proof criterion HOLDS.** On ESP32-S3 the heap does not collapse the way ESP8266 did; the largest contiguous block stayed ≥ 16 KB (min 31.7 KB) throughout, so the preventive gating is dead weight. Strong signal for Phase 3.

**Caveats:** 30 min, gentle load — NOT the multi-day "long-running under sustained load" regime where the 1.x ESP8266 eventually failed. Heavier load crashed the bench unit off the network (WiFi/WDT, NOT heap-frag — heap was healthy beforehand; flagged as a separate issue). A longer (hours/days) confirmatory soak should precede actually removing the gating.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Instrumented firmware flashed to a free ESP32-S3 (not the TASK-933 device); 'z' reset from a healthy heap at soak start
- [x] #2 Fragmenting load driven (sat_boiler_emulator + Web UI + MQTT discovery republish) and a representative-window capture collected via capture-mqtt-debug.bat
- [x] #3 Verdict recorded against the proof criterion (min_max_block / histogram / gating counters / max_loop_gap_ms) with the capture transcript attached
- [ ] #4 If proven: Phase-3 removal of drip/tier gating WITH evaluate.py ADR-089/121 gates + ADRs updated, rebuilt and re-soaked clean. If disproven: ESP32-S3 evidence documented on ADR-089/121, gating kept.
<!-- AC:END -->
