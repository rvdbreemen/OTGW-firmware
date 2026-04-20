---
id: TASK-344
title: Lower heap guard thresholds tuned on Crashevans log data
status: Done
assignee:
  - '@claude'
created_date: '2026-04-20 07:20'
updated_date: '2026-04-20 07:28'
labels:
  - mqtt
  - heap
  - 1.4.1
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Further reduce the heap-pressure thresholds to exploit the additional headroom that the TASK-338/339/340/342 burst-reduction fixes provide. Values tuned against the concrete log data from Crashevans (v1.4.0-beta+0d6942a, debug_2a.txt).

**Observed in tester log**
- Lowest heap sample: 7832 bytes
- maxFreeBlock under pressure: down to 4952 bytes
- CRITICAL (2048), WARNING (4096), FRAG_PROMOTE (2048) never reached
- LOW (6144) is the only tier actively firing throttles

**Rationale**
With 2s drip cadence + HEAP_LOW adaptive trigger + Status-burst quiesce shipping in 1.4.1, the dip-floor is expected to rise from ~7800 to 8500+ bytes. The LOW threshold should sit comfortably below that normal dip so throttling only fires on abnormal pressure (longer uptime, fragmentation, extra WS clients), not on routine bursts.

**Changes**
- HEAP_CRITICAL_THRESHOLD   2048 → 1536 (still >= 1 pbuf + margin)
- HEAP_WARNING_THRESHOLD    4096 → 3072 (still >= 2 pbuf + streaming chunk)
- HEAP_LOW_THRESHOLD        6144 → 5120 (tuned: below typical burst floor after fixes)
- HEAP_FRAG_PROMOTE_MAXBLOCK 2048 → 1536 (consistent with CRITICAL)
- MQTT_DISCOVERY_HEAP_MIN   4000 → 3000 (aligned with new WARNING)

**Not lowered further because**
- CRITICAL < 1024 leaves no headroom for even one pbuf
- WebSocket connect-gate (webSocketStuff.ino:78) uses WARNING; below 3K means accepting new WS clients with near-zero margin
- Tester base has no live telemetry; cannot roll back from a silent regression
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 HEAP_CRITICAL_THRESHOLD set to 1536 in helperStuff.ino
- [x] #2 HEAP_WARNING_THRESHOLD set to 3072 in helperStuff.ino
- [x] #3 HEAP_LOW_THRESHOLD set to 5120 in helperStuff.ino
- [x] #4 HEAP_FRAG_PROMOTE_MAXBLOCK set to 1536 in helperStuff.ino
- [x] #5 MQTT_DISCOVERY_HEAP_MIN set to 3000 in MQTTstuff.ino
- [x] #6 Comment block in MQTTstuff.ino updated to reference new WARNING value (was Keep in sync with HEAP_WARNING)
- [x] #7 Full build (firmware + littlefs) passes on esp8266
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Edit helperStuff.ino — CRITICAL, WARNING, LOW defines
2. Edit helperStuff.ino — HEAP_FRAG_PROMOTE_MAXBLOCK define
3. Edit MQTTstuff.ino — MQTT_DISCOVERY_HEAP_MIN + its sync-comment
4. Commit with descriptive title
5. Push to origin/1.4.1
6. Run full build (firmware + filesystem)
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Lowered all 5 heap-pressure thresholds on branch 1.4.1 tuned to Crashevans tester log data:

- HEAP_CRITICAL_THRESHOLD   2048 -> 1536  (helperStuff.ino:687)
- HEAP_WARNING_THRESHOLD    4096 -> 3072  (helperStuff.ino:688)
- HEAP_LOW_THRESHOLD        6144 -> 5120  (helperStuff.ino:689) — tuned value
- HEAP_FRAG_PROMOTE_MAXBLOCK 2048 -> 1536  (helperStuff.ino:724)
- MQTT_DISCOVERY_HEAP_MIN   4000 -> 3000  (MQTTstuff.ino:49)

Updated the sync comment in MQTTstuff.ino to reference the new HEAP_WARNING value (3072) instead of the stale historical 12000-bytes-for-1200-byte-pbuf rationale.

Build verified: full firmware + filesystem build on esp8266 via build.py (Python 3.12). Both artifacts produced clean:
- OTGW-firmware-1.4.1-beta+7f5fdaa.ino.bin (0.69 MB)
- OTGW-firmware.1.4.1-beta+7f5fdaa.littlefs.bin (1.98 MB)

No compile errors, no warnings, no stack size regression. Commit 7f5fdaad on origin/1.4.1.

Expected effect on tester setups: combined with the 4 burst-reduction fixes already shipping (TASK-338/339/340/342), throttle events should go from ~15 per 3 minutes observed in debug_2a.txt down to near-zero in routine operation. Only abnormal pressure (long uptime fragmentation, extra WS clients) would now trigger the LOW throttle band.
<!-- SECTION:FINAL_SUMMARY:END -->
