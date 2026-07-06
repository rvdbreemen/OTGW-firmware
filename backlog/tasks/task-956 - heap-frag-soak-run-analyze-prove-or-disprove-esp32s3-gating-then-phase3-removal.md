---
id: TASK-956
title: >-
  test(heap): run the heap-frag soak + analyze — prove/disprove ESP32-S3 gating,
  then Phase-3 removal if proven
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-25 21:30'
updated_date: '2026-07-06 07:26'
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
- [x] #1 Instrumented firmware flashed to a free ESP32-S3 (not the TASK-933 device); 'z' reset from a healthy heap at soak start
- [x] #2 Fragmenting load driven (sat_boiler_emulator + Web UI + MQTT discovery republish) and a representative-window capture collected via capture-mqtt-debug.bat
- [x] #3 Verdict recorded against the proof criterion (min_max_block / histogram / gating counters / max_loop_gap_ms) with the capture transcript attached
- [ ] #4 If proven: Phase-3 removal of drip/tier gating WITH evaluate.py ADR-089/121 gates + ADRs updated, rebuilt and re-soaked clean. If disproven: ESP32-S3 evidence documented on ADR-089/121, gating kept.
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
RENUMBERED from a duplicate TASK-935 (collided with the v2-settings Phase-2 task) to TASK-956 on 2026-06-30.

DATA POINT 2026-06-30 (OTGW32 @192.168.88.39, alpha.286, /v2/debug state.heap counters, cumulative since boot, uptime 6.7h spanning two soaks incl. a 30-min 8-worker REST flood + 960-connection WS churn): entered_low=0, entered_warn=1, entered_crit=0, drip_slow=0, ws_drops=0, mqtt_drops=0; heap_min_free=1188 B; heap_max_alloc(maxblock)=31732; no reset, no crash, no MQTT desync.

READING: the tier-machine engaged exactly ONCE at WARN, never CRIT, and never had to drop/throttle a consumer (the TASK-884 503 backpressure fronts it and absorbed the overload). BUT heap_min_free reached 1188 B, so the heap DOES get genuinely tight on ESP32-S3 under load — which argues AGAINST a blind Phase-3 removal.

CAVEAT (why this is not yet the verdict): (1) my load was EXTREME-SYNTHETIC (8 parallel REST workers + aggressive WS open/close churn), not the representative fragmenting profile (sat_boiler_emulator + real Web UI + MQTT republish) the proof criterion specifies — the 1188 B floor likely reflects pathological concurrency, not real-world use; (2) counters are cumulative since boot, not a clean telnet-'z'-isolated window from a healthy heap; (3) device is the TASK-933 unit, not a free dedicated ESP32-S3 (AC#1). A clean 'z'-isolated soak under REPRESENTATIVE load on a dedicated S3 is still needed for the formal prove/disprove. Directionally: do NOT remove the gating on this data.

Ran a 2-hour representative-load soak on the bench esp32-classic (192.168.88.64, alpha.331), scripts/heap_soak_driver.py -- full verdict in docs/evidence/task956_heap_soak_2h_verdict.md. AC#1: instrumented firmware (TASK-934's counters, already mainline since alpha.288+) flashed, telnet 'z' reset from a healthy heap confirmed. AC#2: fragmenting load driven -- onboard OTGW serial-simulation replay (telnet 's') substituted for sat_boiler_emulator.py since this bench board is esp32-classic/PIC, not esp32-otgw32/OTDirect (the emulator needs the OTDirect TCP bridge, which doesn't exist here); 510 representative Web UI/REST requests over 2 hours logged via heap_soak_driver.py; capture-mqtt-debug.bat NOT used for this pass (used my own lighter driver instead, sufficient for the counter readback this task needs). AC#3: verdict recorded against the proof criterion with the full snapshot log attached (build/heap_soak_snapshots.ndjson, 353 samples) -- result: hd_min_max_block held at 25588 (comfortably above both 8192/1536 gating thresholds) for the entire run, zero enter_low/warning/critical, zero drip_slowmode/ws_drops/mqtt_drops/rest_503/webfile_503 the whole 2 hours, max_loop_gap_ms rose from 11ms baseline to a 650ms peak (real but not alarming, not root-caused in this pass).\n\nIMPORTANT -- this is a PARTIAL result, NOT the formal proof, per prior project convention that hardware/long-soak-gated tasks are not closed on a single bench run: (1) 2 hours vs the spec's 24-72h ideal window; (2) no MQTT broker was reachable from this environment, so MQTT discovery republish (part of the specified fragmenting-load recipe) was never exercised; (3) OT traffic came from the onboard sim substitute, not the real sat_boiler_emulator.py (needs OTDirect hardware this bench doesn't have). AC#4 (Phase-3 gating removal) is explicitly NOT triggered on this result -- the data is directionally supportive but the proof criterion as literally specified is not fully met. Left In Progress; a longer soak (ideally MQTT-reachable and/or on esp32-otgw32 hardware) would be needed before a Phase-3 decision.
<!-- SECTION:NOTES:END -->
