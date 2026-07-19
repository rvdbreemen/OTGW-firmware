---
id: TASK-1038
title: >-
  Fix: emergencyHeapRecovery is a no-op without WebSocket clients and fires far
  too late
status: To Do
assignee: []
created_date: '2026-07-19 14:59'
updated_date: '2026-07-19 15:13'
labels: []
dependencies: []
priority: high
ordinal: 157000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Evidence from martreides 1.7.1 captures (otgw-171.log, otgw-171-2.log, TASK-1037): every emergencyHeapRecovery() invocation logs 'delta=+0 actions=0x06'. Bit 0x01 (drop WebSocket clients) is never set, so no browser was connected. Of the three ADR-079 actions in helperStuff.ino:1171, action 1 does nothing without WS clients, action 2 (OTGWstream.stop()) does nothing without stream clients, and action 3 (clearMQTTConfigPending()) clears a static bitmap and returns no bytes. Recovery is therefore structurally incapable of reclaiming anything in the most common field configuration: MQTT-only, no browser open.

Second defect: the trigger is too late. OTGW-firmware.ino:403 only calls it at HEAP_CRITICAL, which is freeHeap < 1536 (helperStuff.ino:879). In both captures recovery first ran at before=888 bytes free, with maxBlock already around 500. At that point there is nothing left to work with and the crash follows within seconds.

Scope: give recovery at least one action that reclaims memory when no WS/stream clients exist, and move the trigger earlier (HEAP_WARNING or HEAP_LOW) so it acts while recovery is still possible. Consider whether the 30s EMERGENCY_RECOVERY_INTERVAL_MS rate limit is appropriate when the terminal collapse takes under 60s.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 emergencyHeapRecovery reclaims a measurable, logged number of bytes in an MQTT-only configuration with no WS or stream clients
- [ ] #2 Recovery trigger fires early enough that free heap is above HEAP_CRITICAL when the first attempt runs
- [ ] #3 Rate-limit interval justified against the observed collapse rate, with the reasoning recorded in the task
- [ ] #4 delta=+0 no longer appears in a reproduction of the martreides collapse profile
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Strict ordering. Step 2 is worthless before step 1 lands: firing a no-op earlier still reclaims nothing. Zero bytes at 03:02 is as useless as zero bytes at 03:04.

1. Make recovery actually reclaim in the MQTT-only, no-browser configuration.
   - Audit what is actually holding heap in the martreides collapse profile (lwIP pcbs and rx buffers of unserviced HTTP connections are the prime suspect, see TASK-1039).
   - Give emergencyHeapRecovery at least one action that returns bytes when hasWebSocketClients() is false and no OTGWstream clients exist.
   - Verify by log: delta must be measurably positive in a reproduction. This is the gate on the whole task.

2. Only once step 1 shows delta > 0, move the trigger earlier.
   - OTGW-firmware.ino:403 currently fires only at HEAP_CRITICAL (freeHeap < 1536). Both captures show the first attempt at before=888, far past the point of rescue.
   - Move to HEAP_WARNING (3072) or HEAP_LOW (5120). Pick based on measured recovery yield at each level, not on intuition.
   - Watch for thrash against the existing TASK-553 drip-mode hysteresis, which already uses HEAP_LOW as its entry trigger and HEAP_LOW_RESTORE_THRESHOLD (6144) to restore.

3. Re-evaluate EMERGENCY_RECOVERY_INTERVAL_MS (30000) against the observed collapse rate.
   - In otgw-171.log the run from first gate trip to crash is under 90 seconds, so a 30s rate limit allows at most two or three attempts. Record the reasoning either way.

Do NOT raise HTTP_SERVE_MIN_MAXBLOCK or MQTT_PUBLISH_MIN_MAXBLOCK as part of this task. Those gates throttle consumers rather than reclaiming memory, they are field-calibrated against the 1460-byte TCP MSS cliff, and raising them makes the TASK-1039 latch engage earlier.
<!-- SECTION:PLAN:END -->
