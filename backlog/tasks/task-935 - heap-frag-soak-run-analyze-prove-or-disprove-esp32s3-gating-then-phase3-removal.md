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
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Instrumented firmware flashed to a free ESP32-S3 (not the TASK-933 device); 'z' reset from a healthy heap at soak start
- [ ] #2 Fragmenting load driven (sat_boiler_emulator + Web UI + MQTT discovery republish) and a representative-window capture collected via capture-mqtt-debug.bat
- [ ] #3 Verdict recorded against the proof criterion (min_max_block / histogram / gating counters / max_loop_gap_ms) with the capture transcript attached
- [ ] #4 If proven: Phase-3 removal of drip/tier gating WITH evaluate.py ADR-089/121 gates + ADRs updated, rebuilt and re-soaked clean. If disproven: ESP32-S3 evidence documented on ADR-089/121, gating kept.
<!-- AC:END -->
